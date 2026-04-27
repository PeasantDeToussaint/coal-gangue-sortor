# 待解决的协议与厂商资料清单

> 这是软件复刻最大的几块未知。每一项都建议建一个 GitHub issue，分配给负责人。

## 紧急 P0（不解决软件无法上线）

### Q1. ✅ 已关闭！气枪驱动方案确认：Beckhoff EL2828 直驱

**FaDriver-64 已废弃**，不再使用。气枪由 Beckhoff EL2828 直驱：

- EL2828 × 20 = 160 路 24V/2A DO，其中 64 路对应 DF8 电磁阀
- ✅ `hardware/ValveDriverInterface/ValveDriverEL2828.cpp` 已实现 `IValveDriver`
- ✅ 1ms EtherCAT 周期精度，满足 DF8 阀 5–15 ms 响应需求
- ✅ `ValveDriverEL2828` 通过 Beckhoff ADS 写 PLC GVL 变量触发通道

**软件链路（最终方案）**：

```
PipelineEngine → ValveDriverEL2828 → ADS → TwinCAT → EL2828 → DF8 阀 → 气枪喷嘴
```

### Q2. ✅ 已完全解决！X 射线源协议已破解 + 实现

**VJ Technologies IXS200BP500P479**（美国，规格文件 SPC-P479 REV3）

- 控制接口：**RS232（J3，9针母口，Pin2=TX-，Pin3=RX+，Pin5=GND）**
- 串口参数：**9600 8N1 无流控**
- 帧格式：`<STX><命令文本><CR>`
- 完整协议笔记：**[`docs/hardware/protocols/vj-xray-rs232.md`](protocols/vj-xray-rs232.md)** ✅
- 命令集（10 条）：WDOG / PTM / ENBL / VP / CP / CLR / FLT / MON / STAT / PSTAT
- 故障字 9 位：调节/联锁/KOV/AOV/过温/弧/过流/功率/过压
- 联锁：J2 Pin1&Pin2 必须短接才能出射线（安全门开关串联）
- 最大参数：100–200 kV，0.2–2.5 mA，500 W

**已完成**：
- ✅ 旧项目 `XRayLib.cpp` 已破解全部 10 条 ASCII 命令
- ✅ 新仓库 `hardware/XRayInterface/XRaySerial.cpp` 已实现 `IXRaySource` 接口
- ✅ 跨平台 `core/Serial/SerialPort.cpp`（POSIX termios + Win32 CreateFile）
- ✅ 单元测试 `tests/test_xray_serial_format.cpp` 守住协议编码

**仍可索取**（更高准确度，非阻塞）：联系 VJ Technologies（+1-631-981-7100）拿 `P032-IXS-FIRMWARE-P032 R5` 比对漏掉的命令

### Q3. ⏸ 暂缓！工业相机（当前系统未安装）

当前分选机为**纯 X 射线方案**，无工业相机。  
凌云光相机 + 光筒1100 光源是可选扩展，未来到货时再处理。  
软件已预留 `CameraInterface` + `CameraMock`，接入时无需改架构。

> 待设备到货后恢复此 Q3，届时补型号和 SDK。

### Q4. ⏸ 暂缓！光源（当前未安装，与相机同步）

同 Q3，随相机一起接入。

## 高 P1（影响系统集成）

### Q5. ✅ 已完全解决！Beckhoff TwinCAT 3 + EtherCAT 适配器已实现

**确认来源**：货物签收单（山西永创自动化工程有限公司，2026-01-09）

| 模块 | 型号 | 功能 |
|---|---|---|
| EtherCAT 主站卡 | FC9022（PCIe） | 装工控机，驱动整个 EtherCAT 网络 |
| EtherCAT 耦合器 | EK1501 × 2 | 光纤接口，防干扰 |
| 数字量输出 | EL2828 × 20 | 160 路 24V 2A DO（核心执行 I/O）|
| 软件运行时 | TC1100-0291 | TwinCAT 3 PLC Runtime，注册码 00386449 |
| 许可密钥端子 | EL6070 × 1 | 软件 Dongle |
| 电源端子 | EL9410 × 4 | 给总线供 24V |
| 末端盖帽 | EL9011 × 2 | 端子排末端 |

