# 煤矸石智能分选机上位机软件

基于 **X 射线线阵探测器 + 工业相机**双传感器的煤矸石分选机工控软件，采用 C++17 编写，支持 Qt 6 图形界面、Beckhoff TwinCAT（PLC + EL2828 喷嘴直驱）等工业硬件，并通过完整 Mock 体系实现无硬件开发与测试。

---

## 项目状态

| 阶段 | 状态 | 内容 |
|------|------|------|
| Phase 0 | ✅ | 资料盘点 + I/O 点表 + 现场勘查清单 |
| Phase 1 | ✅ | 仓库骨架 + 5 个抽象接口 + 5 个 Mock 实现 |
| Phase 2 | ✅ | 核心算法（分类 / 时序 / 喷嘴映射 / 传感器融合）+ 29 个单元测试 |
| Phase 3 | ✅ | 气枪驱动（Beckhoff EL2828 经 ADS 直驱，TwinCAT GVL BOOL） |
| Phase 4 | ✅ | X 射线源 RS232 驱动 + Detection Technology Aurora SDK 适配器 |
| Phase 5 | ✅ | Beckhoff TwinCAT 3 ADS 适配器 + PLC 变量命名约定 |
| Phase 6 | ✅ | Qt 6 GUI（X 射线瀑布图 + 64 路喷嘴矩阵 + 实时调参面板 + 统计面板） |
| Phase 7 | ✅ | Python 离线工具（皮带标定 / 时序仿真 / 数据回放 / 喷嘴自检） |
| Phase 8 | ✅ | 硬件搭建手册 + 现场安全规程 |

**软件层全部完成**；主要机侧协议与驱动已在仓库实现（**VJ X 射线 RS232**、**Aurora 探测器 SDK 适配**、**Beckhoff ADS + EL2828 阀驱动**、**PLCBeckhoff**）。上线前剩余多为**现场项**：几何与皮带速度标定（Q8）、DF8 阀响应实测（Q9）、变频器/寄存器形态确认（Q6）、Aurora 库文件与 ADS 路由部署、工业相机与光源到货后的集成（Q3/Q4）。

---

## 系统架构

```
[光电触发] → [PLC] ──┬──→ [X 射线线阵探测器] ──┐
                      └──→ [凌云光工业相机]  ────┴──→ [上位机分类引擎]
                                                            ↓
                                                [喷嘴映射 + 时序计算]
                                                            ↓
                    PC ──[Beckhoff ADS]──▶ [TwinCAT / EL2828] ──▶ [DF8 电磁阀 ×64] ──▶ [气枪喷嘴 ×52]
                    PLC ──▶ [三色灯 / 变频 / 急停等通用 I/O]
                                                            ↓
                                                [矸石被吹离 → 废料皮带]
```

### 运行时数据流

1. **探测器帧** — `IDetector` 回调交付 16-bit 线扫原始行（`DetectorFrame`），携带纳秒时间戳。
2. **X 射线分类** — `Classifier::classifyXRayRow` 对每行做阈值分割，`segment()` 合并连续像素段，过滤噪声短段，输出 `ClassifiedSegment` 列表（`Empty` / `Coal` / `Gangue` / `Unknown`）。
3. **传感器融合** — `FusionPolicy::combine(xray, camera)` 按选定的融合模式合并双路结论（当前主循环 X 射线单轨，相机融合接口已就绪）。
4. **喷嘴映射** — `NozzleMapper::nozzlesForRange` 将像素范围映射到对应的喷嘴通道（含两侧扩边）。
5. **时序计算** — `TimingCalculator` 根据皮带速度、传感器到喷嘴距离、阀门与气动延迟，计算精确的开阀时刻（绝对纳秒时间戳）和持续时长。
6. **调度执行** — 若系统已 Arm，`IValveDriver::schedule` 接收 `NozzleCommand` 批次并驱动气枪。
7. **UI 快照** — `PipelineFrameSnapshot` 同步推送到 GUI（瀑布图 + 64 路矩阵实时显示）。

---

## 目录结构

