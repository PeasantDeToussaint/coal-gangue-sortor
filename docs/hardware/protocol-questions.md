# 待解决的协议与厂商资料清单

> 这是软件复刻最大的几块未知。每一项都建议建一个 GitHub issue，分配给负责人。

## 紧急 P0（不解决软件无法上线）

### Q1. AnySystem FaDriver-64 串口协议
- **板卡型号**：`AnySystem_24V FaDriver-64 Ver1.8`
- **接口**：2 × DB9（COM1, COM2）+ 24V 电源
- **作用**：64 路高速电磁阀驱动，串口下命令、板载完成精确时序
- **未知**：
  - 波特率 / 校验位 / 停止位
  - 命令包格式（帧头/长度/通道掩码/触发时间/校验）
  - 是否支持"未来某时刻触发"的预约模式
  - 是否回执 / 心跳 / 错误码
- **获取途径**：
  1. 找供应商（板上印有"AnySystem"商标，AGM FPGA 标）→ 索要 `通信协议手册.pdf`
  2. 找原版上位机程序，串口抓包（推荐工具：CommMaster、串口大师 macOS 版）
  3. 拆解固件（最后选项，需 JTAG）
- **里程碑**：拿到协议后 → Phase 3 实现 `ValveDriverSerial`

### Q2. ✅ 已解决！X 射线源型号已确认

**VJ Technologies IXS200BP500P479**（美国，规格文件 SPC-P479 REV3）

- 控制接口：**RS232（J3，9针母口，Pin2=TX-，Pin3=RX+，Pin5=GND）**
- 协议文档：**P032-IXS-FIRMWARE-P032 R5**（VJ Technologies 内部文件，需索取）
- 联锁：J2 Pin1&Pin2 必须短接才能出射线（安全门开关串联）
- 最大参数：100–200 kV，0.2–2.5 mA，500 W

**下一步**：
1. 联系 VJ Technologies（www.vjtechnologies.com / +1-631-981-7100）索取 P032-IXS-FIRMWARE-P032 R5
2. 或在旧项目代码中找 XLibDll 的 RS232 帧格式（可能是泓博自己封装的）
3. 放入 `docs/hardware/protocols/vj-ixs-firmware-p032-r5.md`

**里程碑**：Phase 4

### Q3. 凌云光相机型号
- **照片可见**：银色金属外壳，长方形视窗，散热鳍片+风扇，蓝色 GigE 网线
- **未知**：
  - 具体型号（可能是 LBAS-… 区域阵 / LBSC-… 线阵）
  - 接口（GigE Vision 居多，也可能 CameraLink）
  - 像素 / 帧率 / 行频
  - SDK（凌云光 LDV、海康 MVS、Basler Pylon、或纯 GenICam）
- **获取途径**：
  1. 现场拍铭牌（QR码扫码）
  2. 联系销售（合同号 / 发货单）
- **里程碑**：Phase 4

### Q4. 光源 "光筒1100" 控制
- **铭牌**：`V2-12-638-520-450-200-0.22-1.5  SN:00232`
- **未知**：是否需要软件控制亮度 / 触发同步
- **获取途径**：扫描铭牌 QR 码；找销售
- **里程碑**：Phase 4

## 高 P1（影响系统集成）

### Q5. ✅ 已解决！控制系统是 Beckhoff TwinCAT 3 + EtherCAT（不是传统 PLC！）

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
- 默认端口：48898（ADS）
- 开源库：[ADS C++ 库](https://github.com/Beckhoff/ADS) / [TwinCAT.Ads NuGet]
- 文档：[infosys.beckhoff.com](https://infosys.beckhoff.com/content/1033/tc3_ads_intro/index.html)
- 关键概念：AMS Net ID / Port / Variable handle / Read-Write

**里程碑**：Phase 5 → 用 Beckhoff ADS 库替换 Modbus，连接 TwinCAT 读写 EL2828 DO

### Q6. 变频器品牌与寄存器
- **现场**：检测带 4 kW 变频器（可能也挂在 EtherCAT 总线上，或 Modbus 独立）
- **注意**：有了 TwinCAT，变频器可能通过 EL6xxx 总线网关连接，而非直接 Modbus
- **获取途径**：拍铭牌 + 查厂家手册；看 TwinCAT 项目源码（如果能拿到的话）
- **里程碑**：Phase 5

### Q7. 探测器线阵驱动
- **现场**：黑色长条线阵，约 1.1 m
- **旧项目用过**：`XLibDll.lib`（Windows 闭源）
- **未知**：是否同型号？行频？通道数？
- **获取途径**：现场看探测器型号 + 找配套上位机
- **里程碑**：Phase 4

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

## 优先级排序（执行顺序建议）

| 优先级 | 任务 | 预估工时 | 阻塞 |
|---|---|---|---|
| P0 | Q1 拿到 FaDriver-64 协议 | 1–7 天 | Phase 3 |
| P0 | Q3 拍凌云光相机铭牌 | 0.5 天 | Phase 4 |
| P0 | Q2 拍 X 射线源铭牌 | 0.5 天 | Phase 4 |
| P0 | Q4 扫光源 QR 码 | 0.5 天 | Phase 4 |
| P1 | Q5 拍 PLC 铭牌 | 0.5 天 | Phase 5 |
| P1 | Q6 拍变频器铭牌 | 0.5 天 | Phase 5 |
| P1 | Q7 确认探测器型号 | 0.5 天 | Phase 4 |
| P2 | Q8 几何参数实测 | 1 天 | 标定 |
| P2 | Q9 阀响应实测 | 1 天 | 标定 |

---

> 软件这边**等不起 P0**，但可以**先做 Mock 与算法**，等协议到位直接接入。  
> 这正是 Phase 0–2 的设计原因。
