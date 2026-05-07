# 煤矸石 X 射线智能分选机上位机软件

**开源替代版** — 基于对原 `Gangue.exe`（袁工作品）完整反向工程而重建。

原软件作者及源代码已不可得；本仓库通过对部署包内所有 DLL 符号表、反编译 C 伪代码、TwinCAT 项目文件、配置文件及运行日志的系统分析，精确复现了原有功能，并在架构上予以改进，使其可被 Git 管理、跨机器部署、持续迭代。

---

## 系统概述

### 工作原理

1. X 射线源（200 kV / 2.3 mA）向传送带上的物料发射扇形射线束
2. 线阵探测器（2180 像素，PixUnit = 1.2295 mm）逐行采集透射强度图像
3. 每 1150 行拼成一幅 2D 帧（≈2180×1150 像素，16-bit 灰度）
4. TensorRT YOLOv8-seg 模型对帧做实例分割（矸石 / 煤 / 混合，3 类）
5. 检测到矸石 → 通过 Beckhoff ADS 驱动 136 路 EL2828 电磁阀，延迟 TimerGap=2456 ms 后喷气
6. 矸石被吹离传送带至废料仓；煤继续前进

### 硬件拓扑（现场确认，先瑗矿）

```
X 射线源（COM1, 200kV/2.3mA, VJ/LN RS-232 协议）
        ↓ 射线
  传送带（皮带速度由 S7-1500 PLC 反馈, 2035 mm/s 额定）
        ↓ 透射强度
线阵探测器（GigE, 192.168.1.199, Camera Link, XLibDll SDK）
        ↓ signalAcqDataArray(uint16*, width, height, timestamp)
DetectorLib.dll → DataAcquirer → PipelineEngine
        ↓ TRT 推理（D:\best_Xray.trt, 640×640 YOLOv8-seg）
BeckHoffLib.dll → TcAdsDll.DLL → Beckhoff EtherCAT
        ↓ AdsSyncWriteReq(0x3040030, 0x81000006, 136 bytes)
136 路 EL2828 电磁阀（20 个 EL2828 模块，EtherCAT 光纤总线）
        ↓
气枪喷嘴 → 矸石被吹至废料仓

辅助：
  Beifu 控制器（AMS NetID: 2.192.168.0.102.1.1, 0x81000000, 154路光机联锁）
  S7-1500 PLC（192.168.2.100:102, DB15, 皮带速度/急停/安全门）
  ACS 运动控制器（10.0.0.100:7070, 皮带启停）
  Hikvision 线阵相机（可选融合，当前 para0_0_2=0 已禁用）
```

---

## 反向工程成果

本项目通过以下手段完整重建了原软件的技术细节：

| 来源 | 获得的关键信息 |
|---|---|
| `config.xml` 实测值 | indexGroup=0x3040030, indexOffsetWrite=0x81000006, TimerGap=2456, QNum=136 |
| `BeckHoff.cpp` 源代码 | Beifu 控制器写地址 = 0x81000000（非 0x81000006）|
| `TwinCAT Project1.tsproj` | EtherCAT 完整拓扑：154 路输出，20×EL2828，双光纤耦合器 |
| `DetectorLib.dll.c` 反编译 | 完整 7 步探测器初始化序列；`create("DetectorLib")` 参数确认 |
| `Qt_OpenCV_Image_Processing.dll.c` 反编译 | TimerGap 为固定值（非动态公式）；`_IO_Output` 为检测几何描述符 |
| `XRayLib.dll.c` 反编译 | CP 命令为 5 位 µA；MON 响应位置（mA 偏移 6-10，温度 12-15）|
| `PLCControlLib.dll.c` 反编译 | serialport 参数为型号字符串"S7-1500"；INT16 数组索引 = 字节偏移/2 |
| `ThreadManager.dll.c` 反编译 | `create("DetectorLib")` 确认；阀门逻辑在 Gangue.exe 主进程，非 DLL |
| `ValvePlateLib.dll.c` 反编译 | 串口阀板备用方案（UseComValve=1），当前机器未启用 |
| `gangue_sys.log` 运行日志 | 心跳格式：begin/is not Busy/finish/finishfinish；安装路径 E:\Gangue |
| `xlog.dat` X 射线通信日志 | VJ RS-232 命令帧格式（STX+命令+CR）|

---

## 目录结构