```
coal-gangue-sorter/
├── core/                   纯 C++17 算法核心（无 Qt 依赖）
│   ├── Config/             config.xml 解析（tinyxml2）
│   ├── Classifier/         X 射线 / 相机分类器，像素段合并
│   ├── Fusion/             双传感器融合策略
│   ├── NozzleMapping/      像素 → 喷嘴通道映射
│   ├── Timing/             皮带时序与阀门延迟计算
│   ├── Pipeline/           PipelineEngine 主循环编排
│   └── Serial/             POSIX/Win32 串口基础层
├── hardware/               硬件抽象层
│   ├── IDetector.*         线阵探测器接口
│   ├── ICamera.*           工业相机接口
│   ├── IXRaySource.*       X 射线源接口
│   ├── IValveDriver.*      气枪阀门驱动接口
│   ├── IPLC.*              PLC 接口（皮带速度、急停、指示灯）
│   ├── *Mock.*             五路全功能 Mock 实现
│   ├── DetectorAurora.*    Detection Technology Aurora SDK 适配（可选）
│   ├── XRaySerial.*        VJ X 射线源 RS232 驱动（可选）
│   ├── PLCBeckhoff.*       Beckhoff TwinCAT 3 ADS 适配（可选）
│   └── ValveDriverEL2828.* Beckhoff EL2828 直驱适配（可选）
├── app/
│   ├── main_console.cpp    无界面命令行运行器
│   └── gui/                Qt 6 图形界面
│       ├── MainWindow.*    主窗口（Arm/Disarm/急停工具栏）
│       ├── ConfigPanel.*   实时参数调节面板
│       ├── XRayWaterfallView.*  X 射线瀑布图控件
│       ├── NozzleMatrixView.*   64 路喷嘴矩阵控件
│       └── StatsPanel.*    吞吐量 / 误判率统计面板
├── tests/                  单元测试（自定义 TEST_CASE / EXPECT_* 框架）
│   ├── test_classifier.cpp
│   ├── test_nozzle_mapper.cpp
│   ├── test_timing.cpp
│   ├── test_fusion.cpp
│   ├── test_xray_serial_format.cpp
│   └── test_pipeline_engine.cpp
├── tools/                  Python 离线工具
│   ├── calibrate_belt.py   皮带速度标定
│   ├── timing_simulator.py 时序参数仿真
│   ├── replay_session.py   历史帧数据回放
│   └── nozzle_self_test.py 喷嘴通道自检
├── config/
│   └── config.example.xml  配置模板（复制为 config.xml 后现场调参）
├── docs/                   详细文档
│   ├── architecture.md     软件分层与设计决策
│   ├── gui-build.md        Qt 编译与界面说明
│   ├── learning-path.md    新人 4 周成长路线
│   └── hardware/           硬件手册（物料 / I/O 点表 / 安全 / 协议）
└── third_party/
    └── aurora-sdk/         Detection Technology SDK 占位（库文件不入库）
```

---

## 快速开始（无硬件 Mock 模式）

### 依赖

| 依赖 | 最低版本 | 说明 |
|------|----------|------|
| CMake | 3.16 | 构建系统 |
| C++ 编译器 | GCC 10 / Clang 12 / MSVC 2019 | 需支持 C++17 |
| Qt | 5.12 或 Qt 6.x | 仅 GUI 目标需要 |
| Python | 3.10 | 仅离线工具需要 |

### 编译与运行

```bash
# 克隆仓库
git clone <repo-url>
cd coal-gangue-sorter

# 配置（Mock + GUI）
cmake -B build \
      -DCGS_BUILD_GUI=ON \
      -DCGS_BUILD_TESTS=ON \
      -DCGS_BUILD_CONSOLE=ON

# 编译
cmake --build build -j$(nproc)

# 准备配置（仓库根目录执行；也可设置环境变量 CGS_CONFIG_PATH 指向任意路径）
cp config/config.example.xml config/config.xml

# 运行 GUI（默认读取 config/config.xml；无文件时回退内置 Mock 参数）
./build/app/coal_gangue_sorter_gui

# 运行命令行（运行 10 秒后退出）
./build/app/coal_gangue_sorter_console --seconds 10

# 指定配置文件
./build/app/coal_gangue_sorter_console --config /path/to/config.xml

# 执行单元测试
cd build && ctest --output-on-failure
```

Mock 模式下可以观察到：X 射线 / 相机伪数据流、64 路喷嘴矩阵实时闪烁、PLC 状态轮询。

### 启用真实硬件（可选 CMake 选项）

