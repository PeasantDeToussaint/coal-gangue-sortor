# 软件架构

## 分层

```
┌─────────────────────────────────────────────┐
│  app/    Qt GUI 主程序 + 插件                 │  Phase 6 ✅
├─────────────────────────────────────────────┤
│  core/   纯 C++ 算法（无 Qt 依赖）             │  Phase 2 ✅
│   ├── Config            config.xml 解析（tinyxml2）│
│   ├── Classifier        X射线/相机 单帧分类     │
│   ├── Fusion            双信号融合策略         │
│   ├── NozzleMapping     像素 → 喷嘴ID         │
│   └── Timing            速度 + 距离 → 延迟     │
├─────────────────────────────────────────────┤
│  hardware/  抽象接口（Phase 1 ✅）+ 实现        │
│   ├── XRayInterface     X射线源（kV/mA/启停）  │  Phase 4 ✅
│   ├── DetectorInterface 线阵探测器（行帧流）   │  Phase 4 ✅
│   ├── CameraInterface   工业相机（GigE Vision）│  暂缓（设备未到货）
│   ├── ValveDriverInterface  64路阀驱动        │  Phase 3 ✅
│   └── PLCInterface      皮带/变频/灯/急停      │  Phase 5 ✅
└─────────────────────────────────────────────┘
```

每层只依赖下层。`core` 完全独立于 Qt 和厂商 SDK，可以纯命令行做大量回归测试。

## 数据流（运行态）

```
Detector ──▶ DetectorFrame ─┐
                              ├─▶ Classifier ──▶ Segments ──▶ Fusion ──▶ Material/pixel
Camera   ──▶ CameraFrame ──┘                                        │
                                                                     ▼
                              Belt speed (PLC) ──▶ TimingCalculator
                                                                     │
                                                                     ▼
              Frame.timestampNs + delay  ──▶  fireAtNs
                                                                     │
                                                                     ▼
              Pixel range ──▶ NozzleMapper ──▶ {nozzleIds}
                                                                     │
                                                                     ▼
                                  std::vector<NozzleCommand> ──▶ ValveDriver
                                                                     │
                                                                     ▼
                        Beckhoff ADS ──▶ TwinCAT / EL2828 ──▶ DF8 阀 ──▶ 气枪
```

## 关键设计决策

### 1. Mock 和真实实现走同一接口
`hardware/*/I*.h` 的所有方法两套实现都满足。开发期纯靠 Mock，硬件到位后只换工厂。

### 2. 时序统一用 `std::chrono::steady_clock` 的 ns 时间戳
- `DetectorFrame::timestampNs` 是采集瞬间
- `NozzleCommand::fireAtNs` 是触发瞬间
- 算法层不关心 wall clock，只算相对延迟
- 这样断电重启 / 时区切换都不会影响触发时序

### 3. 喷嘴命令是"批量预约"，不是"立即触发"
- 上位机把未来的喷射时刻打包交给 `IValveDriver`（生产路径为 `ValveDriverEL2828`：ADS 写 GVL BOOL）
- TwinCAT / EtherCAT 周期（通常约 1 ms）内翻转到目标 DO，与 DF8 阀 5–15 ms 机械响应相匹配
- 上位机只需要保证"最迟在执行前 N ms 把命令到位"
- 这是 64 路分选系统的常见做法（周期由现场 PLC 任务与总线配置决定）

### 4. 分类算法和融合策略分离
`Classifier` 输出每像素初判，`FusionPolicy` 决定如何把双源结合：
- `XRayOnly`：相机未到货时
- `XRayAuthoritative`：相机辅助 Unknown 像素
- `AndReject`：保守模式，两个传感器都说矸石才扔
- `OrReject`：激进模式，任一说矸石就扔
- `CameraOnly`：调试 / 无 X 射线场景

### 5. 单元测试覆盖纯算法
不需要硬件、不需要 Qt、CI 上 30 ms 跑完。任何对算法行为的修改都先看测试是否更新。

## 依赖

