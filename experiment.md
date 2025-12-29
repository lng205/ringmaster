# 实验一：端到端实时视频传输中 RLNC-FEC 对冻结/延迟的改善（含裸 UDP 对比）

## 1. 目标
在存在 RTT（网络时延）的情况下，对比以下方案在不同丢包率下的：
- 冻结（卡顿/停顿）是否显著减少
- 帧可解码延迟是否降低
- 为此付出的带宽代价（冗余/重传）是多少

核心结论预期：
- 裸 UDP：丢包直接导致帧缺失/跳帧，冻结最严重或画面断续
- ARQ-only：能恢复但要等 RTT，冻结时长与 RTT 强相关
- RLNC+ARQ：多数丢包由 FEC 在 0RTT 内恢复，冻结显著减少

---

## 2. 对照组（4 组）
所有组使用相同视频参数（分辨率/fps/码率/帧内切片逻辑一致），仅恢复机制不同。

| 组别 | 名称 | FEC 冗余度 R | ARQ 重传 |
|---|---|---:|---|
| G0 | 裸 UDP | 0 | 关闭 |
| G1 | ARQ-only | 0 | 开启 |
| G2 | RLNC+ARQ (R=0.1) | 0.1 | 开启 |
| G3 | RLNC+ARQ (R=0.2) | 0.2 | 开启 |

> 备注：如果实现上“关闭 ARQ”不方便，G0 可用“只发 DATA，不 ACK 不重传”的方式实现；确保无 FEC、无重传。

---

## 3. 网络仿真（tc netem）
使用 Linux `tc netem` 注入丢包和时延。每次实验开始前设置，结束后清除。

### 3.1 推荐命令（避免重复 add 报错）
```bash
# 设置 loss + delay（直接覆盖）
sudo tc qdisc replace dev lo root netem loss ${LOSS}% delay ${DELAY}ms

# 查看
tc qdisc show dev lo

# 清除
sudo tc qdisc del dev lo root 2>/dev/null || true
````

---

## 4. 实验变量与点位（两阶段：曲线 + 敏感性）

为保证图好看且不爆炸，使用“关键曲线图点多 + 其他维度点少”的设计。

### 4.1 主曲线：固定 delay=50ms，扫 loss（画曲线）

* delay：50ms
* loss：0%, 5%, 10%, 15%, 20%（5 点）
* 方案：G0/G1/G2/G3（4 条线）
* 组数：5 × 4 = 20 组

### 4.2 延迟敏感性：固定 loss=10%，扫 delay（画分组柱状图）

* loss：10%
* delay：0ms, 50ms, 100ms（3 点）
* 方案：G0/G1/G2/G3
* 组数：3 × 4 = 12 组

### 4.3 重复次数（建议）

每组重复 N=3（取平均值；可给误差条）。如时间紧，至少 N=2。

* 总运行次数（N=3）：(20+12)*3=96 次（仍可接受）
* 时间特别紧：只对主曲线做 N=3，敏感性做 N=1~2

---

## 5. 固定参数（建议写入论文）

* 视频：`1920x1080`（或你当前默认）/ `fps=24` / `cbr=2000kbps`（按你系统现状）
* MTU / 分片策略：按系统默认；保证各组一致
* ARQ 参数：最大重传 3 次；重传间隔 >= 1 RTT；1 秒后强制关键帧（保持你现在逻辑）

---

## 6. 指标定义（必须严格、可复现）

为避免“冻结次数”争议，主用“冻结占比/冻结总时长”。

### 6.1 帧可解码率（Frame Decoding Ratio）

[
P_{dec} = \frac{N_{decoded}}{N_{total}}
]

* N_total：发送的帧数（或接收端观测到的帧 ID 数）
* N_decoded：接收端成功拼接并输出的帧数

### 6.2 帧可解码延迟（Frame Decode Delay）

对每帧 i：
[
D_i = t^{(rx)}*{dec,i} - t^{(tx)}*{send,i}
]

* (t^{(tx)}_{send,i})：发送端写入的帧发送时间戳（你协议已有 48-bit send timestamp）
* (t^{(rx)}_{dec,i})：接收端“该帧可解码”的本地时间（完成拼帧/解码前一刻）
  输出：
* 平均延迟：mean(D_i)
* （可选）p95：p95(D_i)（更能体现尾部冻结）

### 6.3 冻结阈值与冻结时长（Stall）

设播放阈值：
[
T_{freeze} = \frac{2}{fps}
]

* fps=24 时，T_freeze≈83ms

对每帧 i 的冻结贡献：
[
S_i = \max(0, D_i - T_{freeze})
]
输出：

* 冻结总时长：(\sum_i S_i)
* 冻结占比（推荐）：(\frac{\sum_i S_i}{T_{play}})

  * (T_{play}) 可取实验播放时长（例如 60s）

### 6.4 带宽代价（Overhead）

建议至少给一个简单口径（便于写结论）：

* repair 包比例：repair_packets / total_packets
* 或总发送字节数：tx_bytes_total（含重传、含 repair）

---

## 7. 运行步骤（每组实验的标准流程）

### 7.1 准备

* 选择固定视频输入（同一文件）
* 约定每组运行时长：60s（或固定帧数，例如 24fps × 60 = 1440 帧）
* 清理旧 tc 配置：`sudo tc qdisc del dev lo root ...`

### 7.2 设置网络

```bash
sudo tc qdisc replace dev lo root netem loss ${LOSS}% delay ${DELAY}ms
```

### 7.3 启动接收端（先启动）

示例（按你现有参数）：

```bash
./build/receiver 127.0.0.1 12345 1920 1080 --fps 24 --cbr 2000 --lazy 2 -v \
  --log rx_${SCHEME}_loss${LOSS}_d${DELAY}.log
