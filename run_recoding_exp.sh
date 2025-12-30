#!/bin/bash
# 重编码增益实验（简化版）
# 后台运行: nohup ./run_recoding_exp.sh > recoding_exp.log 2>&1 &

VIDEO="${1:-ice_4cif_30fps.y4m}"
WIDTH=704
HEIGHT=576
FPS=30
CBR=3000
DURATION=10
REPEAT=1

EXP_NAME="high_bitrate_3000k"
OUTDIR="experiments/${EXP_NAME}/data"
mkdir -p "$OUTDIR"

# 简化对照组: 透传 vs 重编码
MODES=(
    "passthrough_r01 0.1 0"
    "recode_r01      0.1 0.1"
    "passthrough_r02 0.2 0"
    "recode_r02      0.2 0.2"
)

# 丢包率 (20%+ 对帧级纠删无意义)
# 只跑有丢包的场景，0%已经验证过了且必定是100%，不需要浪费时间
LOSSES=(0 5 10 15)
DELAY=25

cleanup() {
    pkill -9 sender 2>/dev/null
    pkill -9 relay 2>/dev/null
    pkill -9 receiver 2>/dev/null
    sudo tc qdisc del dev lo root 2>/dev/null
    # 等待端口释放
    for i in {1..20}; do
        if ! ss -tln | grep -qE ':1234[56]\b'; then
            break
        fi
        sleep 0.2
    done
}
trap cleanup EXIT

# 验证实验结果有效性（至少收到 50 帧）
is_valid_result() {
    local csv="$1"
    [[ -f "$csv" ]] && [[ $(wc -l < "$csv") -ge 50 ]]
}

run_one() {
    local mode=$1 sender_r=$2 relay_r=$3 loss=$4 run=$5
    local tag="${mode}_loss${loss}_r${run}"
    local done_flag="$OUTDIR/.done_$tag"
    
    if [[ -f "$done_flag" ]]; then
        echo "SKIP: $tag"
        return 0
    fi
    
    echo "========================================"
    echo "RUN: $tag (sender R=$sender_r, relay R=$relay_r, loss=${loss}%)"
    echo "========================================"
    
    cleanup
    sleep 1
    
    # 先不加丢包，让连接握手完成
    # Sender
    timeout $((DURATION + 10)) ./build/sender 12345 "$VIDEO" -R $sender_r \
        2> "$OUTDIR/tx_$tag.log" &
    sleep 1
    
    # Relay
    timeout $((DURATION + 7)) ./build/relay 127.0.0.1 12345 12346 -R $relay_r \
        2> "$OUTDIR/relay_$tag.log" &
    sleep 1
    
    # Receiver (先启动，等待连接成功)
    timeout $((DURATION + 2)) ./build/receiver 127.0.0.1 12346 $WIDTH $HEIGHT \
        --fps $FPS --cbr $CBR --lazy 2 -o "$OUTDIR/rx_$tag.csv" \
        2> "$OUTDIR/rx_$tag.log" &
    RX_PID=$!
    
    # 等待 Sender 收到 CONFIG（检查日志中出现 "Received config"）
    for i in {1..30}; do
        if grep -q "Received config" "$OUTDIR/tx_$tag.log" 2>/dev/null; then
            break
        fi
        sleep 0.2
    done
    
    # 连接成功后再加丢包和延迟
    sudo tc qdisc add dev lo root netem loss ${loss}% delay ${DELAY}ms
    
    # 等待 Receiver 结束
    wait $RX_PID 2>/dev/null || true
    
    cleanup
    
    # 只有结果有效才标记完成
    if is_valid_result "$OUTDIR/rx_$tag.csv"; then
        touch "$done_flag"
        echo "DONE: $tag ($(wc -l < "$OUTDIR/rx_$tag.csv") frames)"
    else
        echo "FAILED: $tag - will retry next run"
        rm -f "$done_flag"
    fi
    # 快速检查，端口释放后立即开始下一次
    sleep 0.5
}

echo "=== 重编码实验开始: $(date) ==="
echo "视频: $VIDEO, 时长: ${DURATION}s, 重复: ${REPEAT}次"
echo ""

for loss in "${LOSSES[@]}"; do
    for mode_cfg in "${MODES[@]}"; do
        read -r mode sender_r relay_r <<< "$mode_cfg"
        for ((run=1; run<=REPEAT; run++)); do
            run_one "$mode" "$sender_r" "$relay_r" "$loss" "$run"
        done
    done
done

echo ""
echo "=== 实验完成: $(date) ==="
echo "结果: $OUTDIR/"
