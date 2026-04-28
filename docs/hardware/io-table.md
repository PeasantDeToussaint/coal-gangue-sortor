# I/O 点表

> 本表是软件与硬件协同的核心契约文档。  
> 所有信号、地址、协议在此对齐；任何变更必须先改本文件再改代码。  
> **状态**：`⚙️ ADS 待配置` = 已确认上位机经 **Beckhoff ADS / TwinCAT** 交互，具体 GVL 符号或与变频器/AI 的绑定待现场对齐（见 [`protocol-questions.md`](protocol-questions.md) Q5、Q6）。

## 1. 信号方向定义

- **DI** = Digital Input（数字输入，从外设读到控制器）
- **DO** = Digital Output（数字输出，控制器驱动外设）
- **AI** = Analog Input（模拟输入）
- **AO** = Analog Output（模拟输出）
- **DataIn** = 大数据流入（图像 / 帧）
- **DataOut** = 大数据流出
- **Cmd** = 命令型双向（请求-响应）

## 2. 系统级信号（控制层）

| # | 信号名 | 方向 | 起点 | 终点 | 物理层 | 协议 | 时序要求 | 状态 | 备注 |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 物料到位触发 | DI | 光电开关 | PLC | 24V 干接点 | 硬线 | 上升沿 | ✅ 已有 | 物料进入检测区起始 |
| 2 | 触发同步给上位机 | DI | PLC | PC | 以太网 | Beckhoff ADS（读 GVL 中的触发/同步 BOOL 或计数器） | < 1 ms 抖动 | ⚙️ ADS 待配置 | 用于 X 射线/探测器与上位机节拍对齐；符号见 `beckhoff-ads.md` |
| 3 | 急停状态回读 | DI | 急停按钮 + 安全继电器 | PLC + 主回路 | 24V 双回路 | 硬线 | < 100 ms | ✅ 已有 | 双回路冗余 |
| 4 | 系统就绪 | DO | PLC | 三色绿灯 | 24V | 硬线 | — | ✅ 已有 | |
| 5 | 系统运行 | DO | PLC | 三色黄灯 | 24V | 硬线 | — | ✅ 已有 | |
| 6 | 系统故障 | DO | PLC | 三色红灯 + 蜂鸣 | 24V | 硬线 | — | ✅ 已有 | |
| 7 | 上料带启停 | DO | PC / HMI | PLC → CJX2-2510 | 以太网 + 硬线 | Beckhoff ADS → TwinCAT → DO/接触器（GVL 待对齐） | < 500 ms | ⚙️ ADS 待配置 | 7.5 kW |
| 8 | 振动分筛启停 | DO | PC / HMI | PLC → CJX2-4011 | 以太网 + 硬线 | Beckhoff ADS → TwinCAT → DO/接触器（GVL 待对齐） | < 500 ms | ⚙️ ADS 待配置 | 15 kW |
| 9 | 粉末料带启停 | DO | PC / HMI | PLC → CJX2-1810 | 以太网 + 硬线 | Beckhoff ADS → TwinCAT → DO/接触器（GVL 待对齐） | < 500 ms | ⚙️ ADS 待配置 | 4.5 kW |
| 10 | **检测带变频频率** | AO / Cmd | PC / HMI | PLC → 变频器 | 以太网 ± 现场总线 | Beckhoff ADS（或经 EL6xxx 网关）；寄存器/Hz 映射 **Q6 待确认** | 写入响应 < 100 ms | ⚙️ ADS 待配置 | 4 kW，控制皮带速度（关键工艺参数） |
| 11 | 检测带启停 | DO | PC / HMI | PLC → 变频器使能 | 以太网 + 硬线 | Beckhoff ADS → TwinCAT（GVL 待对齐） | < 500 ms | ⚙️ ADS 待配置 | |
| 12 | 合格料带启停 | DO | PC / HMI | PLC → CJX2-1810 | 以太网 + 硬线 | Beckhoff ADS → TwinCAT → DO/接触器（GVL 待对齐） | < 500 ms | ⚙️ ADS 待配置 | 5.5 kW |
| 13 | 废料带启停 | DO | PC / HMI | PLC → CJX2-1810 | 以太网 + 硬线 | Beckhoff ADS → TwinCAT → DO/接触器（GVL 待对齐） | < 500 ms | ⚙️ ADS 待配置 | 5.5 kW |
| 14 | 空压机启停 | DO | PLC | 接触器 | TwinCAT 内部逻辑 + 硬线 | TwinCAT 程序（上位机经 ADS 可间接联锁） | — | ⚙️ ADS 待配置 | 75 kW，独立控制 |
| 15 | 气源压力反馈 | AI | 压力传感器 | PLC | 4–20 mA → EL1xxx AI | Beckhoff ADS 读 AI 映像（通道待配置） | 1 Hz 巡检 | ⚙️ ADS 待配置 | 低压报警 |
| 16 | 吹粉尘电磁阀 | DO | PLC | 阀 | 24V | 硬线 | 定时器或事件 | ✅ 已有 | 探测器视窗清洁 |
| 17 | 各电机过载 | DI | 热继电器 | PLC | 24V 干接点 | 硬线 | — | ✅ 已有 | 故障联锁 |

## 3. 检测层信号（传感采集）

