# FEC 设计文档

## 概述

Ringmaster 使用帧内 FEC（Forward Error Correction）来应对网络丢包。采用 Reed-Solomon 编码，基于 Jerasure 库实现。

## 帧结构

每个视频帧被分片后封装为 `FECDatagram`：

```
+------------+----------+----------+----------+------------+---------+---------+
| frame_id   | fec_type | frag_id  | frag_cnt | repair_cnt | padding | payload |
| (4 bytes)  | (1 byte) | (2 bytes)| (2 bytes)| (2 bytes)  | (2 bytes)|  ...    |
+------------+----------+----------+----------+------------+---------+---------+
```

| 字段 | 说明 |
|------|------|
| `frame_id` | 帧序号 |
| `fec_type` | 0=数据包, 1=冗余包 |
| `frag_id` | 分片序号 |
| `frag_cnt` | 数据分片总数 (k) |
| `repair_cnt` | 冗余分片数 (m) |
| `payload` | 分片数据 |

## RS 编码参数

给定帧大小和冗余率，计算 RS 参数：

```
k = ceil(frame_size / max_payload)   # 数据分片数
m = ceil(k * redundancy)             # 冗余分片数
```

**恢复能力**：最多可恢复 m 个丢失的分片（任意位置）

## 自适应冗余率

根据网络丢包率动态调整冗余率，平衡带宽开销和恢复能力。

### 算法

使用 EWMA（指数加权移动平均）估计丢包率：

```
estimated_loss = α × current_loss + (1-α) × estimated_loss
```

其中 α = 0.3

### 冗余率选择

| 估计丢包率 | 冗余率 | 策略 |
|-----------|--------|------|
| < 5% | 25% | 节省带宽 |
| 5% - 15% | 50% | 平衡 |
| > 15% | 75% | 优先恢复 |

### 接口

```cpp
class AdaptiveFEC {
public:
    void update(int received, int expected);  // 更新估计
    float get_redundancy() const;             // 获取当前冗余率
    static float recommend(float loss_rate);  // 静态推荐
};
```

## 文件结构

```
src/fec/
├── intra_frame.cc/hh   # FEC 编解码实现
├── jerasure.cc/hh      # Jerasure 库封装
└── adaptive_fec.hh     # 自适应冗余率控制
```
