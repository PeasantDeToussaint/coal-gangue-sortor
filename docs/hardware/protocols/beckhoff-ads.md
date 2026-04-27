# Beckhoff TwinCAT 3 / ADS 协议接入

> **设备**：Beckhoff FC9022 PCIe 主站 + EK1501 耦合器 + EL2828 ×20 + EL9410 ×4 + EL6070 + TwinCAT 3 Runtime  
> **协议**：ADS (Automation Device Specification) over TCP/IP，端口 48898  
> **状态**：✅ 协议成熟稳定，使用官方开源 C++ 库

## 通信架构

```
┌──────────────── 工控机 (Windows 或 Linux 上的 TwinCAT/BSD) ──────────────┐
│                                                                              │
│  ┌─────────────────┐         ADS         ┌──────────────────────────┐      │
│  │  Coal-Gangue    │ ◄──── TCP:48898 ──► │  TwinCAT 3 PLC Runtime   │      │
│  │  Qt App (我们)   │                     │  (TC1100-0291)           │      │
│  └─────────────────┘                     └──────────────────────────┘      │
│                                                       │                      │
│                                                       │ EtherCAT 1ms 周期    │
│                                                       ▼                      │
└──────────────────────────────────────────────────────┬───────────────────────┘
                                                       │
                                                  FC9022 PCIe 卡
                                                       │
                              ┌────────────── EtherCAT 主站光纤 ──────────────┐
                              │                                                │
                              ▼                                                │
                          EK1501 (耦合器)                                       │
                              │                                                │
        ┌─────────────────────┼─────────────────────────────────────────┐     │
        │ EL9410 (24V 电源)    │ EL2828 ×20 (160路 DO)  │ EL6070 (许可)  │     │
        │                      │  ├── DO 1..64 → 气枪阀 (DF8 × 32)       │     │
        │                      │  ├── DO 65..72 → 皮带启停 (×6)          │     │
        │                      │  ├── DO 73..80 → 三色灯 / 蜂鸣器        │     │
        │                      │  ├── DO 81..96 → 变频器使能 / 故障复位 │     │
        │                      │  └── DO 97..160 → 余量                  │     │
        │                      │                                          │     │
        │                      │ DI 端子 (从EL1xxx读)                    │     │
        │                      │  ├── DI 1 → 急停反馈                     │     │
        │                      │  ├── DI 2 → 光电触发                     │     │
        │                      │  ├── DI 3..8 → 各电机运行/故障         │     │
        │                      │  └── ...                                 │     │
        └─────────────────────────────────────────────────────────────┘     │
                                                                             │
                                                                            ─┘
```

## 上位机软件方案

### 推荐：Beckhoff 官方开源 ADS C++ 库

- 仓库：https://github.com/Beckhoff/ADS
- 许可：MIT
- 跨平台：Windows / Linux（macOS 可编但需禁用 Windows-only 部分）
- 不依赖 TwinCAT 安装包（直接 TCP 通信）

```bash
git clone https://github.com/Beckhoff/ADS third_party/ads
mkdir build && cd build
cmake -B build -DADS_BUILD_EXAMPLES=OFF
cmake --build build
# 产出 libAdsLib.a
```

### 关键概念

| 概念 | 说明 | 示例 |
|---|---|---|
| **AMS Net ID** | TwinCAT 设备的 6 字节标识 | `5.45.22.57.1.1` |
| **AMS Port** | 服务端口（PLC Runtime = 851） | `851` |
| **Variable Handle** | 通过名字解析得到的变量句柄 | uint32 |
| **PLC 变量名** | `MAIN.bConveyorRun` | string |
| **Symbol Table** | 项目编译时导出的变量表 | `.tpy` 文件 |

### 典型读写流程

```cpp
#include <AdsLib.h>

// 1. 路由表加上 PLC 的 AMS Net ID（一次性）
const AmsNetId remoteNetId{5,45,22,57,1,1};
const std::string remoteIp = "192.168.1.10";
AdsAddRoute(remoteNetId, remoteIp.c_str());

// 2. 打开 ADS 连接
AdsDevice route(remoteIp, remoteNetId, 851);

// 3. 通过名字解析变量句柄
auto handle = route.GetHandle("MAIN.bConveyorRun");

// 4. 写值
uint8_t v = 1;
route.WriteReqEx(ADSIGRP_SYM_VALBYHND, *handle, sizeof(v), &v);

// 5. 读值
uint8_t out;
uint32_t bytes;
route.ReadReqEx2(ADSIGRP_SYM_VALBYHND, *handle, sizeof(out), &out, &bytes);

// 6. 订阅变化通知（用于触发回调，比轮询高效）
auto notif = route.AddDeviceNotification(...);
```

## 在新仓库中的实现规划

`hardware/PLCInterface/PLCBeckhoff.h/.cpp`：实现 `IPLC` 接口。

### 编译开关

```bash
cmake -B build \
  -DCGS_HAS_BECKHOFF_ADS=ON \
  -DCGS_BECKHOFF_ADS_DIR=third_party/ads
```

### 变量映射约定（与 PLC 工程师对齐）

PLC 端要求暴露这些 GVL 全局变量（命名规范一致即可，下面给推荐名）：

```pascal
// GVL_Coal.var
VAR_GLOBAL
    // 输出（上位机写）
    bConveyorFeeder        : BOOL;       // 上料带启停
    bConveyorVibrator      : BOOL;       // 振动分筛
    bConveyorPowder        : BOOL;       // 粉末料带
    bConveyorDetection     : BOOL;       // 检测带
    bConveyorAccepted      : BOOL;       // 合格料带
    bConveyorReject        : BOOL;       // 废料带
    rDetectionBeltSpeedHz  : REAL;       // 检测带变频频率给定
    bLampGreen             : BOOL;       // 三色绿灯
    bLampYellow            : BOOL;       // 黄灯
    bLampRed               : BOOL;       // 红灯
    bBuzzer                : BOOL;       // 蜂鸣器

    // 输入（上位机读）
    bEmergencyStop         : BOOL;       // 急停状态（true=按下）
    bAirPressureOk         : BOOL;       // 气源压力正常
    bXRayInterlockOk       : BOOL;       // X射线互锁回路通
    rDetectionBeltActualHz : REAL;       // 实际频率反馈
    bConveyorFeederFault   : BOOL;       // 各电机故障位
    // ... 其他故障/状态

    // 事件（上位机订阅 ADS 通知）
    bMaterialTrigger       : BOOL;       // 光电触发上升沿
END_VAR
```

变量名不重要，重要的是**新仓库代码端把这个映射独立成一份配置文件**，将来 PLC 工程师改名只改配置不改代码。

## 待 PLC 端工程师确认的事项

- [ ] AMS Net ID 与 IP（让对方提供 SystemManager → Routing 截图）
- [ ] 变量是否在 GVL（GlobalVarList）暴露并设为 `VAR_GLOBAL` + 编译选项 `Persistent`?
- [ ] EL2828 物理通道与 PLC 变量的对应表（可能附在 IO 配置 `.xti`）
- [ ] 变频器是否挂在 EtherCAT 总线上（用 EL6xxx 网关），还是独立 RS485 给 PLC
- [ ] 急停回路是否硬接到 EL1xxx DI（非纯软件锁存）

## 测试

无 PLC：用 `PLCMock` 跑全流程（已实现）  
有 PLC 无硬件：在 PLC 工程里写一个空 `MAIN`，只放上面那些 GVL 变量，运行 PLC 即可联调  
有完整硬件：现场调试，按 `docs/hardware/site-survey-checklist.md` 逐项确认
