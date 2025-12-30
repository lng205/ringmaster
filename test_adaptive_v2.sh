#!/bin/bash
# 自适应冗余实验 v2
# 目的1: 验证丢包估计与冗余调节
# 目的2: 测试带宽节省（保持QoS）

VIDEO="ice_4cif_30fps.y4m"
OUTDIR="results_adaptive"
mkdir -p "$OUTDIR"

# 阶梯式丢包率变化: 0% → 10% → 20% → 10% → 0%
run_stepped_test() {
    local name=$1 extra=$2
    local port=$((12600 + RANDOM % 100))
    
    pkill sender 2>/dev/null; sleep 0.5
    sudo tc qdisc del dev lo root 2>/dev/null
    sudo tc qdisc add dev lo root netem delay 20ms
    
    # 后台阶梯变化丢包率
    (
        sleep 2;  sudo tc qdisc change dev lo root netem loss 0% delay 20ms
        sleep 6;  sudo tc qdisc change dev lo root netem loss 10% delay 20ms
        sleep 6;  sudo tc qdisc change dev lo root netem loss 20% delay 20ms
        sleep 6;  sudo tc qdisc change dev lo root netem loss 10% delay 20ms
        sleep 6;  sudo tc qdisc change dev lo root netem loss 0% delay 20ms
    ) &
    
    timeout 32 ./build/sender $port "$VIDEO" $extra 2> "$OUTDIR/${name}.log" &
    sleep 1
    timeout 30 ./build/receiver 127.0.0.1 $port 704 576 --fps 30 \
        -o "$OUTDIR/${name}.csv" 2>/dev/null
    
    pkill sender 2>/dev/null; wait 2>/dev/null
    sudo tc qdisc del dev lo root 2>/dev/null
}

echo "===== 自适应冗余算法实验 ====="
echo "丢包率阶梯: 0% → 10% → 20% → 10% → 0% (每阶段6秒)"
echo ""

echo "[1/2] 自适应冗余..."
run_stepped_test "stepped_adaptive" ""

echo "[2/2] 固定30%冗余..."
run_stepped_test "stepped_fixed30" "-R 0.3 --fixed-redundancy"

echo ""
echo "========== 结果 =========="

# 目的1: 冗余调节验证
echo ""
echo "【目的1】冗余度随丢包率变化:"
grep "New Redundancy" "$OUTDIR/stepped_adaptive.log" | \
    awk -F'[:/()]' '{printf "  EWMA=%.2f → R=%.2f\n", $4, $NF}'

# 目的2: 带宽对比
echo ""
echo "【目的2】带宽与QoS对比:"
for name in stepped_adaptive stepped_fixed30; do
    bytes=$(grep "Cumulative" "$OUTDIR/${name}.log" | tail -1 | grep -oP 'tx_bytes=\K\d+')
    frames=$(wc -l < "$OUTDIR/${name}.csv" 2>/dev/null)
    printf "  %-18s tx_bytes=%s  frames=%s\n" "$name" "$bytes" "$frames"
done

