#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
实验一结果绘图脚本 - 符合国标规范（黑白版）
"""

import os
import glob
import numpy as np
import matplotlib.pyplot as plt
from matplotlib import rcParams

# 设置国标规范字体和样式
plt.style.use('default')
# 使用国标规范字体：宋体(SimSun)用于正文，黑体(SimHei)用于标题
rcParams['font.family'] = ['SimSun', 'SimHei', 'DejaVu Sans', 'sans-serif']
rcParams['font.sans-serif'] = ['SimHei', 'SimSun', 'DejaVu Sans']
rcParams['font.serif'] = ['SimSun', 'SimHei']
rcParams['axes.unicode_minus'] = False
rcParams['font.size'] = 10.5  # 五号字体
rcParams['axes.titlesize'] = 12
rcParams['axes.labelsize'] = 10.5
rcParams['legend.fontsize'] = 12
rcParams['xtick.labelsize'] = 10
rcParams['ytick.labelsize'] = 10
rcParams['figure.dpi'] = 150
rcParams['savefig.dpi'] = 300
rcParams['axes.linewidth'] = 0.8
rcParams['lines.linewidth'] = 1.2
rcParams['lines.markersize'] = 6

# 数据目录
RESULTS_DIR = "results"
FPS = 30
DURATION = 10
EXPECTED_FRAMES = FPS * DURATION  # 300帧

# 方案配置 - 黑白样式
SCHEMES = ['G0', 'G1', 'G2', 'G3']
SCHEME_LABELS = {
    'G0': '裸UDP',
    'G1': 'ARQ-only', 
    'G2': 'RLNC+ARQ (R=0.1)',
    'G3': 'RLNC+ARQ (R=0.2)'
}

# 黑白配色：使用不同灰度
SCHEME_COLORS = {
    'G0': '#000000',  # 黑色
    'G1': '#404040',  # 深灰
    'G2': '#808080',  # 中灰
    'G3': '#A0A0A0'   # 浅灰
}

# 不同线型区分
SCHEME_LINESTYLES = {
    'G0': '-',      # 实线
    'G1': '--',     # 虚线
    'G2': '-.',     # 点划线
    'G3': ':'       # 点线
}

# 不同标记区分
SCHEME_MARKERS = {
    'G0': 'o',      # 圆形
    'G1': 's',      # 方形
    'G2': '^',      # 三角形
    'G3': 'D'       # 菱形
}

# 柱状图填充样式
SCHEME_HATCHES = {
    'G0': '',       # 无填充
    'G1': '///',    # 斜线
    'G2': '...',    # 点
    'G3': 'xxx'     # 交叉
}

def load_data(scheme, loss, delay):
    """加载指定条件的实验数据"""
    pattern = f"{RESULTS_DIR}/rx_{scheme}_loss{loss}_d{delay}_r*.csv"
    files = glob.glob(pattern)
    
    all_frames = []
    all_delays = []
    
    for f in files:
        try:
            with open(f, 'r') as fp:
                for line in fp:
                    parts = line.strip().split(',')
                    if len(parts) >= 4:
                        frame_id = int(parts[0])
                        t_send = int(parts[1])
                        t_dec = int(parts[2])
                        decoded = int(parts[3])
                        if decoded == 1:
                            delay_ms = t_dec - t_send
                            if 0 <= delay_ms < 10000:  # 过滤异常值，允许0延迟
                                all_frames.append(frame_id)
                                all_delays.append(delay_ms)
        except Exception as e:
            print(f"Warning: {f}: {e}")
    
    return all_frames, all_delays

def calc_metrics(scheme, loss, delay):
    """计算指标"""
    frames, delays = load_data(scheme, loss, delay)
    
    # 帧可解码率
    n_runs = len(glob.glob(f"{RESULTS_DIR}/rx_{scheme}_loss{loss}_d{delay}_r*.csv"))
    if n_runs == 0:
        return None, None, None, None
    
    total_expected = EXPECTED_FRAMES * n_runs
    decode_rate = len(frames) / total_expected * 100 if total_expected > 0 else 0
    
    # 延迟统计
    if len(delays) > 0:
        avg_delay = np.mean(delays)
        p95_delay = np.percentile(delays, 95)
        std_delay = np.std(delays)
    else:
        avg_delay = p95_delay = std_delay = 0
    
    return decode_rate, avg_delay, p95_delay, std_delay

def plot_figure1():
    """图1：帧可解码率 vs 丢包率（主曲线）"""
    fig, ax = plt.subplots(figsize=(8, 5))
    
    losses = [0, 5, 10, 15, 20]
    delay = 50
    
    for scheme in SCHEMES:
        rates = []
        for loss in losses:
            rate, _, _, _ = calc_metrics(scheme, loss, delay)
            rates.append(rate if rate else 0)
        
        ax.plot(losses, rates, 
                color=SCHEME_COLORS[scheme],
                linestyle=SCHEME_LINESTYLES[scheme],
                marker=SCHEME_MARKERS[scheme],
                markerfacecolor='white',
                markeredgecolor=SCHEME_COLORS[scheme],
                markeredgewidth=1.2,
                label=SCHEME_LABELS[scheme],
                linewidth=1.5,
                markersize=7)
    
    ax.set_xlabel('丢包率/%')
    ax.set_ylabel('帧可解码率/%')
    ax.set_xlim(-1, 21)
    ax.set_ylim(0, 105)
    ax.set_xticks(losses)
    ax.grid(True, linestyle='--', alpha=0.5, color='gray')
    ax.legend(loc='lower left', framealpha=0.95, edgecolor='black')
    
    plt.tight_layout()
    plt.savefig('results/fig1_decode_rate.png', bbox_inches='tight', facecolor='white')
    plt.savefig('results/fig1_decode_rate.pdf', bbox_inches='tight', facecolor='white')
    plt.close()
    print("已保存: fig1_decode_rate.png/pdf")

def plot_figure2():
    """图2：平均帧延迟 vs 丢包率（主曲线）"""
    fig, ax = plt.subplots(figsize=(8, 5))
    
    losses = [0, 5, 10, 15, 20]
    delay = 50
    
    for scheme in SCHEMES:
        avg_delays = []
        std_delays = []
        for loss in losses:
            _, avg, _, std = calc_metrics(scheme, loss, delay)
            avg_delays.append(avg if avg else 0)
            std_delays.append(std if std else 0)
        
        ax.errorbar(losses, avg_delays,
                    yerr=std_delays,
                    color=SCHEME_COLORS[scheme],
                    linestyle=SCHEME_LINESTYLES[scheme],
                    marker=SCHEME_MARKERS[scheme],
                    markerfacecolor='white',
                    markeredgecolor=SCHEME_COLORS[scheme],
                    markeredgewidth=1.2,
                    label=SCHEME_LABELS[scheme],
                    linewidth=1.5,
                    markersize=7,
                    capsize=3,
                    capthick=1)
    
    ax.set_xlabel('丢包率/%')
    ax.set_ylabel('平均帧延迟/ms')
    ax.set_xlim(-1, 21)
    ax.set_xticks(losses)
    ax.grid(True, linestyle='--', alpha=0.5, color='gray')
    ax.legend(loc='upper left', framealpha=0.95, edgecolor='black')
    
    plt.tight_layout()
    plt.savefig('results/fig2_avg_delay.png', bbox_inches='tight', facecolor='white')
    plt.savefig('results/fig2_avg_delay.pdf', bbox_inches='tight', facecolor='white')
    plt.close()
    print("已保存: fig2_avg_delay.png/pdf")

def plot_figure3():
    """图3：帧可解码率 vs 时延（敏感性柱状图）"""
    fig, ax = plt.subplots(figsize=(8, 5))
    
    delays_list = [0, 50, 100]
    loss = 10
    
    x = np.arange(len(delays_list))
    width = 0.18
    
    for i, scheme in enumerate(SCHEMES):
        rates = []
        for delay in delays_list:
            rate, _, _, _ = calc_metrics(scheme, loss, delay)
            rates.append(rate if rate else 0)
        
        offset = (i - 1.5) * width
        bars = ax.bar(x + offset, rates, width,
                      color='white',
                      edgecolor='black',
                      linewidth=1,
                      hatch=SCHEME_HATCHES[scheme],
                      label=SCHEME_LABELS[scheme])
    
    ax.set_xlabel('网络时延/ms')
    ax.set_ylabel('帧可解码率/%')
    ax.set_xticks(x)
    ax.set_xticklabels([f'{d}' for d in delays_list])
    ax.set_ylim(0, 110)
    ax.grid(True, linestyle='--', alpha=0.5, axis='y', color='gray')
    ax.legend(loc='lower right', framealpha=0.95, edgecolor='black')
    
    plt.tight_layout()
    plt.savefig('results/fig3_delay_sensitivity.png', bbox_inches='tight', facecolor='white')
    plt.savefig('results/fig3_delay_sensitivity.pdf', bbox_inches='tight', facecolor='white')
    plt.close()
    print("已保存: fig3_delay_sensitivity.png/pdf")

def plot_figure4():
    """图4：平均帧延迟 vs 时延（敏感性柱状图）"""
    fig, ax = plt.subplots(figsize=(8, 5))
    
    delays_list = [0, 50, 100]
    loss = 10
    
    x = np.arange(len(delays_list))
    width = 0.18
    
    for i, scheme in enumerate(SCHEMES):
        avg_delays = []
        for delay in delays_list:
            _, avg, _, _ = calc_metrics(scheme, loss, delay)
            avg_delays.append(avg if avg else 0)
        
        offset = (i - 1.5) * width
        bars = ax.bar(x + offset, avg_delays, width,
                      color='white',
                      edgecolor='black',
                      linewidth=1,
                      hatch=SCHEME_HATCHES[scheme],
                      label=SCHEME_LABELS[scheme])
    
    ax.set_xlabel('网络时延/ms')
    ax.set_ylabel('平均帧延迟/ms')
    ax.set_xticks(x)
    ax.set_xticklabels([f'{d}' for d in delays_list])
    ax.grid(True, linestyle='--', alpha=0.5, axis='y', color='gray')
    ax.legend(loc='upper left', framealpha=0.95, edgecolor='black')
    
    plt.tight_layout()
    plt.savefig('results/fig4_delay_sensitivity_latency.png', bbox_inches='tight', facecolor='white')
    plt.savefig('results/fig4_delay_sensitivity_latency.pdf', bbox_inches='tight', facecolor='white')
    plt.close()
    print("已保存: fig4_delay_sensitivity_latency.png/pdf")

def generate_table():
    """生成汇总表格"""
    print("\n=== 表1 实验结果汇总 (delay=50ms) ===")
    print("| 丢包率 | 方案 | 帧可解码率/% | 平均延迟/ms | P95延迟/ms |")
    print("|--------|------|-------------|------------|-----------|")
    
    for loss in [0, 5, 10, 15, 20]:
        for scheme in SCHEMES:
            rate, avg, p95, _ = calc_metrics(scheme, loss, 50)
            if rate is not None:
                print(f"| {loss:2d}     | {SCHEME_LABELS[scheme]:18s} | {rate:11.1f} | {avg:10.1f} | {p95:9.1f} |")

if __name__ == '__main__':
    # os.chdir('/root/ringmaster')
    
    print("正在生成图表（黑白国标规范版）...")
    plot_figure1()
    plot_figure2()
    plot_figure3()
    plot_figure4()
    generate_table()
    print("\n图表生成完成！保存在 results/ 目录")