**上位机通信协议**：**Beckhoff ADS（Automation Device Specification）over TCP/IP**
- 默认端口：48898（路由），851（PLC Runtime）
- 开源库：[Beckhoff/ADS C++ 库](https://github.com/Beckhoff/ADS)（MIT 开源）
- 完整接入文档：**[`docs/hardware/protocols/beckhoff-ads.md`](protocols/beckhoff-ads.md)** ✅

**已完成**：
- ✅ 协议文档 + PLC 变量映射约定（GVL）
- ✅ `hardware/PLCInterface/PLCBeckhoff.cpp` 实现 `IPLC` 接口（条件编译 `CGS_HAS_BECKHOFF_ADS`）
- ✅ `BeckhoffPlcVariableMap` 配置驱动，PLC 工程师改名不影响代码
- ✅ 50Hz 后台变化检测线程（光电触发 + 急停事件）

**最后一步**（部署时执行）：
- `git clone https://github.com/Beckhoff/ADS third_party/ads`
- 编译开关：`cmake -DCGS_HAS_BECKHOFF_ADS=ON -DCGS_BECKHOFF_ADS_DIR=third_party/ads`
- 与 PLC 工程师对齐 GVL 变量名（约 15 个，详见协议文档）

### Q6. 变频器品牌与寄存器
- **现场**：检测带 4 kW 变频器（可能也挂在 EtherCAT 总线上，或 Modbus 独立）
- **注意**：有了 TwinCAT，变频器可能通过 EL6xxx 总线网关连接，而非直接 Modbus
- **获取途径**：拍铭牌 + 查厂家手册；看 TwinCAT 项目源码（如果能拿到的话）
- **里程碑**：Phase 5

### Q7. ✅ 已完全解决！探测器 SDK 已提取 + 适配器已写

**Detection Technology Inc. Aurora 系统**（合同 00001861-15，2026-01-04）

- **X-Card DA21506414C × 17**：像素阵列模块（17 × 128 = 2180 像素总宽）
- **X-GCU GT × 1**：全局控制单元（GigE 千兆以太网，UDP 广播发现）
- **触发线 15m**：光电触发信号接入 GCU
- **像素深度**：16 bit（uint16_t）
- 完整接入文档：**[`docs/hardware/protocols/detection-tech-aurora.md`](protocols/detection-tech-aurora.md)** ✅

**已完成**：
- ✅ 旧项目 `include/DetInclude/` 47 个头文件 = 完整 Aurora X-LIB SDK
- ✅ 旧项目 `DetectorLib.cpp` = 完整使用范例
- ✅ 75 个 `XPARA_*` 参数枚举已提取
- ✅ 新仓库 `hardware/DetectorInterface/DetectorAurora.cpp` 已实现 `IDetector` 接口（条件编译 `CGS_HAS_AURORA_SDK`）

**最后一步**（部署时执行）：
- 把 `include/DetInclude/*.h` 拷贝到 `third_party/aurora-sdk/include/`
- 拿到 `xlib.dll` / `xlib.lib`（向地太科特北京 +86 10 6783 2601 索取）放到 `third_party/aurora-sdk/lib/`
- 编译开关：`cmake -DCGS_HAS_AURORA_SDK=ON -DCGS_AURORA_SDK_DIR=third_party/aurora-sdk`

## 中 P2（影响标定精度）

### Q8. 现场几何参数（必须实测，不能查图）
- 探测器中心 → 喷嘴中心 沿皮带方向距离 `L_sd`（mm）
- 喷嘴间距 `S_n`（≈ 30 mm，待实测）
- 喷嘴 1 在皮带横向上的偏移 `X_offset`
- 皮带宽度 `W_belt`
- 皮带额定速度（变频器额定值时）`V_belt_max`

### Q9. 阀响应时间实测
- DF8 电磁阀的"通电→出气延时"和"断电→停止延时"
- 不同压力（0.5 / 0.6 / 0.7 MPa）下的响应曲线
- **实测方法**：高速摄像 + 通断信号示波器
- **里程碑**：Phase 7 标定阶段

### Q10. 探测器视窗污染速率
- 多久需要吹一次？
- "吹粉尘电磁阀"的合理触发周期？
- **里程碑**：现场试运行后定

## 低 P3（产品化时考虑）

- Q11. 主回路电源接入：380V 三相五线，是否带零线？
- Q12. 接地阻抗、防雷
- Q13. 矿井环境的防爆等级要求（ExdII Bt4？）
- Q14. 工业 PC 选型（推荐研华 / 凌华，IP54 以上）
- Q15. 远程运维通道（4G 网关 / VPN）

---

## 协议获取的标准动作

每收到一份协议文档：

1. 把 PDF/Word 放到 `docs/hardware/protocols/`（**不入 Git，含厂商版权**）
2. 在 `docs/hardware/protocol-{name}.md` 写一份提炼后的工程笔记
3. 在 `hardware/{interface}/` 写一份测试程序，先用串口工具或单次脚本验证关键命令
4. 验证通过后再写 C++ 实现

---

## 当前未解决事项（按优先级）

| 优先级 | 任务 | 预估工时 | 阻塞 |
|---|---|---|---|
| P1 | Q6 变频器铭牌 → 确认是否走 EtherCAT | 0.5 天 | 现场调试 |
| P2 | Q8 几何参数实测（探测中心到喷嘴距离） | 1 天 | 时序标定 |
| P2 | Q9 DF8 阀响应时间实测 | 1 天 | 时序标定 |
| P2 | Q10 探测器视窗吹扫周期 | 现场观察 | 维护策略 |
| P3 | Q3 工业相机（设备未到货） | 设备到货时 | — |

所有软件 P0 项已完成，当前无阻塞项。