| CMake 选项 | 说明 |
|------------|------|
| `CGS_HAS_SERIAL=ON` | 启用 POSIX/Win32 串口（VJ X 射线 RS232） |
| `CGS_HAS_AURORA_SDK=ON` | 启用 Detection Technology Aurora 探测器 SDK |
| `CGS_HAS_BECKHOFF_ADS=ON` | 启用 Beckhoff TwinCAT 3 ADS（PLC + EL2828 阀门） |

若已在本机 `third_party/ads/` 克隆 [Beckhoff/ADS](https://github.com/Beckhoff/ADS)，CMake 会自动把 `CGS_BECKHOFF_ADS_DIR` 指到该目录（仍可用 `-DCGS_HAS_BECKHOFF_ADS=ON` 打开编译）。

**Aurora（Windows）**：将旧测厚项目 `Thickness Measure_kenya hebei jinwanli/lib/release/XLibDll.lib` 与 `bin/release/XLibDll.dll` 复制到 `third_party/aurora-sdk/lib/`（该目录下二进制已被 `.gitignore` 忽略，不入库）。在 **macOS / Linux** 上需厂商提供的 `libxlib.so` 才能链接 `CGS_HAS_AURORA_SDK=ON`。

---

## 核心算法说明

### 分类器（`core/Classifier/`）

X 射线分类基于 16-bit 像素强度阈值（值越大表示穿透越强，即越空）：

| 条件 | 判定 |
|------|------|
| `pixel ≥ xrayEmptyMin`（默认 50000） | 空（无物料） |
| `pixel ≥ xrayCoalMin`（默认 25000） | 煤 |
| `pixel < xrayGangueMax`（默认 25000） | 矸石 |
| 其余 | 未知 |

相机分类基于 ROI 列均值（8-bit 灰度）：颜色深（低亮度）倾向于矸石，高亮度为空。

`segment()` 对分类结果做游程合并，并过滤宽度 < `minObjectWidthPx` 的短段以去除噪声。

### 传感器融合（`core/Fusion/`）

支持五种融合模式，通过配置文件 `<fusion><mode>` 切换：

| 模式 | 行为 |
|------|------|
| `XRayOnly` | 仅用 X 射线结论 |
| `CameraOnly` | 仅用相机结论 |
| `AndReject` | 两路均判为矸石才喷 |
| `OrReject` | 任一判为矸石即喷 |
| `XRayAuthoritative` | X 射线优先，相机仅在 X 射线为 Unknown 时生效 |

### 时序计算（`core/Timing/`）

```
开阀时刻 = now + (传感器到喷嘴距离 / 皮带速度) - 阀门开启延迟 - 气动飞行延迟 - 安全裕量
持续时长 = (物料像素段宽度 / 皮带速度) + 阀门关闭延迟
```

所有时刻以 `steady_clock` 纳秒绝对时间戳传递给 `IValveDriver::schedule`，避免浮点延迟误差。

### 喷嘴映射（`core/NozzleMapping/`）

`NozzleGeometry` 配置皮带宽度、像素/喷嘴比（`pixelsPerNozzle`）和两侧扩边像素数。
`nozzlesForRange(pixelStart, pixelEnd)` 返回覆盖该物料段的所有喷嘴 ID 列表（最多 64 路）。

---

## 配置说明

将 `config/config.example.xml` 复制为 `config/config.xml`（或设置环境变量 `CGS_CONFIG_PATH`），按现场实际调整：

```xml
<!-- 硬件类型：mock | aurora | vj-serial | el2828 | beckhoff -->
<xray type="mock" port="/dev/ttyUSB0" kv="80" ma="5"/>
<detector type="mock" lineRateHz="1000" width="1024"/>

<!-- 几何参数（现场实测后填写） -->
<geometry>
    <beltWidthMm>1000</beltWidthMm>
    <sensorToNozzleMm>860</sensorToNozzleMm>   <!-- 传感器到喷嘴距离 -->
    <nozzleSpacingMm>30</nozzleSpacingMm>
    <nozzleCount>64</nozzleCount>
    <pixelsPerNozzle>16</pixelsPerNozzle>
</geometry>

<!-- 时序参数（在台架上实测 DF8 阀门后填写） -->
<timing>
    <valveOpenLatencyMs>8</valveOpenLatencyMs>
    <valveCloseLatencyMs>6</valveCloseLatencyMs>
    <pneumaticTravelMs>2</pneumaticTravelMs>
    <safetyMarginMs>1</safetyMarginMs>
</timing>
```

> **注意：** 命令行与 GUI 会尝试加载 `config/config.xml`（或 `--config` / `CGS_CONFIG_PATH`）；若文件不存在则使用内置默认参数（`--real` 仅在该情况下启用「真实硬件预设」类型名）。

---

## GUI 界面

运行 `coal_gangue_sorter_gui --mock` 后，界面包含：

- **工具栏**：Arm（启动喷吹）/ Disarm（停止喷吹）/ 急停按钮
- **X 射线瀑布图**（`XRayWaterfallView`）：滚动显示实时线扫原始灰度
- **64 路喷嘴矩阵**（`NozzleMatrixView`）：实时显示各通道触发状态
- **参数面板**（`ConfigPanel`）：在线修改分类阈值、时序参数、喷嘴映射比
- **统计面板**（`StatsPanel`）：帧率、矸石率、误判统计

详细界面说明见 [`docs/gui-build.md`](docs/gui-build.md)。

---

## Python 工具

依赖：`numpy`, `matplotlib`, `pyserial`, `pyyaml`（`pip install -r tools/requirements.txt`）

| 脚本 | 用途 |
|------|------|
| `tools/calibrate_belt.py` | 通过打点采样估算皮带线速度 |
| `tools/timing_simulator.py` | 在不接硬件的情况下仿真时序参数灵敏度 |
| `tools/replay_session.py` | 加载录制的原始帧文件，离线重现分类结果 |
| `tools/nozzle_self_test.py` | 顺序触发各喷嘴通道，验证气路与接线 |

---

## 硬件清单

| 设备 | 型号 / 厂商 | 备注 |
|------|-------------|------|
| X 射线源 | **VJ Technologies IXS200BP500P479**（控制盒 ZS3000-011） | RS232 J3；规格 SPC-P479 REV3；协议见 `vj-xray-rs232.md`；完整固件手册 `P032-IXS-FIRMWARE-P032 R5` 可向厂家索取 |
| 线阵探测器 | Detection Technology Aurora | SDK 不入库，需现场交付 |
| 工业相机 | 凌云光（GigE） | 硬件触发模式 |
| 电磁阀 | DF8-2024-10（合肥坤双光电） | 由 EL2828 直驱，高速，需台架测延迟 |
| PLC / IO | Beckhoff TwinCAT 3 + EL2828 | 喷嘴经 EtherCAT DO 直驱；ADS 与上位机通信，变量约定见文档 |
| 配电柜 | 山西天朗电气 | — |

关联合同：**太原泓博科技** C-2024000002

---

## 文档导航

### 新人入门
- 4 周成长路线：[`docs/learning-path.md`](docs/learning-path.md)

### 硬件
- 物料盘点：[`docs/hardware/inventory.md`](docs/hardware/inventory.md)
- I/O 点表：[`docs/hardware/io-table.md`](docs/hardware/io-table.md)
- 搭建手册：[`docs/hardware/build-guide.md`](docs/hardware/build-guide.md)
- 安全规程：[`docs/hardware/safety.md`](docs/hardware/safety.md)
- 协议问题清单：[`docs/hardware/protocol-questions.md`](docs/hardware/protocol-questions.md)

### 协议规范
- VJ X 射线 RS232：[`docs/hardware/protocols/vj-xray-rs232.md`](docs/hardware/protocols/vj-xray-rs232.md)
- Detection Technology Aurora：[`docs/hardware/protocols/detection-tech-aurora.md`](docs/hardware/protocols/detection-tech-aurora.md)
- Beckhoff ADS：[`docs/hardware/protocols/beckhoff-ads.md`](docs/hardware/protocols/beckhoff-ads.md)

### 软件
- 架构与设计决策：[`docs/architecture.md`](docs/architecture.md)
- Qt GUI 构建说明：[`docs/gui-build.md`](docs/gui-build.md)

---

## 已知局限 / 后续工作

- **相机融合主循环接入**：`PipelineEngine` 已预留接口，但当前主循环将相机输入固定传入 `Material::Unknown`，待相机帧同步逻辑完成后接通。
- **根目录 `sorter` 二进制**：疑似遗留构建产物，建议确认后清理。

---

## License

TBD（项目内部使用）
