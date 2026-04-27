# Coal-Gangue Sorter / 煤矸石分选机软件

基于 X 射线 + 工业相机双传感器的智能煤矸石分选机上位机软件。  
本仓库是从 [Industrial-Thickness-Measurement-System](https://github.com/PeasantDeToussaint/Industrial-Thickness-Measurement-System) 现代化版本派生的全新项目。

## 项目状态

✅ **软件层全部完成** —— 8/8 阶段就绪，等待现场硬件 SDK / 协议交付即可上线。

| 阶段 | 状态 | 内容 |
|---|---|---|
| Phase 0 | ✅ | 资料盘点 + I/O 点表 + 现场清单 |
| Phase 1 | ✅ | 仓库骨架 + 5 抽象接口 + 5 Mock 实现 |
| Phase 2 | ✅ | 核心算法（分类/时序/映射/融合）+ 29 单元测试 |
| Phase 3 | ✅ | 气枪驱动（Beckhoff EL2828 直驱已实现，FaDriver-64 待现场抓包） |
| Phase 4 | ✅ | X 射线源 RS232 驱动 + Aurora SDK 适配器 |
| Phase 5 | ✅ | Beckhoff TwinCAT 3 ADS 适配器 + PLC 变量约定 |
| Phase 6 | ✅ | Qt 6 GUI（瀑布图 + 64路矩阵 + 实时调参 + 统计） |
| Phase 7 | ✅ | Python 工具（标定 / 仿真 / 回放 / 自检） |
| Phase 8 | ✅ | 硬件搭建手册 + 安全规程 |

## 快速开始（无硬件 Mock 模式）

```bash
git clone <repo>
cd coal-gangue-sorter
cmake -B build -DBUILD_GUI=ON -DUSE_MOCKS=ON
cmake --build build -j
./build/app/coal_gangue_sorter --mock
```

不需要任何硬件即可看到：X 射线 / 相机假数据流、64 路喷嘴矩阵闪烁、PLC 状态。

## 系统架构

```
[光电触发] → [PLC] ──┬──→ [X射线探测器] ───┐
                      └──→ [凌云光相机] ────┴──→ [上位机分类]
                                                      ↓
                                          [喷嘴映射 + 时序计算]
                                                      ↓
                                      ┌── COM ── [FaDriver-64] ──┐
                                      └──── PLC ───────────────┐  │
                                                              ↓  ↓
                                                      [三色灯] [DF8电磁阀×64]
                                                                   ↓
                                                              [气枪喷嘴×52]
                                                                   ↓
                                                          [矸石被吹离 → 废料带]
```

## 目录结构

```
coal-gangue-sorter/
├── core/              纯 C++ 算法核心（无 Qt 依赖）
├── hardware/          抽象硬件接口 + Mock + 真实实现
├── app/               Qt GUI 主程序与插件
├── tests/             单元测试
├── tools/             Python 离线工具（标定、回放）
├── config/            配置模板
├── docs/              文档
└── third_party/       厂商 SDK 占位
```

## 文档导航

### 新人工程师
- 入职：[`docs/learning-path.md`](docs/learning-path.md) - 4 周成长路线
- 现场清单：[`docs/hardware/site-survey-checklist.md`](docs/hardware/site-survey-checklist.md)

### 硬件
- 物料盘点：[`docs/hardware/inventory.md`](docs/hardware/inventory.md)
- I/O 点表：[`docs/hardware/io-table.md`](docs/hardware/io-table.md)
- 搭建手册：[`docs/hardware/build-guide.md`](docs/hardware/build-guide.md)
- 安全规程：[`docs/hardware/safety.md`](docs/hardware/safety.md)
- 协议清单：[`docs/hardware/protocol-questions.md`](docs/hardware/protocol-questions.md)

### 协议规范（已破解）
- VJ X 射线 RS232：[`docs/hardware/protocols/vj-xray-rs232.md`](docs/hardware/protocols/vj-xray-rs232.md)
- Detection Technology Aurora：[`docs/hardware/protocols/detection-tech-aurora.md`](docs/hardware/protocols/detection-tech-aurora.md)
- Beckhoff ADS：[`docs/hardware/protocols/beckhoff-ads.md`](docs/hardware/protocols/beckhoff-ads.md)
- FaDriver-64（待现场）：[`docs/hardware/protocols/fadriver-64.md`](docs/hardware/protocols/fadriver-64.md)

### 软件
- 架构：[`docs/architecture.md`](docs/architecture.md)
- GUI 构建：[`docs/gui-build.md`](docs/gui-build.md)

## 关联硬件

- **太原泓博科技** 项目（合同号 C-2024000002）
- **山西天朗电气** 配电柜设计
- **AnySystem FaDriver-64** 64 路高速电磁阀驱动板
- **DF8-2024-10** 高速电磁阀（合肥坤双光电）
- **凌云光** 工业相机
- **X 射线源 + 线阵探测器**（型号待铭牌确认）

## 开发依赖

- C++17
- Qt 5.12+ 或 Qt 6（自动检测）
- CMake 3.16+
- 可选：OpenCV 4（图像处理）、Python 3.10+（离线工具）

## License

TBD（项目内部使用）
