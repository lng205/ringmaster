#!/bin/bash
# 自适应冗余实验 - 模拟真实网络波动
# 场景: 长期低丢包(2%)，偶尔突发高丢包(15%)

VIDEO="sintel_trailer_2k_1080p24.y4m"
WIDTH=1920
HEIGHT=1080
FPS=24
CBR=4000  # 4Mbps
OUTDIR="results_adaptive"
mkdir -p "$OUTDIR"

run_test() {
    local name=$1 extra=$2 port=$((12700 + RANDOM % 100))
    
    pkill -9 sender 2>/dev/null; sleep 1
    sudo tc qdisc del dev lo root 2>/dev/null
    sudo tc qdisc add dev lo root netem loss 2% delay 20ms
    
    # 网络波动: 2%(30s) → 15%(10s) → 2%(30s) → 15%(10s) → 2%(20s)
    # 低丢包占比: 80/100 = 80%
    (
        sleep 30; sudo tc qdisc change dev lo root netem loss 15% delay 20ms
        sleep 10; sudo tc qdisc change dev lo root netem loss 2% delay 20ms
        sleep 30; sudo tc qdisc change dev lo root netem loss 15% delay 20ms
        sleep 10; sudo tc qdisc change dev lo root netem loss 2% delay 20ms
    ) 2>/dev/null &
    
    ./build/sender $port "$VIDEO" $extra 2> "$OUTDIR/${name}.log" &
    PID=$!
    sleep 2
    timeout 100 ./build/receiver 127.0.0.1 $port $WIDTH $HEIGHT --fps $FPS --cbr $CBR \
        -o "$OUTDIR/${name}.csv" 2>/dev/null
    kill $PID 2>/dev/null; wait 2>/dev/null
}

echo "===== 自适应冗余实验 ====="
echo "视频: 1080p, 4Mbps"
echo "网络: 2%(30s) → 15%(10s) → 2%(30s) → 15%(10s) → 2%(20s)"
echo "总时长: 100秒, 低丢包占比: 80%"
echo ""

echo "运行自适应..."
run_test "adaptive" ""
echo "运行固定20%..."
run_test "fixed20" "-R 0.2 --fixed-redundancy"

sudo tc qdisc del dev lo root 2>/dev/null

echo ""
echo "========== 结果 =========="
printf "%-12s %12s %10s %10s\n" "方案" "tx_bytes" "冗余包" "解码帧"
for name in adaptive fixed20; do
    stats=$(grep "Cumulative" "$OUTDIR/${name}.log" | tail -1)
    bytes=$(echo "$stats" | grep -oP 'tx_bytes=\K\d+')
    repair=$(echo "$stats" | grep -oP 'repair_pkts=\K\d+')
    frames=$(wc -l < "$OUTDIR/${name}.csv" 2>/dev/null)
    printf "%-12s %12s %10s %10s\n" "$name" "$bytes" "$repair" "$frames"
done

echo ""
echo "自适应冗余度变化:"
grep "New Redundancy" "$OUTDIR/adaptive.log" | awk 'NR%10==0' | tail -15 | \
    sed 's/.*EWMA): /  /' | sed 's/ (Sent.*Redundancy:/  →  R=/'
