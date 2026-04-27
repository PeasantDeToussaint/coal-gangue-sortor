# 离线工具（Phase 7 占位）

这些 Python 脚本会在 Phase 7 实现，目的是让现场调试不依赖 GUI：

- **`calibrate_belt.py`** — 测量"变频器频率 → 实际线速度"曲线  
  操作员在皮带上放标定块，多次触发记录通过两个光电的时间差；脚本拟合出 Hz↔m/s 关系并写入 `config.xml`。

- **`replay_session.py`** — 回放历史采集的 `.bin` 数据  
  把现场录制的 X射线/相机帧重放进算法，用于在办公室无硬件环境调阈值。

- **`timing_simulator.py`** — 喷嘴延迟仿真器  
  输入皮带速度、几何距离、阀响应时间，画出推荐触发延迟与告警边界。

- **`nozzle_self_test.py`** — 64 路喷嘴单点测试  
  逐个喷嘴吹气 200 ms，操作员目视/听声确认是否全部正常。第一次现场调试必跑。

## 运行环境

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install numpy matplotlib pyserial pyyaml
```
