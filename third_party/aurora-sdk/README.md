# Detection Technology Aurora SDK (X-LIB)

> **来源**：从旧项目「金万利测厚机」`~/Thickness Measure_kenya hebei jinwanli/` 的 `include/DetInclude/` 与 `lib/release/XLibDll.lib`、`bin/release/XLibDll.dll` 同步到本目录（头文件已入库；Windows 二进制不入 Git，见仓库根 `.gitignore`）。  
> **供应商**：Detection Technology Inc.（芬兰），中国代理：地太科特电子制造（北京）有限公司  
> **联系**：+86 10 6783 2601  
> **文档编号**：DS0000087 "Programmer's Manual of X-LIB Software Library"

## 文件说明

```
aurora-sdk/
├── include/          49 个公共 API 头文件（Detection Technology 版权）
│   ├── xsystem.h         设备发现与网络配置
│   ├── xcommand.h        参数读写（75 个 XPARA_* 常量）
│   ├── xacquisition.h    图像采集控制（Grab/Snap/Stop）
│   ├── xframe_transfer.h 帧数据接收
│   ├── xoff_correct.h    暗场/亮场校正
│   ├── ximage.h          帧数据封装（_data_/_width_/_height_）
│   └── ...（其余 43 个辅助头文件）
└── lib/
    ├── XLibDll.dll       运行时 DLL（Windows x64 Release）
    ├── XLibDll.lib       链接导入库（Windows Release）
    └── debug/
        ├── XLibDll.dll   Debug 版本
        └── XLibDll.lib
```

## 使用

```bash
cmake -B build \
  -DCGS_HAS_AURORA_SDK=ON \
  -DCGS_AURORA_SDK_DIR=third_party/aurora-sdk
```

## 已知信息

- 通信层：**GigE 千兆以太网（UDP），设备发现用广播**
- 探测器：17 块 X-Card DA21506414C，每块 128 px，总宽 2180 px，16 bit
- 控制器：X-GCU GT（连接工控机 GigE 口）
- 触发：外部触发线（15m）从 PLC EL2828 接入 GCU
- 接口参考：`../../hardware/DetectorInterface/DetectorAurora.h`

## 版权说明

头文件和 DLL 版权归 Detection Technology Inc. 所有，不得二次发布。  
本仓库为内部工程使用，不对外公开。