```
coal-gangue-sorter/
├── core/                       纯 C++17 算法核心
│   ├── Config/                 config.xml 解析（tinyxml2），支持所有原始字段
│   ├── Classifier/             X 射线阈值分类器（TRT 推理回退用）
│   ├── Fusion/                 双传感器融合策略（XRayOnly / CameraOnly / AndReject 等）
│   ├── Inference/              AI 推理引擎层
│   │   ├── IInferenceEngine.*  统一接口 + 工厂（TRT 优先，ONNX 回退）
│   │   ├── FrameAccumulator.*  探测器行扫→2D 帧积累（1150 行→完整帧）
│   │   ├── OnnxInferenceEngine.*  ONNX Runtime 实现（跨平台开发用）
│   │   └── TrtInferenceEngine.*   TensorRT 实现（Windows 生产用）
│   ├── Logging/                spdlog 封装（gangue_sys.log，2 Hz 心跳格式匹配原版）
│   ├── NozzleMapping/          像素→喷嘴映射（XCCR 多项式，136 路）
│   ├── Pipeline/               PipelineEngine 主循环
│   ├── Recording/              帧录制（raw.tif + _IO_Mat.pgm）
│   └── Timing/                 时序计算（TimerGap 固定值，反编译确认）
├── hardware/                   硬件抽象层
│   ├── DetectorInterface/
│   │   ├── DetectorDetectorLib.*  ★ DetectorLib.dll 适配（首选，7步初始化序列）
│   │   └── DetectorAurora.*       Aurora SDK 直接适配（备用）
│   ├── XRayInterface/
│   │   ├── XRayLibAdapter.*    ★ XRayLib.dll 适配（首选，原版 DLL）
│   │   ├── XRaySerial.*        VJ RS-232 协议重实现（备用）
│   │   └── BeiduAdapter.*      Beifu ADS 联锁适配（0x81000000 确认）
│   ├── ValveDriverInterface/
│   │   ├── ValveDriverBeckHoffLib.*  ★ BeckHoffLib.dll 适配（首选）
│   │   └── ValveDriverEL2828.*       开源 AdsLib 直接适配（备用）
│   ├── PLCInterface/
│   │   ├── PLCControlLibAdapter.*  ★ PLCControlLib.dll 适配（首选）
│   │   ├── PLCS7.*                 snap7 直接适配（备用）
│   │   └── ACSMotionClient.*       ACS TCP 皮带运动控制
│   └── CameraInterface/
│       └── CameraHikGigE.*         海康威视 GigE 相机（融合模式备用）
├── app/
│   ├── main_console.cpp        无界面命令行
│   └── gui/
│       ├── main_gui.cpp        GUI 入口（含预热流程、Beifu/ACS 初始化）
│       ├── MainWindow.*        主窗口（菜单动态解析 menu.xml，匹配原版）
│       ├── PreheatDialog.*     X 射线预热对话框（停机时间→预热时长表）
│       ├── XRaySettingsDialog.* 光机面板（实时故障指示灯，2Hz 轮询）
│       ├── DetectorSettingsDialog.* 探测器面板（暗场/亮场/相机校正）
│       ├── PlcSettingsDialog.*  PLC 面板
│       ├── LogCenter.*         日志中心（Ctrl+M，spdlog 回调接入）
│       └── style.qss           ★ 原版 Qt 样式表（从部署包直接提取）
├── third_party/
│   ├── detectorlib/            DetectorLib.lib + 重建头文件
│   ├── xraylib/                XRayLib.lib + 重建头文件
│   ├── beckofflib/             BeckHoffLib.lib + 重建头文件
│   ├── plccontrollib/          PLCControlLib.lib + 重建头文件
│   └── qtopencv/               Qt_OpenCV_Image_Processing.lib + 重建头文件
├── config/
│   ├── config.example.xml      配置模板（所有真实现场值已预填）
│   ├── calibrateImageXray.tif  ★ 探测器暗/亮场校正文件（从部署包提取）
│   ├── T_Linea_C4096-7um_Internal.ccf  ★ 海康相机配置（内触发，从部署包提取）
│   └── T_Linea_C4096-7um_External.ccf  ★ 海康相机配置（外触发，从部署包提取）
├── images/
│   └── mode/
│       └── mode.txt            ★ 探测器平场校正模型（2180行，gain/offset对，从部署包提取）
├── tests/
│   ├── test_frame.tif          ★ 真实 X 射线帧（从先瑗矿机器部署包提取）
│   ├── test_gpu_frame.tif      ★ GPU 测速用真实帧
│   ├── test_pipeline_engine.cpp
│   └── test_config_loader.cpp
└── tools/
    ├── train_xray.py           ★ YOLOv8-seg 训练脚本（重建，640/1280px，3类）
    ├── collect_training_data.py ★ 从机器 data/ 目录自动生成训练集
    ├── dataset.yaml            ★ YOLOv8 数据集配置（类别：矸石/煤/混合）
    ├── seg_thousand.py         批量阈值分割生成标注
    ├── calibrate_belt.py       皮带速度标定
    └── nozzle_self_test.py     136 路喷嘴通道自检
```

