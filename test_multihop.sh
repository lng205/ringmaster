#!/bin/bash
# 简单多跳场景测试: Sender → Relay → Receiver

VIDEO="ice_4cif_30fps.y4m"
WIDTH=704
HEIGHT=576
FPS=30
CBR=500
OUTDIR="results_multihop"
mkdir -p "$OUTDIR"

cleanup() {
    pkill -9 sender 2>/dev/null
    pkill -9 relay 2>/dev/null
    sudo tc qdisc del dev lo root 2>/dev/null
}
trap cleanup EXIT

cleanup
sleep 1

# 模拟网络: 5%丢包 + 20ms延迟
sudo tc qdisc add dev lo root netem loss 5% delay 20ms

echo "===== 多跳场景测试 ====="
echo "架构: Sender(12345) → Relay(12346) → Receiver"
echo "网络: 5% 丢包, 20ms 延迟"
echo ""

# 启动 Sender (端口12345, 10%冗余)
./build/sender 12345 "$VIDEO" -R 0.1 2> "$OUTDIR/sender.log" &
SENDER_PID=$!
sleep 1

# 启动 Relay (连接Sender, 监听12346, 自适应冗余)
./build/relay -A 127.0.0.1 12345 12346 2> "$OUTDIR/relay.log" &
RELAY_PID=$!
sleep 1

# 启动 Receiver (连接Relay, 运行30秒)
echo "运行中..."
timeout 30 ./build/receiver 127.0.0.1 12346 $WIDTH $HEIGHT --fps $FPS --cbr $CBR \
    -o "$OUTDIR/receiver.csv" 2> "$OUTDIR/receiver.log"

echo ""
echo "========== 结果 =========="
echo "Sender 日志:"
tail -5 "$OUTDIR/sender.log"
echo ""
echo "Relay 日志:"
tail -10 "$OUTDIR/relay.log"
echo ""
echo "Receiver 统计:"
frames=$(wc -l < "$OUTDIR/receiver.csv" 2>/dev/null || echo 0)
echo "解码帧数: $frames"

