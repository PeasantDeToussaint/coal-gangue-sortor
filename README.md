# Coal-Gangue Sorter / 煤矸石分选机软件

基于 X 射线 + 工业相机双传感器的智能煤矸石分选机上位机软件。  
本仓库是从 [Industrial-Thickness-Measurement-System](https://github.com/PeasantDeToussaint/Industrial-Thickness-Measurement-System) 现代化版本派生的全新项目。

## 项目状态

🚧 **早期开发中** —— Mock 框架已可运行，硬件适配层逐步推进。

| 阶段 | 状态 | 内容 |
|---|---|---|
| Phase 0 | ✅ | 资料盘点与 I/O 点表（`docs/hardware/`）|
| Phase 1 | 🚧 | 仓库骨架、4 个抽象接口、Mock 实现 |
| Phase 2 | ⏳ | 核心算法（分类、时序、喷嘴映射）+ 单元测试 |
| Phase 3 | ⏳ | FaDriver-64 串口驱动 |
| Phase 4 | ⏳ | X 射线探测器 + 凌云光相机 |
| Phase 5 | ⏳ | PLC + 变频器 + 三色灯 |
| Phase 6 | ⏳ | Qt UI 主流程 |
| Phase 7 | ⏳ | 标定与离线工具 |
| Phase 8 | ⏳ | 硬件搭建手册 |

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

- 新人入职：[`docs/learning-path.md`](docs/learning-path.md)
- 硬件资料：[`docs/hardware/inventory.md`](docs/hardware/inventory.md)
- I/O 点表：[`docs/hardware/io-table.md`](docs/hardware/io-table.md)
- 待解决协议：[`docs/hardware/protocol-questions.md`](docs/hardware/protocol-questions.md)
- 软件架构：[`docs/architecture.md`](docs/architecture.md)（编写中）
- 算法说明：[`docs/algorithm.md`](docs/algorithm.md)（编写中）
- 现场标定：[`docs/calibration.md`](docs/calibration.md)（编写中）

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
