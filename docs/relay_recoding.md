# 多跳 RLNC 中继系统

## 1. 架构

```
Sender:12345 ──DATA/REPAIR──> Relay:12346 ──DATA/REPAIR──> Receiver
             <────HOP_ACK────             <────HOP_ACK────
             <──────────────ACK──────────────────────────
```

| 组件 | 职责 | 端口角色 |
|------|------|----------|
| Sender | 视频编码 + RLNC 系统码编码 | 监听，等待下游连接 |
| Relay | 透传 + RLNC 重编码 | 连接上游，监听下游 |
| Receiver | RLNC 解码 + 视频解码播放 | 连接上游 |

## 2. 协议

### 2.1 消息类型

| 类型 | 值 | 方向 | 用途 |
|------|---|------|------|
| DATA | 0 | 下行 | 原始数据包，系数为单位向量 |
| REPAIR | 1 | 下行 | 冗余包，系数为随机向量 |
| ACK | 1 | 上行 | 端到端确认，用于 Sender 的 RTT 估计和 ARQ 重传 |
| CONFIG | 2 | 上行 | Receiver 发送的视频参数（分辨率、帧率、码率） |
| HOP_ACK | 3 | 上行 | 逐跳确认，仅传递到上一跳，用于测量单链路丢包率 |

### 2.2 Datagram 格式

```
| frame_id (4B) | frame_type (1B) | fec_type (1B) | frag_id (2B) |
| frag_cnt (2B) | padding (2B) | send_ts (8B) | payload (变长) |
```

- `frame_id`: 视频帧序号
- `frame_type`: KEY(1) 或 NONKEY(2)
- `fec_type`: DATA(0) 或 REPAIR(1)
- `frag_id`: 分片序号
- `frag_cnt`: 该帧总分片数 k
- `padding`: 最后一个分片的填充字节数
- `send_ts`: 发送时间戳（微秒）

### 2.3 Payload 结构

```
| 编码系数 (k 字节) | 数据块 (max_payload - k 字节) |
```

DATA 包的系数为单位向量（第 i 个 DATA 包的系数为 eᵢ），REPAIR 包的系数为随机向量。

### 2.4 HOP_ACK 格式

```
| type (1B) = 3 | frame_id (4B) | frag_id (2B) |
```

总长度：7 字节。

## 3. 秩感知重编码（Rank-Aware Recoding）

### 3.1 设计目标

在中继节点生成额外冗余包，对抗下游链路的丢包。通过秩判断决定何时可以重编码，即使部分 DATA 包丢失，只要收到的包（DATA + REPAIR）秩达到 k，仍可生成有效的重编码包。

### 3.2 触发条件

当 Relay 收到的包的系数矩阵秩达到 k 时触发重编码，生成 `floor(k × R)` 个重编码包。

与旧设计的对比：

| 方面 | 旧设计 | 新设计（秩感知） |
|------|--------|------------------|
| 触发条件 | 收齐 k 个 DATA 包 | 收到的包秩 ≥ k |
| 缓存范围 | 仅 DATA | DATA + REPAIR |
| 上游丢包容忍 | 不容忍 | 可容忍（只要秩够） |

### 3.3 秩检查算法

对系数矩阵进行 GF(2⁸) 高斯消元，统计主元数量：

```cpp
bool check_rank(const vector<vector<uint8_t>>& coeffs, int k) {
  auto matrix = coeffs;  // 复制
  int pivot = 0;
  
  for (int col = 0; col < k && pivot < matrix.size(); col++) {
    // 找主元
    int sel = -1;
    for (int i = pivot; i < matrix.size(); i++) {
      if (matrix[i][col] != 0) { sel = i; break; }
    }
    if (sel == -1) continue;
    
    swap(matrix[pivot], matrix[sel]);
    
    // 归一化 + 消元
    uint8_t inv = gf.div(1, matrix[pivot][col]);
    for (int j = col; j < k; j++)
      matrix[pivot][j] = gf.mul(matrix[pivot][j], inv);
    
    for (int i = 0; i < matrix.size(); i++) {
      if (i != pivot && matrix[i][col] != 0) {
        uint8_t f = matrix[i][col];
        for (int j = col; j < k; j++)
          matrix[i][j] = gf.sub(matrix[i][j], gf.mul(matrix[pivot][j], f));
      }
    }
    pivot++;
  }
  
  return pivot >= k;
}
```

