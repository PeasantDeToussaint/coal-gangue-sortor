# AnySystem FaDriver-64 串口协议（待破解）

> **设备**：64 通道高速电磁阀驱动板  
> **铭牌**：`AnySystem_24V FaDriver-64 Ver1.8`  
> **接口**：2 × DB9（COM1/COM2）+ 24V DC 电源  
> **状态**：❌ **协议尚未确认** — Phase 3 待现场抓包或厂家资料

## 目前已知

来自照片观察：

- 板上标识：`AnySystem_24V`、`FaDriver-64 Ver1.8`、`AGM`（FPGA 主控）
- 输出端子分组：`Fa1-1` `Fa8-9` `Fa16-17` `Fa24-25` `Fa32-33` `Fa40-41` `Fa48-49` `Fa56-57` `Fa64`（8 组，每组 8 路）
- 两个 DB9 接口 `QianShi`（前事？） `HouShi`（后事？）— 中文拼音标签
- 24V 电源端子位于左下角，标 `+24V` `GND`
- 配套：32 个 DF8-2024-10 电磁阀（每个 4 路）

## 重要发现：可能可以绕过

**Beckhoff EL2828 × 20 = 160 路 24V/2A DO**，足以直接驱动 64 路电磁阀。  
气枪本来就需要的微秒级时序由 EtherCAT 1ms 周期 + EL2828 通道延时寄存器实现。

**两个最可能的部署方案：**

### 方案 A：FaDriver-64 直驱（设计文档原意？）

```
PC → COM 串口 → FaDriver-64 → DF8 电磁阀 → 气枪
```

**优点**：FaDriver-64 自带 FPGA，时序精度可能 < 0.1 ms  
**缺点**：协议未知，需要破解

### 方案 B：Beckhoff EL2828 直驱（更现代）

```
PC → ADS → TwinCAT → EtherCAT → EL2828 → DF8 电磁阀 → 气枪
```

**优点**：协议成熟，开源库现成；统一在 Beckhoff 生态  
**缺点**：EtherCAT 周期 1 ms（仍能满足绝大多数分选场景）  
**佐证**：采购清单里 EL2828 ×20 = 160 路远超气枪需要，**很可能就是直驱设计**

### 方案 C：两者并存（冗余 / 时序保险）

FaDriver-64 处理超高速时序，Beckhoff 处理普通 I/O。

## 现场调查清单

到现场后请完成：

- [ ] 拍清楚 FaDriver-64 串口接到了哪台设备（PC？还是 PLC？）
- [ ] 拍清楚气枪线缆走向：从 DF8 出来的线接到 FaDriver-64 还是 EL2828？
- [ ] 看 PC 上是否安装 AnySystem 配套上位机软件，**是的话用串口抓包工具捕获协议**
- [ ] 板边丝印 / 不干胶 / 二维码上的厂家联系方式
- [ ] 观察板子上电后的 LED 行为（是否周期性闪烁 = 心跳）

## 抓包步骤（一旦拿到 PC + 原版上位机）

1. **物理层**：USB 串口分线器（如 FTDI USB-RS232 + 一拖二适配器）插在 PC 与 FaDriver 之间
2. **软件**：
   - macOS：`screen /dev/cu.usbserial-XX 9600` + `tee` 记录
   - Windows：[`Realterm`](https://realterm.sourceforge.io) / [`HHD Free Serial Port Monitor`](https://freeserialanalyzer.com/)
   - Linux：`socat -d -d /dev/ttyUSB0,raw,echo=0 PIPE:/tmp/cap`
3. **测试用例**：
   - 板上电不操作 → 看是否有心跳包
   - 上位机点击"喷嘴 1 测试" → 抓到的字节就是单路命令
   - 点击"喷嘴 1 + 5 + 7 同时触发" → 多路命令
   - 修改触发延时 → 看哪几个字节变
4. **解析**：把抓到的二进制保存到 `docs/hardware/protocols/captures/`，写 Python 解析脚本

## 假设的协议结构（基于业内常见做法）

如下是**纯猜测**，等抓包确认：

```
帧格式（猜测）：
  STX (0x55 / 0xAA / 0xFA / 0xFD ?)
  CMD  (1 byte: 0x01=fire, 0x02=status, 0x03=config, ...)
  LEN  (1-2 bytes)
  DATA (LEN bytes)
  CRC  (1-2 bytes, sum or CRC16)
  ETX  (0x0A or 0x0D ?)

单路触发命令（猜测）：
  STX, CMD=0x01, LEN, ChannelMask(8 bytes), DelayUs(2-4 bytes), DurationMs(2 bytes), CRC, ETX

通道掩码（猜测）：
  64 路用 8 字节位掩码：bit 0..63
  例如 喷嘴 1 + 5 + 7 → 0x55 0x00 0x00 ... = bin 01010101
```

## 实现策略

新仓库已有 `ValveDriverMock` 实现完整逻辑。**`ValveDriverFaDriver64` 的真实实现暂不写代码**，只在文档中保留协议假设。原因：

1. 协议未确认，写出来的代码 100% 错
2. EL2828 直驱（方案 B）可能根本不需要这块板
3. 等现场抓包后，**1 天内可补完真实驱动**（参照 `XRaySerial` 的实现风格，复用 `core/Serial/SerialPort`）

## 何时回来填这个文档

- 现场调试日 → 抓包 → 把字节流贴到本文档"已抓获的命令"章节
- 厂家资料到手 → 把官方协议表贴到"协议表"章节
- 真实驱动写完 → 把"实现策略"改为"已实现"，附测试结果

## 已抓获的命令（待填）

```
TODO: 现场抓包后填入
```

## 协议表（待填）

| 命令 | 字节序列 | 含义 | 响应 |
|---|---|---|---|
| ? | ? | ? | ? |