```

### 7.4 启动发送端

* 裸 UDP（G0）：关闭 FEC、关闭 ARQ
* ARQ-only（G1）：R=0，开启 ARQ
* RLNC+ARQ（G2/G3）：R=0.1/0.2，开启 ARQ

示例（你现有命令风格）：

```bash
# G2：R=0.1
./build/sender 12345 video.y4m -R 0.1 -F -v \
  --log tx_G2_loss${LOSS}_d${DELAY}.log
```

### 7.5 结束与清理

* 结束 sender/receiver
* 清除 tc：

```bash
sudo tc qdisc del dev lo root 2>/dev/null || true
```

---

## 8. 日志要求（最小字段集合）

为保证能算指标，每帧至少记录以下字段（建议 CSV 一行一帧）：

接收端（必须）：

* frame_id
* t_send_ms（从包头取该帧发送时间戳；或该帧首包的 send timestamp）
* t_dec_ms（该帧“判定 complete 并可解码”的本地时间）
* decoded_ok（0/1）
* used_fec_pkts（该帧解码中使用到的 repair 包数量，可选）
* arq_retx_pkts（该帧相关的重传包数量，可选）

发送端（可选但建议）：

* tx_bytes_total
* tx_packets_total
* tx_repair_packets
* tx_retrans_packets

> 若你暂时没有 per-frame 的 used_fec/arq_retx，也不影响实验一结论；先保证 D_i、decoded_ok、tx_bytes 能输出。

---

## 9. 数据处理与画图（如何画图）

推荐用 Python（matplotlib）或 gnuplot。最终至少输出 3 张图 + 1 个表。

### 9.1 计算流程（从日志到指标）

对每个实验点（scheme, loss, delay）：

1. 从 rx log 读取所有帧
2. 过滤 decoded_ok=1 的帧计算 D_i
3. 计算：

   * P_dec
   * mean(D_i)（可选 p95）
   * S_total = sum(max(0, D_i - T_freeze))
   * StallRatio = S_total / T_play
4. 从 tx log 读取总发送字节/包数（如有），计算 overhead

### 9.2 图 1（主曲线，最重要）

**冻结占比 vs 丢包率（delay=50ms）**

* x：loss（0,5,10,15,20）
* y：StallRatio（或冻结总时长 ms）
* 4 条线：G0/G1/G2/G3
* 说明文字：R 增加带来的冻结下降趋势；ARQ-only 在高 loss 下冻结明显上升

### 9.3 图 2（主曲线配套）

**平均帧可解码延迟 vs 丢包率（delay=50ms）**

* 同上 x
* y：mean(D_i)（可选加 p95 的虚线或误差条）
* 预期：ARQ-only 随 loss 增长明显，RLNC+ARQ 更平缓

### 9.4 图 3（敏感性柱状图）

**冻结占比 vs 时延（loss=10%）**

* x：delay（0,50,100）
* 每个 delay 下 4 根柱：G0/G1/G2/G3
* 预期：delay 越大，ARQ-only 越差；RLNC 相对收益越大

### 9.5 表 1（带宽代价）

给一张小表（delay=50ms，loss=10% 和 20% 两行即可）：

* scheme
* tx_bytes_total 或 repair% / retrans%
* StallRatio
* mean(D_i)

---

## 10. 结果撰写要点（可直接粘进论文）

* 裸 UDP 作为下界：丢包直接导致帧缺失，冻结/断续明显
* ARQ-only 在存在 RTT 的网络中，恢复主要依赖重传，冻结时长与 RTT 同量级增长
* RLNC+ARQ 在中等丢包下，大部分丢失包可在 0RTT 内恢复，显著降低冻结占比，并降低延迟尾部（p95）
* RLNC 的收益随 delay 增大而增大；在 delay≈0 时与 ARQ 差异较小（与预期一致）
* 代价：引入 repair 包带宽开销（用表 1 量化“多花多少带宽换来冻结下降”）

---

## 11. 交付清单（本实验最终产出）

* 图 1：StallRatio vs loss（delay=50ms，4 条线）
* 图 2：mean decode delay vs loss（delay=50ms，4 条线）
* 图 3：StallRatio vs delay（loss=10%，分组柱状）
* 表 1：带宽代价与效果（选取代表点）
* 一段结论文字（按第 10 节要点）