复杂度：O(n × k²)，其中 n 为收到的包数。

### 3.4 编码过程

设已缓存 n 个 payload 为 `P₁, P₂, ..., Pₙ`（可包含 DATA 和 REPAIR）：

1. **生成随机系数**：生成 n 个系数 `r₁, r₂, ..., rₙ`，确保至少一个非零
2. **线性组合**：对整个 payload 逐字节计算
   ```
   P_new[j] = Σᵢ (rᵢ × Pᵢ[j])  for j = 0..len-1
   ```
3. **构造新包**：设置 `fec_type = REPAIR`，分配新的 `frag_id`

### 3.5 正确性保证

由于线性组合的封闭性，重编码包的系数是原始包系数的线性组合：

```
设原始包系数为 C₁, C₂, ..., Cₙ（各为 k 维向量）
重编码包系数 = r₁·C₁ + r₂·C₂ + ... + rₙ·Cₙ
```

只要原始包的秩为 k，重编码包就是原始数据的有效线性组合，Receiver 可以用它参与解码。

## 4. 自适应冗余

### 4.1 设计动机

端到端 ACK 无法区分各跳的丢包情况。HOP_ACK 机制让每个节点只根据下一跳的丢包率调节冗余，避免冗余的浪费或不足。

### 4.2 丢包率估计

每秒统计一次：

1. **观测丢包率**：`obs_loss = 1 - acks_received / packets_sent`
2. **校正 ACK 丢包**：假设数据和 ACK 路径丢包率相同，则 `obs_loss = 2P - P²`
   - 解得：`sample_loss = 1 - √(1 - obs_loss)`
3. **EWMA 平滑**：`ewma_loss = α × sample_loss + (1-α) × ewma_loss`，α = 0.2

### 4.3 冗余度计算

目标：使冗余包数量能覆盖丢失的包。

```
target_loss = ewma_loss × 1.1  // 安全系数
R = target_loss / (1 - target_loss)
R = clamp(R, 0, 0.5)
```

示例：
- 丢包率 10% → R ≈ 12%
- 丢包率 20% → R ≈ 28%
- 丢包率 30% → R ≈ 47%

### 4.4 各节点的行为

| 节点 | 发送什么 | 收集什么 | 调节依据 |
|------|----------|----------|----------|
| Sender | DATA + REPAIR → 下游 | HOP_ACK ← 下游 | 第一跳丢包率 |
| Relay | DATA + REPAIR + 重编码 → 下游 | HOP_ACK ← 下游 | 下一跳丢包率 |
| Receiver | HOP_ACK → 上游 | - | - |

## 5. Relay 数据流处理

### 5.1 下行数据（来自 Sender）

```
收到 Datagram:
├─ 缓存 payload + 提取系数（DATA 和 REPAIR 都缓存）
├─ 发送 HOP_ACK 给上游
├─ 透传原始包给下游
├─ 统计 packets_sent++
└─ if 秩达到 k 且未重编码:
    └─ 生成 k×R 个重编码包，发送给下游
```

### 5.2 上行消息（来自 Receiver）

```
收到 Msg:
├─ if HOP_ACK:
│   └─ 更新 hop_acks_received（不转发）
├─ if ACK:
│   └─ 透传给 Sender
└─ if CONFIG:
    └─ 透传给 Sender
```

### 5.3 状态管理

每帧维护一个 `FrameState`：

