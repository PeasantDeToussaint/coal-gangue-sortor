# Detection Technology Aurora SDK 接入说明（已破解）

> **来源**：旧项目 `include/DetInclude/`（47 个头文件，完整 X-LIB SDK）+ `DetectorLib/DetectorLib.cpp`（完整使用示例）。  
> **官方文档**：`DS0000087 - Programmer's Manual of X-LIB Software Library`（公开下载）  
> **状态**：✅ SDK 头文件 + 用法范例已提取，新仓库 `DetectorAurora` 真实驱动直接基于此实现

## 已确认的探测器规格

| 参数 | 值 | 来源 |
|---|---|---|
| 像素阵列 | **17 块 X-Card DA21506414C** | 合同 00001861-15 |
| 像素总数 | **2180 px**（17×128） | 旧代码 `fwrite(..., 512*2180, fp)` |
| 像素深度 | **16 bit** | `unsigned short` 保存 |
| 控制器 | X-GCU GT × 1 | 合同 |
| 物理层 | **GigE 千兆以太网（UDP）** | XSystem 用 IP/UDP 广播发现设备 |
| 触发线 | 15m 外触发（光电信号接 GCU） | 合同 |
| 默认行数 | 512 | 旧代码 `m_lineNumber = 512` |

## SDK 核心类（来自旧项目 include/DetInclude/）

| 类 | 头文件 | 职责 |
|---|---|---|
| `XSystem` | `xsystem.h` | 网络配置 / 设备发现 / 选取 XDevice |
| `XCommand` | `xcommand.h` | 参数读写（kV、积分时间、增益等 75 个参数） |
| `XAcquisition` | `xacquisition.h` | 触发图像采集（连续/快照） |
| `XFrameTransfer` | `xframe_transfer.h` | 接收图像数据流 |
| `XOffCorrect` | `xoff_correct.h` | 暗场 / 亮场校正 |
| `XGigFactory` | `xgig_factory.h` | 创建网络对象 |
| `XImage` | `ximage.h` | 帧数据封装（`_data_` / `_width` / `_height`） |
| `XDevice` | `xdevice.h` | 设备元信息（IP、端口、卡型） |
| `IXCmdSink` | `ixcmd_sink.h` | 命令通道事件回调（错误、状态） |
| `IXImgSink` | `iximg_sink.h` | 图像通道事件回调（**OnFrameReady**） |

## 关键参数枚举（`XPARA_*`，共 75 个）

`xcommand.h` 中定义：

| 常用 XPARA | 含义 |
|---|---|
| `XPARA_INT_TIME` | 积分时间（μs，影响行频） |
| `XPARA_DM_GAIN` | 探测器增益（low + high 两字节） |
| `XPARA_EN_SCAN` | 使能扫描 |
| `XPARA_BASE_LINE` | 基线值 |
| `XPARA_EN_OFFSET_CORRECT` | 使能暗场校正 |
| `XPARA_EN_GAIN_CORRECT` | 使能亮场校正 |
| `XPARA_EN_BASELINE_CORRECT` | 使能基线校正 |
| `XPARA_LINE_TRIGGER_MODE` | 行触发模式 |
| `XPARA_EN_LINE_TRIGGER` | 使能行触发 |
| `XPARA_FRAME_TRIGGER_MODE` | 帧触发模式 |
| `XPARA_EN_FRAME_TRIGGER` | 使能帧触发 |
| `XPARA_PIXEL_NUMBER` | 像素数 |
| `XPARA_GCU_HEALTH` | GCU 健康（温湿度、电压） |
| `XPARA_DM_HEALTH` | DM（探测器模块）健康 |
| `XPARA_CH_NUM` | 通道数（多卡总数） |

完整 75 个详见 `XPARA_CODE + 1..75`。

## 典型流程（旧代码 `DetectorLib::slotDet_init/_connect/_acquire`）

