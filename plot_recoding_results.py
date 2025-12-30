#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
重编码实验结果绘图脚本 - 符合国标规范（黑白版）
"""

import os
import glob
import numpy as np
import matplotlib.pyplot as plt
from matplotlib import rcParams

# 设置国标规范字体和样式
plt.style.use('default')
rcParams['font.family'] = ['SimSun', 'SimHei', 'DejaVu Sans', 'sans-serif']
rcParams['font.sans-serif'] = ['SimHei', 'SimSun', 'DejaVu Sans']
rcParams['axes.unicode_minus'] = False
rcParams['font.size'] = 10.5
rcParams['axes.titlesize'] = 12
rcParams['axes.labelsize'] = 10.5
rcParams['legend.fontsize'] = 9
rcParams['xtick.labelsize'] = 10
rcParams['ytick.labelsize'] = 10
rcParams['figure.dpi'] = 150
rcParams['savefig.dpi'] = 300
rcParams['axes.linewidth'] = 0.8
rcParams['lines.linewidth'] = 1.5
rcParams['lines.markersize'] = 7

# 数据目录
RESULTS_DIR = "results_recoding"
FPS = 30
DURATION = 10
EXPECTED_FRAMES = FPS * DURATION

# 方案配置
MODES = ['passthrough_r01', 'recode_r01', 'passthrough_r02', 'recode_r02']
MODE_LABELS = {
    'passthrough_r01': '透传 (R=0.1)',
    'recode_r01':      '重编码 (R=0.1)',
    'passthrough_r02': '透传 (R=0.2)',
    'recode_r02':      '重编码 (R=0.2)'
}
MODE_COLORS = {
    'passthrough_r01': '#000000',
    'recode_r01':      '#404040',
    'passthrough_r02': '#808080',
    'recode_r02':      '#C0C0C0'
}
MODE_LINESTYLES = {
    'passthrough_r01': '-',
    'recode_r01':      '--',
    'passthrough_r02': '-.',
    'recode_r02':      ':'
}
MODE_MARKERS = {
    'passthrough_r01': 'o',
    'recode_r01':      's',
    'passthrough_r02': '^',
    'recode_r02':      'D'
}
MODE_HATCHES = {
    'passthrough_r01': '',
    'recode_r01':      '///',
    'passthrough_r02': '...',
    'recode_r02':      'xxx'
}

LOSSES = [0, 5, 10, 15]


def load_data(mode, loss):
    """加载指定条件的实验数据"""
    # 数据目录指向实际的实验输出路径
    DATA_DIR = "experiments/high_bitrate_3000k/data"
    pattern = f"{DATA_DIR}/rx_{mode}_loss{loss}_r*.csv"
    files = glob.glob(pattern)
    
    all_frames = []
    all_delays = []
    run_frame_counts = []
    
    for f in files:
        frames_in_run = 0
        try:
            with open(f, 'r') as fp:
                for line in fp:
                    parts = line.strip().split(',')
                    if len(parts) >= 3:
                        frame_id = int(parts[0])
                        # 只统计前 EXPECTED_FRAMES 帧
                        if frame_id >= EXPECTED_FRAMES:
                            continue
                            
                        t_send = int(parts[1])
                        t_dec = int(parts[2])
                        delay_ms = t_dec - t_send
                        if 0 <= delay_ms < 10000:
                            all_delays.append(delay_ms)
                            frames_in_run += 1
            if frames_in_run >= 50:  # 有效run
                run_frame_counts.append(frames_in_run)
        except Exception as e:
            print(f"Warning: {f}: {e}")
    
    return run_frame_counts, all_delays


def calc_metrics(mode, loss):
    """计算指标"""
    frame_counts, delays = load_data(mode, loss)
    
    if not frame_counts:
        return None, None, None, None, None
    
    # 帧可解码率
    avg_frames = np.mean(frame_counts)
    decode_rate = avg_frames / EXPECTED_FRAMES * 100
    
    # 延迟统计
    if delays:
        avg_delay = np.mean(delays)
        p95_delay = np.percentile(delays, 95)
        std_delay = np.std(delays)
    else:
        avg_delay = p95_delay = std_delay = 0
    
    return decode_rate, avg_delay, p95_delay, std_delay, delays


def plot_figure1():
    """图1：帧可解码率 vs 丢包率"""
    fig, ax = plt.subplots(figsize=(7, 4.5))
    
    for mode in MODES:
        rates = []
        for loss in LOSSES:
            rate, _, _, _, _ = calc_metrics(mode, loss)
            rates.append(rate if rate else 0)
        
        ax.plot(LOSSES, rates,
                color=MODE_COLORS[mode],
                linestyle=MODE_LINESTYLES[mode],
                marker=MODE_MARKERS[mode],
                markerfacecolor='white',
                markeredgecolor=MODE_COLORS[mode],
                markeredgewidth=1.2,
                label=MODE_LABELS[mode])
    
    ax.set_xlabel('丢包率/%')
    ax.set_ylabel('帧可解码率/%')
    ax.set_xlim(-1, 16)
    ax.set_ylim(60, 102)
    ax.set_xticks(LOSSES)
    ax.grid(True, linestyle='--', alpha=0.5, color='gray')
    ax.legend(loc='lower left', framealpha=0.95, edgecolor='black')
    
    plt.tight_layout()
    plt.savefig(f'{RESULTS_DIR}/fig1_decode_rate.png', bbox_inches='tight', facecolor='white')
    plt.savefig(f'{RESULTS_DIR}/fig1_decode_rate.pdf', bbox_inches='tight', facecolor='white')
    plt.close()
    print("已保存: fig1_decode_rate.png/pdf")


def plot_figure2():
    """图2：平均帧延迟 vs 丢包率"""
    fig, ax = plt.subplots(figsize=(7, 4.5))
    
    for mode in MODES:
        avg_delays = []
        std_delays = []
        for loss in LOSSES:
            _, avg, _, std, _ = calc_metrics(mode, loss)
            avg_delays.append(avg if avg else 0)
            std_delays.append(std if std else 0)
        
        ax.errorbar(LOSSES, avg_delays,
                    yerr=std_delays,
                    color=MODE_COLORS[mode],
                    linestyle=MODE_LINESTYLES[mode],
                    marker=MODE_MARKERS[mode],
                    markerfacecolor='white',
                    markeredgecolor=MODE_COLORS[mode],
                    markeredgewidth=1.2,
                    label=MODE_LABELS[mode],
                    capsize=3,
                    capthick=1)
    
    ax.set_xlabel('丢包率/%')
    ax.set_ylabel('平均帧延迟/ms')
    ax.set_xlim(-1, 16)
    ax.set_xticks(LOSSES)
    ax.grid(True, linestyle='--', alpha=0.5, color='gray')
    ax.legend(loc='upper left', framealpha=0.95, edgecolor='black')
    
    plt.tight_layout()
    plt.savefig(f'{RESULTS_DIR}/fig2_avg_delay.png', bbox_inches='tight', facecolor='white')
    plt.savefig(f'{RESULTS_DIR}/fig2_avg_delay.pdf', bbox_inches='tight', facecolor='white')
    plt.close()
    print("已保存: fig2_avg_delay.png/pdf")


def plot_figure3():
    """图3：延迟CDF对比（15%丢包）"""
    fig, ax = plt.subplots(figsize=(7, 4.5))
    
    loss = 15
    
    for mode in MODES:
        _, _, _, _, delays = calc_metrics(mode, loss)
        if delays:
            sorted_delays = np.sort(delays)
            cdf = np.arange(1, len(sorted_delays) + 1) / len(sorted_delays)
            ax.plot(sorted_delays, cdf * 100,
                    color=MODE_COLORS[mode],
                    linestyle=MODE_LINESTYLES[mode],
                    label=MODE_LABELS[mode])
    
    # 标注关键线
    ax.axhline(y=95, color='gray', linestyle=':', alpha=0.7, linewidth=1)
    ax.axvline(x=100, color='gray', linestyle=':', alpha=0.7, linewidth=1)
    ax.text(105, 50, '100ms阈值', fontsize=8, color='gray')
    ax.text(400, 96, 'P95', fontsize=8, color='gray')
    
    ax.set_xlabel('帧延迟/ms')
    ax.set_ylabel('累积百分比/%')
    ax.set_xlim(0, 600)
    ax.set_ylim(0, 102)
    ax.grid(True, linestyle='--', alpha=0.5, color='gray')
    ax.legend(loc='lower right', framealpha=0.95, edgecolor='black')
    
    plt.tight_layout()
    plt.savefig(f'{RESULTS_DIR}/fig3_delay_cdf.png', bbox_inches='tight', facecolor='white')
    plt.savefig(f'{RESULTS_DIR}/fig3_delay_cdf.pdf', bbox_inches='tight', facecolor='white')
    plt.close()
    print("已保存: fig3_delay_cdf.png/pdf")


def plot_figure4():
    """图4：高延迟帧占比柱状图"""
    fig, ax = plt.subplots(figsize=(7, 4.5))
    
    THRESHOLD = 100  # ms
    
    x = np.arange(len(LOSSES))
    width = 0.35
    
    for i, mode in enumerate(MODES):
        high_delay_pcts = []
        for loss in LOSSES:
            _, _, _, _, delays = calc_metrics(mode, loss)
            if delays:
                high_count = sum(1 for d in delays if d > THRESHOLD)
                pct = high_count / len(delays) * 100
            else:
                pct = 0
            high_delay_pcts.append(pct)
        
        offset = (i - 0.5) * width
        ax.bar(x + offset, high_delay_pcts, width,
               color='white',
               edgecolor='black',
               linewidth=1,
               hatch=MODE_HATCHES[mode],
               label=MODE_LABELS[mode])
    
    ax.set_xlabel('丢包率/%')
    ax.set_ylabel(f'高延迟帧占比/% (>{THRESHOLD}ms)')
    ax.set_xticks(x)
    ax.set_xticklabels([f'{l}' for l in LOSSES])
    ax.set_ylim(0, 100)
    ax.grid(True, linestyle='--', alpha=0.5, axis='y', color='gray')
    ax.legend(loc='upper left', framealpha=0.95, edgecolor='black')
    
    plt.tight_layout()
    plt.savefig(f'{RESULTS_DIR}/fig4_high_delay_ratio.png', bbox_inches='tight', facecolor='white')
    plt.savefig(f'{RESULTS_DIR}/fig4_high_delay_ratio.pdf', bbox_inches='tight', facecolor='white')
    plt.close()
    print("已保存: fig4_high_delay_ratio.png/pdf")


def generate_table():
    """生成汇总表格"""
    print("\n=== 表1 重编码实验结果汇总 ===")
    print("| 丢包率 | 模式 | 帧可解码率/% | 平均延迟/ms | P95延迟/ms |")
    print("|--------|------|-------------|------------|-----------|")
    
    for loss in LOSSES:
        for mode in MODES:
            rate, avg, p95, _, _ = calc_metrics(mode, loss)
            if rate is not None:
                print(f"| {loss:2d}     | {MODE_LABELS[mode]:8s} | {rate:11.1f} | {avg:10.1f} | {p95:9.0f} |")


if __name__ == '__main__':
    os.chdir('/root/ringmaster')
    
    print("正在生成重编码实验图表...")
    plot_figure1()
    plot_figure2()
    generate_table()
    print(f"\n图表生成完成！保存在 {RESULTS_DIR}/ 目录")

