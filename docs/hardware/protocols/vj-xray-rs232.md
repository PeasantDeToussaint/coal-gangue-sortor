# VJ Technologies X 射线源串口协议（已破解）

> **来源**：太原泓博现有金万利测厚机项目 `XRayLib/XRayLib.cpp`，已实测可用。  
> **设备**：VJ Technologies 系列 X 射线发生器（IXS200BP500P479 等）  
> **官方文档**：`P032-IXS-FIRMWARE-P032 R5`（向 VJ 索取，本协议是该文档的子集）  
> **状态**：✅ 完整破解，新仓库 `XRaySerial` 真实驱动直接基于此实现

## 物理层

| 参数 | 值 |
|---|---|
| 接口 | RS232（VJ 设备 J3 端口，DB9 母口） |
| 波特率 | **9600** |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | None |
| 流控 | None |

引脚（VJ J3）：Pin 2 = TX-，Pin 3 = RX+，Pin 5 = SIGNAL GND。  
**注意**：J2 联锁口 Pin 1 & Pin 2 必须通过外部安全开关短接，否则高压无法启辉。

## 帧格式

每条命令格式：

```
STX (0x02) + 命令文本 + CR (0x0D)
```

每条响应格式：

```
STX (0x02) + 数据文本 + CR (0x0D)
```

C++ 表达：

```cpp
QString cmd = QString("\x02ENBL1\r");        // 开高压
m_serialPort->write(cmd.toUtf8());
```

## 命令清单

| 命令 | 含义 | 返回 |
|---|---|---|
| `WDOG0` / `WDOG1` | 看门狗 关 / 开 | 无 |
| `PTM<sec>` | 设置预热时间，单位秒（4 位补零，如 `PTM0030`） | 无 |
| `ENBL1` / `ENBL0` | 高压使能 开 / 关（=出射线 / 停射线） | 无 |
| `VP<v*10>` | 设置管电压，单位 kV ×10，4 位补零（如 80.0 kV → `VP0800`） | 无 |
| `CP<uA>` | 设置管电流，单位 μA，4 位补零（注意：旧设备最大 700 μA，新设备 IXS200BP500P479 最大 2500 μA） | 无 |
| `CLR` | 清除所有故障 | 无 |
| `FLT` | 查询故障字 | `<STX>X0 X1 X2 X3 X4 X5 X6 X7 X8<CR>`，9 位空格分隔，0=正常 / 1=故障 |
| `MON` | 监控当前 kV / μA / 温度 | `<STX>kVkV.k uAuA TT.T<CR>`，~21 字节 |
| `STAT` | 高压状态 | `<STX>0<CR>`=关，`<STX>1<CR>`=开 |
| `PSTAT` | 预热状态 | `<STX>0<CR>`=非预热，`<STX>1<CR>`=预热中 |

### 故障字位定义（来自 FLT 响应）

| Bit | 含义 |
|---|---|
| X0 | Regulation Fault（kV/mA 失稳） |
| X1 | Interlock Open（联锁打开 / 安全门没关） |
| X2 | KOV（阴极过电压） |
| X3 | AOV（阳极过电压） |
| X4 | Over Temperature（油温过高，57–63°C 切断） |
| X5 | Arc Detection（弧放电，10s 内多次锁定） |
| X6 | Over Current（过电流） |
| X7 | Power Limit（功率超出额定） |
| X8 | Over Voltage（过电压，210–220 kV 切断） |

任一位为 1 都会自动切断高压，必须发 `CLR` 才能恢复。

## 典型流程（开机 → 出射线）

```
1. 打开串口 9600 8N1
2. 发 WDOG0 关闭看门狗
3. 发 PTM0030 设置预热时间 30 秒
4. 等待操作员关闭安全门（互锁回路通）
5. 发 ENBL1 开高压
6. 进入 1 Hz 状态轮询循环：
     发 FLT，解析故障字
     若无故障 → 发 MON 取 kV/uA/T
     发 STAT 取开关状态
     发 PSTAT 取预热状态
7. 用户调参：VP / CP（在线可改）
8. 关机：发 ENBL0 关高压，发 WDOG1 启动看门狗
```

## 旧代码经验记录

| 经验 | 说明 |
|---|---|
| 5000ms 信号量超时 | Qt 信号槽跨线程调用，5s 不响应认为失败 |
| 1 Hz 状态轮询 | 高于这个频率，串口处理不过来 |
| 需要看门狗管理 | 长时间不通信会自动关高压保护 |
| 响应必须以 `\r` 结尾 | 否则要继续 readyRead 拼接 |
| 所有数值都是 ASCII | 不是二进制，调试方便 |

## 在新仓库中的实现

`hardware/XRayInterface/XRaySerial.h/.cpp` 实现 `IXRaySource` 接口，直接照搬本协议。  
为了减少 Qt 依赖，新版用 **POSIX termios**（macOS/Linux）+ Win32 `CreateFile`（Windows）的小型跨平台串口封装 `core/Serial/SerialPort.h`，避免拉入 `QtSerialPort`。

测试方式：
- 没硬件 → 用 `socat -d -d pty,raw,echo=0 pty,raw,echo=0` 创建虚拟串口对 + 写一个 Python 脚本扮演 X 射线源响应。
- 有硬件 → 现场用 USB-RS232 转换器直连 VJ 控制盒 J3 口。

## 数值转换示例

```
80.0 kV  → "VP0800"     格式: VP + (kv * 10) 取整 + 4位补零
1500 uA  → "CP1500"     格式: CP + uA 取整 + 4位补零
30 sec   → "PTM0030"    格式: PTM + sec 取整 + 4位补零
```