```cpp
struct FrameState {
  uint32_t frame_id;
  FrameType frame_type;
  uint16_t k;                      // 总分片数
  uint16_t padding;
  size_t payload_size;
  vector<vector<uint8_t>> coeffs;  // 每个包的系数向量
  vector<string> payloads;         // 对应的 payload
  bool recoded;                    // 是否已重编码
};
```

旧帧（frame_id < current - 10）会被清理以释放内存。

## 6. 使用

### 6.1 启动命令

```bash
# Sender（监听12345，10%冗余）
./build/sender 12345 video.y4m -R 0.1

# Relay（连接Sender:12345，监听12346，自适应冗余）
./build/relay -A 127.0.0.1 12345 12346

# Receiver（连接Relay:12346）
./build/receiver 127.0.0.1 12346 704 576 --fps 30 --cbr 500
```

### 6.2 参数说明

**Sender / Relay 共用参数：**

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-R <ratio>` | 冗余度 | 0.1 |
| `-A` | 启用自适应冗余 | 禁用 |
| `-v` | 详细日志 | 禁用 |

**Relay 专用参数：**

| 位置参数 | 说明 |
|----------|------|
| `upstream_host` | 上游（Sender）地址 |
| `upstream_port` | 上游端口 |
| `listen_port` | 监听端口（供 Receiver 连接） |

### 6.3 网络模拟示例

```bash
# 模拟每跳 5% 丢包 + 20ms 延迟
sudo tc qdisc add dev lo root netem loss 5% delay 20ms

# 运行测试
./test_multihop.sh

# 清理
sudo tc qdisc del dev lo root
```

## 7. 已修复的问题

### 7.1 Payload 长度不一致导致重编码失效（2024-12-30）

**问题**：最后一个 DATA 包为节省带宽会**截断传输**（不传 padding 的 0），导致各 payload 长度不一致。原 `recode()` 假设所有 payload 等长，用第一个包的长度遍历所有包，当遍历到较短的最后一个 DATA 包时**越界读取垃圾数据**，生成的重编码包无效。

**修复**：遍历时使用每个 payload 的实际长度，较短包末尾等效补 0。

```cpp
// 修复前
size_t len = state.payload_size;  // 第一个包的大小
for (size_t j = 0; j < len; j++) {
    out[j] = ... state.payloads[i][j] ...  // 越界！
}

// 修复后
size_t plen = state.payloads[i].size();  // 每个包的实际长度
for (size_t j = 0; j < plen; j++) { ... }
```

### 7.2 frag_id 冲突（2024-12-30）

**问题**：Relay 的 `next_frag_id_` 从 0 开始，与 Sender 发送的 REPAIR 包 `frag_id` 冲突。Receiver 的 `Frame::insert_frag()` 按 `frag_id + k` 存储 REPAIR 包，冲突时先到的包被保留，后到的被丢弃。

**修复**：`next_frag_id_` 从 1000 开始，避开 Sender 的 frag_id 范围。

## 8. 局限性

| 限制 | 原因 |
|------|------|
| 秩不足时不重编码 | 无法保证编码质量 |
| 多跳需手动级联 | 每个 Relay 只能连接一个上游和一个下游 |
| 冗余度上限 50% | 防止带宽浪费 |
| 单 Receiver | Relay 只记录第一个连接的 Receiver 地址 |

## 9. 代码结构

```
src/app/
├── relay.cc              # Relay 主程序
│   ├── FrameState        # 帧状态（系数矩阵、payload 缓存）
│   ├── check_rank()      # GF(2⁸) 高斯消元秩检查
│   ├── Relay::run()      # 主循环（poll 上下游）
│   ├── handle_datagram() # 处理下行数据 + 秩判断 + 触发重编码
│   └── recode()          # 线性组合生成重编码包
├── protocol.hh/cc        # 协议定义（Datagram, Msg, HopAckMsg）
├── redundancy_controller.hh/cc  # 自适应冗余算法
└── galois.hh/cc          # GF(2⁸) 运算
```
