# Mahimahi 网络仿真使用指南

## 核心原理

Mahimahi 为每个 `mm-*` 命令创建**独立的网络命名空间**，提供虚拟网络环境。

### 网络架构
```
Host (10.0.0.1) ←PPP→ Mahimahi Container (10.0.0.2)
```

- **MAHIMAHI_BASE=10.0.0.1**: 指向 host outside containers
- **Container IP=10.0.0.2**: Mahimahi 分配的虚拟IP

## 关键限制

⚠️ **重要**: `127.0.0.1` (localhost) 通信**不经过**虚拟网络接口，**不受网络限制影响**。

## 正确用法

### 基本语法
```bash
mm-delay <milliseconds> <command>
mm-loss uplink|downlink <rate> <command>
mm-link <uplink-trace> <downlink-trace> <command>
```

### Ringmaster 测试配置

#### 方法1: Host + Container (推荐)
```bash
# Terminal 1: sender 在 host 上运行
./build/sender 12345 ice_4cif_30fps.y4m

# Terminal 2: receiver 在 mahimahi 环境中，使用 MAHIMAHI_BASE
mm-delay 50 ./build/receiver $MAHIMAHI_BASE 12345 704 576 --fps 30 --cbr 500
```

#### 组合网络条件
```bash
# 延迟 + 丢包
mm-delay 50 mm-loss uplink 0.02 ./build/receiver $MAHIMAHI_BASE 12345 ...

# 延迟 + 带宽限制 (需要创建 trace 文件)
echo "1.0" > 1Mbps.trace
mm-delay 50 mm-link 1Mbps.trace 1Mbps.trace ./build/receiver $MAHIMAHI_BASE 12345 ...
```

## 验证方法

### 1. 检查环境
```bash
# 验证工具可用
which mm-delay mm-loss mm-link

# 检查 MAHIMAHI_BASE
mm-delay 10 env | grep MAHIMAHI
```

### 2. 验证网络效果
```bash
# 比较 RTT 值
# 正常环境: ~0.05-0.25ms
# 50ms延迟环境: ~99-101ms (双向延迟)
```

### 3. 进程监控
```bash
# 检查进程状态
ps aux | grep -E "(sender|receiver|mm-)"

# 检查网络连接
netstat -tunp | grep :12345
```

## 故障排除

### 问题: "Connection refused"
**原因**: 使用了 `127.0.0.1` 而非 `MAHIMAHI_BASE`
**解决**: 改用 `$MAHIMAHI_BASE`

### 问题: RTT 值不变
**原因**: 网络流量未通过虚拟接口
**解决**: 确认使用正确的 IP 地址配置

### 问题: 端口被占用
**原因**: 之前的进程未清理
**解决**:
```bash
pkill -f "sender\|receiver\|mm-"
```

## 高级用法

### 创建带宽 trace 文件
```bash
# 常量带宽
echo "1.0" > 1Mbps.trace    # 1Mbps
echo "0.5" > 500Kbps.trace  # 500Kbps

# 可变带宽 (每行一个时间点的带宽值)
echo -e "2.0\n1.5\n1.0\n0.5" > variable.trace
```

### 多条件组合
```bash
# 复杂网络环境
mm-delay 100 mm-loss uplink 0.05 down 0.02 mm-link high.trace low.trace ./program
```

## 最佳实践

1. **总是使用 MAHIMAHI_BASE** 进行跨命名空间通信
2. **验证 RTT 值** 确认网络限制生效
3. **清理进程** 避免端口冲突
4. **记录配置** 便于重现测试结果