---

## 快速开始

### macOS 开发环境（Mock 模式）

```bash
brew install cmake git qt@5
cd coal-gangue-sorter

cmake -B build \
  -DCGS_BUILD_GUI=ON \
  -DCGS_BUILD_CONSOLE=ON \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)"
cmake --build build -j$(sysctl -n hw.logicalcpu)

./build/coal_gangue_sorter_gui --mock
./build/coal_gangue_sorter_console --mock --seconds 5
```

### Windows 生产构建（现场工控机）

```bat
cmake -B build ^
  -DCGS_BUILD_GUI=ON ^
  -DCGS_BUILD_CONSOLE=ON ^
  -DCGS_HAS_SERIAL=ON ^
  -DCGS_HAS_DETECTORLIB=ON ^
  -DCGS_HAS_XRAYLIB=ON ^
  -DCGS_HAS_BECKOFFLIB=ON ^
  -DCGS_HAS_PLCCONTROLLIB=ON ^
  -DCMAKE_PREFIX_PATH="D:\Qt\Qt5.14.2\5.14.2\msvc2017_64"

cmake --build build --config Release -j4

rem 从 E:\Gangue\ 复制运行时 DLL 到 build\Release\
copy E:\Gangue\DetectorLib.dll  build\Release\
copy E:\Gangue\XRayLib.dll      build\Release\
copy E:\Gangue\BeckHoffLib.dll  build\Release\
copy E:\Gangue\PLCControlLib.dll build\Release\
copy E:\Gangue\XLibDll.dll      build\Release\
copy E:\Gangue\Qt5*.dll         build\Release\
```

### 离线推理测试（无硬件）

```bat
rem 用真实 X 射线帧测试 TRT 推理管线
build\Release\cgs_test_offline --tif tests\test_frame.tif --model D:\best_Xray.trt
```

---

## CMake 编译选项

| 选项 | 说明 | 生产推荐 |
|---|---|---|
| `CGS_HAS_DETECTORLIB=ON` | DetectorLib.dll 适配（首选探测器驱动）| ✅ |
| `CGS_HAS_XRAYLIB=ON` | XRayLib.dll 适配（首选 X 射线驱动）| ✅ |
| `CGS_HAS_BECKOFFLIB=ON` | BeckHoffLib.dll 适配（首选阀门驱动）| ✅ |
| `CGS_HAS_PLCCONTROLLIB=ON` | PLCControlLib.dll 适配（首选 PLC 驱动）| ✅ |
| `CGS_HAS_SERIAL=ON` | 串口支持（XRaySerial 备用驱动）| ✅ |
| `CGS_HAS_BECKHOFF_ADS=ON` | 开源 AdsLib（ValveDriverEL2828 备用）| 可选 |
| `CGS_HAS_SNAP7=ON` | snap7（PLCS7 备用，S7-1500）| 可选 |
| `CGS_HAS_TENSORRT=ON` | TensorRT 推理（需 NVIDIA GPU）| ✅ 生产 |
| `CGS_HAS_ONNX=ON` | ONNX Runtime 推理（跨平台开发）| 开发用 |
| `CGS_HAS_HIKVISION=ON` | 海康相机（融合模式，当前禁用）| 可选 |

---

## 关键硬件参数（先瑗矿现场确认值）