| # | 信号名 | 方向 | 起点 | 终点 | 物理层 | 协议 | 时序要求 | 状态 | 备注 |
|---|---|---|---|---|---|---|---|---|---|
| 18 | X射线源 高压使能 | DO | PC | X射线控制器 | RS232（J3 DB9） | VJ ASCII 命令（`vj-xray-rs232.md`） | < 5 s 启辉 | ✅ 已有 | `XRaySerial.cpp`；慢启动，需预热 |
| 19 | X射线 管电压设定 | AO / Cmd | PC | X射线控制器 | RS232 | VJ `VP` / `CP` 等命令 | — | ✅ 已有 | kV / μA 设定见协议笔记 |
| 20 | X射线 管电流设定 | AO / Cmd | PC | X射线控制器 | RS232 | 同上 | — | ✅ 已有 | 与行 19 同一串口会话 |
| 21 | X射线 状态回读 | AI / Cmd | X射线控制器 | PC | RS232 | VJ `STAT` / `MON` / `FLT` 等 | 1 Hz 巡检 | ✅ 已有 | 联锁、温度、故障字 |
| 22 | X射线 帧数据 | DataIn | 线阵探测器 | PC | GigE（GCU） | Aurora X-LIB SDK（`detection-tech-aurora.md`） | 行频可配（积分时间/参数） | ✅ 已有 | `DetectorAurora.cpp`；16 bit 单行 |
| 23 | 相机 触发命令 | Cmd | PC | 凌云光相机 | GigE Vision | GenICam / SDK | 软触发 < 1 ms | ❓ | **设备未到货，暂缓 Q3**；或硬触发线 |
| 24 | 相机 帧数据 | DataIn | 凌云光相机 | PC | GigE 千兆网 | GigE Vision (GenICam) | 与 X 射线同步 | ❓ | **暂缓 Q3** |
| 25 | 光源使能 | DO | PC / PLC | 光筒1100 LED 驱动器 | 24V / DALI / TBD | TBD | — | ❓ | **暂缓 Q4**（与相机同步） |
| 26 | 光源亮度 | AO | PC | LED 驱动器 | PWM / 0-10V / TBD | TBD | — | ❓ | **暂缓 Q4** |

## 4. 执行层信号（喷射 → 高速、抖动敏感）

| # | 信号名 | 方向 | 起点 | 终点 | 物理层 | 协议 | 时序要求 | 状态 | 备注 |
|---|---|---|---|---|---|---|---|---|---|
| 27 | 喷嘴 1–64 触发（上位机 → PLC） | Cmd | PC | TwinCAT（EL2828 映像） | 以太网 | Beckhoff ADS（TCP 48898） | EtherCAT 任务周期级 | ✅ 已有 | `ValveDriverEL2828`，见 beckhoff-ads.md |
| 28 | DF8 阀 1–64 驱动 | DO | EL2828（TwinCAT） | DF8 电磁阀 | 24V 高速 | 硬线 | 5–15 ms 响应 | ✅ 已有 | 工厂标定 |
| 29 | 气源压力 | AI | 压力开关 | PC（可选） | 4–20 mA / 数字阈值 | 经 PLC AI 映像 + ADS 读（或直连 AI 卡） | 慢 | ⚙️ ADS 待配置 | 喷射前自检；与行 15 同源信号不同消费端时可合并描述 |

## 5. 上位机内部数据流

| # | 数据 | 起点 | 终点 | 频率 | 备注 |
|---|---|---|---|---|---|
| A | X射线行帧 | DetectorInterface | Classifier | 1–4 kHz | 单行 N 像素 |
| B | 相机帧 | CameraInterface | Classifier | 与触发同步或自由运行 | 区域或线阵 |
| C | 分类结果（每行） | Classifier | NozzleMapper | 与 A/B 同频 | {COAL, GANGUE, EDGE} 数组 |
| D | 喷嘴动作清单 | NozzleMapper + TimingCalc | ValveDriver | 帧率 ÷ 节拍 | (nozzleId, fireAtNs, durationMs) |
| E | 喷射回执 | ValveDriver | StatsLogger | 异步 | 实际触发时间戳，用于审计 |
| F | 系统状态 | PLCInterface | UI | 1–5 Hz | 启停、急停、变频频率 |
| G | 报警事件 | 任意模块 | AlarmPlugin | 即时 | 含级别 / 来源 / 描述 |

## 6. 端子分配（待与 XT1 对齐）

电气图标注 XT1 端子排有 30 个端子，编号 101–149 + N + PE。具体回路号到现场后逐条核对：

| 端子号 | 推测含义 | 回路号 |
|---|---|---|
| 1 | 控制电源 L1 | 101 |
| 2 | … | 103 |
| 3 | … | 105 |
| 4 | … | 107 |
| 5 | 电源指示 | 109 |
| 6 | 启动按钮 | 111 |
| 7 | 停止按钮 | 113 |
| 8 | 自锁继电器 1KA1 | 115 |
| 9 | 自锁继电器 1KA2 | 119 |
| 10 | 故障继电器 | 121 |
| 11 | 运行指示 | 123 |
| 12 | 停止指示 | 125 |
| 13 | 来自PLC控制柜 信号 | 127 |
| … | … | … |
| 30 | N（中性线） | — |

> **TODO**：现场用万用表逐点测量，把"推测含义"列改为"实测含义"。

## 7. 软件接口对应

每条信号在软件中由哪个接口承载：

| 信号 # | C++ 接口 | 方法 |
|---|---|---|
| 1 | `IPLC` | `subscribeMaterialTrigger(callback)` |
| 4–6 | `IPLC` | `setLamp(LampColor, LampState)` |
| 7–14 | `IPLC` | `setConveyor(ConveyorId, bool)` |
| 10 | `IPLC` | `setBeltSpeedHz(double)` |
| 18–22 | `IXRay` + `IDetector` | `start()` / `setKv()` / `pollFrame()` |
| 23–24 | `ICamera` | `triggerSoft()` / `pollFrame()` |
| 27 | `IValveDriver` | `schedule()`（ADS → EL2828） |
| 28 | `IValveDriver` | `selfTest()` / `pollStatus()` |