```cpp
// 1. 设备发现
XSystem sys;
sys.SetLocalIP("192.168.1.100");        // 工控机本地 IP
sys.Open();
int n = sys.FindDevice();                // 广播搜索 X-GCU
XDevice* dev = sys.GetDevice(0);         // 取第一台

// 2. 注册回调
class MySink : public IXImgSink {
    void OnFrameReady(XImage* img) override {
        // img->_data_ : uint16* 长度 _width × _height
        // img->_width  : 2180
        // img->_height : 行数
    }
};
MySink sink;
xacquisition.RegisterEventSink(&sink);

// 3. 命令通道
XCommand cmd;
cmd.SetFactory(&factory);
cmd.Open(dev);
cmd.SetPara(XPARA_INT_TIME, 800);        // 800 μs 积分
cmd.SetPara(XPARA_EN_SCAN, 1);

// 4. 采集
xtransfer.SetLineNum(512);
xacquisition.RegisterFrameTransfer(&xtransfer);
xacquisition.EnableLineInfo(1);          // 行尾加 8 字节信息头
xacquisition.Open(dev, &cmd);
xacquisition.Grab(0);                    // 0 = 连续采集

// 5. 校正（启动时一次性）
xoff.CalculatePara(1, &xacquisition, &xtransfer, 0);    // 暗场
xoff.CalculatePara(0, &xacquisition, &xtransfer, 50000); // 亮场
xoff.SaveFile("./mode/mode_up.txt");
// 运行时：
xoff.LoadFile("./mode/mode_up.txt");
xoff.DoCorrect(image);
```

## 帧数据布局（已确认）

```
XImage 内存布局：
  _data_  : uint16_t*  指向连续内存
  _width  : 2180       (17 块 × 128 像素)
  _height : 512        (默认每帧行数，可改)
  
  每行字节数 = 2180 × 2 = 4360 bytes
  每帧字节数 = 4360 × 512 = 2.23 MB
  
  若启用 EnableLineInfo(1)，每行尾部多 8 字节同步信息
```

## 错误事件回调

```cpp
class CmdSink : public IXCmdSink {
    void OnXError(uint32_t err_id, const char* msg) {
        // err_id 见 SDK
    }
    void OnXEvent(uint32_t event_id, float data) {
        switch (event_id) {
            case 56: temperature = data; break;
            case 57: humidity = data; break;
        }
    }
};
```

## SDK 文件来源

旧项目 `include/DetInclude/` 47 个头文件已包含全部 SDK 公共 API，可直接复制到新仓库 `third_party/aurora-sdk/include/`（注意版权，不入 Git）。

库文件需向供应商索取：
- Windows：`xlib.dll` + `xlib.lib`（即旧项目的 `XLibDll.lib`）
- Linux：`libxlib.so`

供应商联系方式：
- 国内代理：地太科特电子制造（北京），+86 10 6783 2601
- 原厂：Detection Technology Inc.（芬兰），www.deetee.com

## 在新仓库中的实现

`hardware/DetectorInterface/DetectorAurora.h/.cpp` 实现 `IDetector` 接口：

```cpp
class DetectorAurora : public IDetector {
    XSystem _sys;
    XCommand _cmd;
    XAcquisition _acq;
    XFrameTransfer _xfer;
    XOffCorrect _off;
    XGigFactory _factory;
    
    bool open(const DetectorConfig& cfg) override {
        _sys.SetLocalIP(cfg.localIp.c_str());
        if (!_sys.Open()) return false;
        if (_sys.FindDevice() <= 0) return false;
        // ... 套用旧代码 slotDet_connect 逻辑
    }
    
    void OnFrameReady(XImage* img) {
        DetectorFrame f;
        f.frameId = ++_frameId;
        f.width = img->_width;
        f.height = img->_height;
        f.data.assign(img->_data_, img->_data_ + img->_width * img->_height);
        if (_cb) _cb(f);
    }
};
```

仅当 `CGS_HAS_AURORA_SDK=ON` 时编译；否则 `createDetector()` 工厂回退到 `DetectorMock`。

## 测试策略

无硬件无 SDK 环境：用 `DetectorMock` 跑全流程。  
有 SDK 无硬件：把 `XSystem::FindDevice` 包成 mock 返回模拟数据。  
有硬件：现场调试，先跑 SDK 自带 demo，再切到我们的应用。