| 参数 | 值 | 来源 |
|---|---|---|
| X 射线电压 | 200 kV | config.xml Voltage |
| X 射线电流 | 2.3 mA（=2300 µA）| config.xml Current |
| X 射线串口 | COM1, 9600-8N1 | xlog.dat + XRayLib 反编译 |
| CP 命令格式 | 5位零填充 µA（"02300"）| XRayLib.dll.c 反编译确认 |
| 探测器 IP | 192.168.1.199 | config.xml |
| 探测器像素数 | 2180（DNum=17模块×128px）| config.xml |
| 积分时间 | 540 µs | config.xml intTime |
| 每帧行数 | 1150 | config.xml LineNumber |
| 喷嘴数量 | 136（QNum）| config.xml |
| 有效像素范围 | QStart=11 ~ QEnd=1066 | config.xml |
| 每喷嘴像素数 | 7.757 px | (1066-11)/136 |
| ADS 索引组 | 0x3040030 | config.xml / BeckHoff.cpp |
| 阀门写偏移 | 0x81000006（主喷嘴排）| config.xml 确认 |
| Beifu 写偏移 | 0x81000000（光机联锁）| BeckHoff.cpp 源码确认 |
| **TimerGap（喷吹延迟）** | **2456 ms** | config.xml + Qt_OpenCV反编译 **[固定值，非动态公式]** |
| 皮带名义速度 | 2035 mm/s | config.xml Speed |
| PLC IP | 192.168.2.100:102 | config.xml |
| PLC 型号字符串 | "S7-1500"（PLCControlLib第3参数）| PLCControlLib.dll.c 反编译确认 |
| 皮带速度 DB | DB15，字节偏移 146，INT16 | PLCState 面板 + 反编译 |
| 皮带速度数组索引 | 73（= 146/2，INT16 数组）| PLCControlLib.dll.c 反编译确认 |
| AI 模型 | YOLOv8-seg，640px 输入，3类 | model 命名 + 反编译 |
| 模型路径 | D:\best_Xray.trt | 现场截图 D:\ 目录 |
| 软件安装路径 | E:\Gangue | gangue_sys.log 确认 |

---

## AI 训练工作流

原版采用 YOLOv8 实例分割模型（`best_seg.onnx` → `best_Xray.trt`），通过机器采集的 X 射线帧迭代训练：

```bash
# 1. 从机器运行数据生成训练集（需 MustSave=1 运行过一段时间）
python tools/collect_training_data.py \
  --data D:/data \
  --out D:/data/xray_dataset

# 2. 用 labelImg 审查并修正自动标注（每1000张约30分钟）

# 3. 训练
python tools/train_xray.py \
  --data D:/data/xray_dataset/dataset.yaml \
  --imgsz 640 --epochs 200

# 4. 导出 → TRT（在目标 GPU 机器上执行）
trtexec --onnx=best_seg.onnx --saveEngine=best_Xray.trt
trtexec --onnx=best_seg.onnx --saveEngine=best_Xray_fp16.trt --fp16

# 5. 复制到 D:\ 并更新 config.xml trtPath
```

类别定义（`DP_class_num=3`, `Classes="1,0,0"`）：
- 类别 0：矸石（触发喷吹）
- 类别 1：煤（通过）
- 类别 2：混合/夹矸（通过，可通过 ClassesAlter 调整）

---

## 现场上线流程

### 无硬件阶段（当前可完成）
- [ ] Windows 编译（`cmake` + 复制 DLL）
- [ ] `--mock` 模式 GUI 验证（面板、日志、预热对话框）
- [ ] `cgs_test_offline` 用 `tests/test_frame.tif` 验证 TRT 推理
- [ ] 工业相机连接测试（`CameraHikGigE` 适配器，.ccf 文件已就位）

### 需现场停机（一次性，约半天）
- [ ] `cgs_test_bf` 验证 136 路喷嘴全部响应
- [ ] 确认探测器连接（日志出现 `ip:xxx,cmdport:yyy,imgport:zzz`）
- [ ] 确认皮带速度读数单位（对比转速表实测值）
- [ ] X 射线预热 + kV/mA 确认
- [ ] 暗场校正 + 亮场校正（更新 mode.txt）
- [ ] 首次带料分选 + 矸石率核对

---

## 已知剩余不确定项

| 项目 | 状态 | 解决方式 |
|---|---|---|
| `slotDet_init` 第2参数含义 | 当前传 0，意义未明 | 现场观察日志，若失败尝试 1150 |
| 皮带速度单位（mm/s? 或其他）| 当前假设 raw/1000 = m/s | 现场与转速表对照 |
| ACS 皮带控制轴号 | 当前假设轴 0 | 查看 OpenClaw 项目配置文件 |
| Beifu 控制器功能细节 | 地址已确认，具体功能未知 | 现场测试 XRay_enable 是否依赖 |

---

## 许可

内部项目使用。  
本仓库不包含原 `Gangue.exe` 的任何二进制文件；所有代码均独立重写。  
第三方 DLL（DetectorLib、XRayLib 等）属于原机器制造方的知识产权，本仓库仅包含其 `.lib` 导入库和从符号表重建的头文件。
