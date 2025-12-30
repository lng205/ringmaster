#!/bin/bash
# 动态丢包率实验 - 展示自适应算法的响应能力

VIDEO="ice_4cif_30fps.y4m"
PORT=12500
OUTDIR="results_adaptive"
mkdir -p "$OUTDIR"

# 后台动态调整丢包率
dynamic_loss() {
    local pattern=$1
    while true; do
        for loss in $pattern; do
            sudo tc qdisc change dev lo root netem loss ${loss}% delay 20ms 2>/dev/null
            echo "[$(date +%H:%M:%S)] 丢包率: ${loss}%"
            sleep 5
        done
    done
}

run_dynamic_test() {
    local name=$1 extra=$2 pattern=$3
    
    pkill sender 2>/dev/null; sleep 1
    sudo tc qdisc del dev lo root 2>/dev/null
    sudo tc qdisc add dev lo root netem loss 5% delay 20ms
    
    # 启动动态丢包
    dynamic_loss "$pattern" &
    LOSS_PID=$!
    
    # 运行测试
    timeout 35 ./build/sender $PORT "$VIDEO" $extra 2> "$OUTDIR/${name}.log" &
    sleep 2
    timeout 30 ./build/receiver 127.0.0.1 $PORT 704 576 --fps 30 \
        -o "$OUTDIR/${name}.csv" 2>/dev/null
    
    kill $LOSS_PID 2>/dev/null
    pkill sender 2>/dev/null
    sudo tc qdisc del dev lo root 2>/dev/null
    
    echo "=== $name 冗余度变化 ==="
    grep "New Redundancy\|fixed redundancy" "$OUTDIR/${name}.log" | \
        sed 's/.*Loss rate /Loss: /' | sed 's/ \[fixed.*//' | tail -8
}

echo "===== 动态丢包率实验 ====="
echo "丢包率变化: 5% → 20% → 5% → 20% → 5% → 20%"
echo ""

run_dynamic_test "dynamic_adaptive" "" "5 20 5 20 5 20"
echo ""
run_dynamic_test "dynamic_fixed15" "-R 0.15 --fixed-redundancy" "5 20 5 20 5 20"

echo ""
echo "=== 解码帧对比 ==="
for f in "$OUTDIR"/dynamic_*.csv; do
    name=$(basename "$f" .csv)
    frames=$(wc -l < "$f" 2>/dev/null)
    echo "$name: $frames 帧"
done

