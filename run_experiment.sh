#!/bin/bash
# 实验自动化脚本 - 支持断点续传
# 用法: ./run_experiment.sh [视频文件]
# 后台运行: nohup ./run_experiment.sh video.y4m > experiment.log 2>&1 &

# 不使用 set -e，手动处理错误

VIDEO=${1:-"ice_4cif_30fps.y4m"}
WIDTH=704
HEIGHT=576
FPS=30
CBR=500
PORT=12345
DURATION=10  # 每组实验时长(秒)
REPEAT=3     # 重复次数

OUTDIR="results"
mkdir -p "$OUTDIR"

# 实验配置: "方案名 冗余度 额外参数"
SCHEMES=(
    "G0 0 --no-arq"
    "G1 0 --fixed-redundancy"
    "G2 0.1 --fixed-redundancy"
    "G3 0.2 --fixed-redundancy"
)

# 主曲线: delay=50ms, loss变化
MAIN_DELAY=50
MAIN_LOSSES=(0 5 10 15 20)

# 敏感性: loss=10%, delay变化
SENS_LOSS=10
SENS_DELAYS=(0 50 100)

run_one() {
    local scheme=$1 redundancy=$2 extra=$3 loss=$4 delay=$5 run=$6
    local tag="${scheme}_loss${loss}_d${delay}_r${run}"
    local done_flag="$OUTDIR/.done_$tag"
    
    # 断点续传: 跳过已完成
    if [[ -f "$done_flag" ]]; then
        echo "SKIP: $tag (already done)"
        return 0
    fi
    
    echo "========================================"
    echo "RUN: $tag"
    echo "========================================"
    
    # 设置网络
    sudo tc qdisc del dev lo root 2>/dev/null || true
    if (( loss > 0 || delay > 0 )); then
        sudo tc qdisc add dev lo root netem loss ${loss}% delay ${delay}ms
    fi
    
    # 启动 sender (先启动，等待 receiver 连接)
    timeout $((DURATION + 10)) ./build/sender $PORT "$VIDEO" -R $redundancy $extra \
        2> "$OUTDIR/tx_$tag.log" &
    local tx_pid=$!
    sleep 2
    
    # 启动 receiver
    timeout $DURATION ./build/receiver 127.0.0.1 $PORT $WIDTH $HEIGHT \
        --fps $FPS --cbr $CBR --lazy 2 -o "$OUTDIR/rx_$tag.csv" 2>/dev/null || true
    
    # 杀掉 sender 并等待
    kill $tx_pid 2>/dev/null || true
    wait $tx_pid 2>/dev/null || true
    
    # 清理网络
    sudo tc qdisc del dev lo root 2>/dev/null || true
    
    # 标记完成
    touch "$done_flag"
    echo "DONE: $tag"
    sleep 2
}

echo "=== 实验开始: $(date) ==="
echo "视频: $VIDEO, 时长: ${DURATION}s, 重复: ${REPEAT}次"

# 主曲线实验
for loss in "${MAIN_LOSSES[@]}"; do
    for scheme_cfg in "${SCHEMES[@]}"; do
        read -r scheme redundancy extra <<< "$scheme_cfg"
        for ((run=1; run<=REPEAT; run++)); do
            run_one "$scheme" "$redundancy" "$extra" "$loss" "$MAIN_DELAY" "$run"
        done
    done
done

# 敏感性实验 (跳过 delay=50 因为主曲线已跑)
for delay in "${SENS_DELAYS[@]}"; do
    [[ $delay -eq $MAIN_DELAY ]] && continue
    for scheme_cfg in "${SCHEMES[@]}"; do
        read -r scheme redundancy extra <<< "$scheme_cfg"
        for ((run=1; run<=REPEAT; run++)); do
            run_one "$scheme" "$redundancy" "$extra" "$SENS_LOSS" "$delay" "$run"
        done
    done
done

echo "=== 实验完成: $(date) ==="
echo "结果保存在: $OUTDIR/"