- C++17 (std::chrono, std::filesystem 可用)
- POSIX threads (`-pthread`)
- [tinyxml2](https://github.com/leethomason/tinyxml2)（CMake FetchContent 自动拉取，解析 `config.xml`）
- 可选：Qt 5.12+ / Qt 6（仅 GUI）
- 可选：[Beckhoff/ADS C++ 库](https://github.com/Beckhoff/ADS)（MIT；克隆到 `third_party/ads/`，CMake 自动检测）
- 可选：Detection Technology Aurora X-LIB（Windows `.dll`/`.lib` 在 `third_party/aurora-sdk/lib/`，不入 Git）

第三方厂商 SDK 都通过 `hardware/*/Real*.cpp` 桥接，其余代码不引用。

## 关键采购单信息（已从文件确认）

### 控制系统：Beckhoff TwinCAT 3 + EtherCAT

来源：山西永创自动化（2026-01-09 货物签收单）

PC（FC9022 PCIe 主站卡）
  → EtherCAT 总线（1ms 实时周期）
    → EK1501 耦合器（光纤）
      → EL2828 × 20（160路 24V 2A DO）← 驱动电磁阀 + 其他 I/O
      → EL9410 × 4（总线供电）
      → EL6070（TwinCAT 软件许可 Dongle）

TwinCAT 3 Runtime（TC1100-0291，注册码 00386449）
  ↕ ADS 协议（TCP/IP，端口 48898）
Qt 上位机软件

### X 射线源：VJ Technologies IXS200BP500P479

来源：规格文件 SPC-P479 REV3（已取得）

J3（RS232，9针母口）
  Pin2=TX-，Pin3=RX+，Pin5=GND
  → 上位机 COM 口 → IXRayReal 驱动
  协议文档：P032-IXS-FIRMWARE-P032 R5（待取得）

## 构建

```bash
# Mock 模式（无硬件，开发/CI 用）
cmake -B build -DCGS_BUILD_TESTS=ON -DCGS_BUILD_CONSOLE=ON -DCGS_BUILD_GUI=ON
cmake --build build -j
ctest --test-dir build
./build/app/coal_gangue_sorter_console --seconds 5

# 真实硬件模式（Windows 工控机，已有 third_party/ads/ 和 aurora-sdk/lib/）
cmake -B build \
  -DCGS_BUILD_GUI=ON \
  -DCGS_HAS_SERIAL=ON \
  -DCGS_HAS_AURORA_SDK=ON \
  -DCGS_HAS_BECKHOFF_ADS=ON
cmake --build build -j

# 配置文件（第一次部署）
cp config/config.example.xml config/config.xml
# 编辑 config.xml：填串口号、ADS endpoint、实测几何/时序参数
./build/app/coal_gangue_sorter_gui
```

## 完成情况（全部 Phase 已交付）

| Phase | 内容 | 状态 |
|-------|------|------|
| 1 | 仓库骨架 + 5 个抽象接口 + Mock | ✅ |
| 2 | 核心算法 + 29 个单元测试 | ✅ |
| 3 | Beckhoff EL2828 经 ADS 直驱（`ValveDriverEL2828.cpp`） | ✅ |
| 4 | VJ X射线 RS232（`XRaySerial.cpp`）+ Aurora 探测器（`DetectorAurora.cpp`） | ✅ |
| 5 | Beckhoff TwinCAT ADS 适配器（`PLCBeckhoff.cpp`）+ 变量约定 | ✅ |
| 6 | Qt GUI（瀑布图 / 矩阵 / 调参面板 / 统计） | ✅ |
| 7 | Python 工具（标定 / 仿真 / 回放 / 自检） | ✅ |
| 8 | 硬件手册 + 安全规程 | ✅ |
| — | XML 配置加载（`core/Config/ConfigLoader`，tinyxml2） | ✅ |

**剩余工作全部为现场事项**：几何实测 (Q8)、DF8 阀响应实测 (Q9)、与 PLC 工程师对齐 GVL 变量名、变频器接入形态确认 (Q6)。详见 `docs/hardware/protocol-questions.md`。
