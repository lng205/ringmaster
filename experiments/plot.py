#!/usr/bin/env python3
"""实验绘图脚本"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib
matplotlib.use('Agg')
import os

# 中文字体
from matplotlib.font_manager import FontProperties
import matplotlib.font_manager as fm

def find_chinese_font():
    for font in fm.fontManager.ttflist:
        name = font.name.lower()
        if 'cjk' in name or 'chinese' in name or 'simsun' in name:
            return font.fname
    return None

FONT_PATH = find_chinese_font()
CFONT = FontProperties(fname=FONT_PATH) if FONT_PATH else FontProperties()

plt.rcParams['axes.unicode_minus'] = False
plt.rcParams['figure.dpi'] = 150

def plot_exp_a():
    """实验A: 丢包恢复能力"""
    if not os.path.exists('exp_a_results.csv'):
        return
    df = pd.read_csv('exp_a_results.csv')
    
    fig, ax = plt.subplots(figsize=(8, 5))
    colors = ['#0072B2', '#009E73', '#D55E00']
    markers = ['o', 's', '^']
    
    for i, (r, grp) in enumerate(df.groupby('redundancy')):
        m = grp['m'].iloc[0]
        label = f"冗余率{int(r*100)}% (m={m})"
        ax.plot(grp['lost'], grp['success_rate'], 
               marker=markers[i], color=colors[i], label=label, linewidth=2)
    
    ax.set_xlabel('丢包数量', fontproperties=CFONT, fontsize=12)
    ax.set_ylabel('恢复成功率 (%)', fontproperties=CFONT, fontsize=12)
    ax.set_title('实验A: FEC丢包恢复能力', fontproperties=CFONT, fontsize=14)
    ax.set_ylim(-5, 105)
    ax.legend(prop=CFONT, frameon=True, edgecolor='black')
    ax.grid(True, alpha=0.3, linestyle='--')
    
    plt.tight_layout()
    plt.savefig('exp_a_recovery.png', bbox_inches='tight', facecolor='white')
    print("保存: exp_a_recovery.png")

def plot_exp_f():
    """实验F: 自适应冗余率"""
    if not os.path.exists('exp_f_results.csv'):
        return
    df = pd.read_csv('exp_f_results.csv')
    
    # 按丢包率聚合（取平均），避免重复点
    fixed = df[df['method'] == 'fixed_50'].groupby('loss_rate')[['recovery_rate', 'bandwidth_overhead']].mean().reset_index()
    adaptive = df[df['method'] == 'adaptive'].groupby('loss_rate')[['recovery_rate', 'bandwidth_overhead']].mean().reset_index()
    
    # 按丢包率排序
    fixed = fixed.sort_values('loss_rate')
    adaptive = adaptive.sort_values('loss_rate')
    
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    
    # 图1: 恢复率对比
    ax = axes[0]
    ax.plot(fixed['loss_rate'] * 100, fixed['recovery_rate'] * 100, 
           'o-', color='#0072B2', label='固定50%', linewidth=2, markersize=8)
    ax.plot(adaptive['loss_rate'] * 100, adaptive['recovery_rate'] * 100, 
           's-', color='#D55E00', label='自适应', linewidth=2, markersize=8)
    
    ax.set_xlabel('丢包率 (%)', fontproperties=CFONT, fontsize=12)
    ax.set_ylabel('帧恢复率 (%)', fontproperties=CFONT, fontsize=12)
    ax.set_title('(a) 恢复率对比', fontproperties=CFONT, fontsize=14)
    ax.set_ylim(80, 102)
    ax.legend(prop=CFONT, frameon=True, edgecolor='black')
    ax.grid(True, alpha=0.3, linestyle='--')
    
    # 图2: 带宽开销对比
    ax = axes[1]
    ax.plot(fixed['loss_rate'] * 100, fixed['bandwidth_overhead'] * 100, 
           'o-', color='#0072B2', label='固定50%', linewidth=2, markersize=8)
    ax.plot(adaptive['loss_rate'] * 100, adaptive['bandwidth_overhead'] * 100, 
           's-', color='#D55E00', label='自适应', linewidth=2, markersize=8)
    
    ax.set_xlabel('丢包率 (%)', fontproperties=CFONT, fontsize=12)
    ax.set_ylabel('带宽开销 (%)', fontproperties=CFONT, fontsize=12)
    ax.set_title('(b) 带宽开销对比', fontproperties=CFONT, fontsize=14)
    ax.legend(prop=CFONT, frameon=True, edgecolor='black')
    ax.grid(True, alpha=0.3, linestyle='--')
    
    plt.tight_layout()
    plt.savefig('exp_f_adaptive.png', bbox_inches='tight', facecolor='white')
    print("保存: exp_f_adaptive.png")

def plot_exp_d():
    """实验D: RS编解码性能"""
    if not os.path.exists('exp_d_results.csv'):
        return
    df = pd.read_csv('exp_d_results.csv')
    
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    
    # 帧大小转为KB显示
    df['frame_kb'] = df['frame_size'] / 1024
    
    # 图1: 编解码时间 vs 帧大小
    ax = axes[0]
    for r in [25, 50]:
        sub = df[df['redundancy_pct'] == r]
        ax.plot(sub['frame_kb'], sub['encode_us'] / 1000, 
               'o-', label=f'编码 (冗余{r}%)', linewidth=2, markersize=6)
        ax.plot(sub['frame_kb'], sub['decode_with_loss_us'] / 1000, 
               's--', label=f'解码 (冗余{r}%)', linewidth=2, markersize=6)
    
    ax.set_xlabel('帧大小 (KB)', fontproperties=CFONT, fontsize=12)
    ax.set_ylabel('时间 (ms)', fontproperties=CFONT, fontsize=12)
    ax.set_title('(a) 编解码时间', fontproperties=CFONT, fontsize=14)
    ax.legend(prop=CFONT, frameon=True, edgecolor='black', fontsize=9)
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.set_xscale('log')
    ax.set_yscale('log')
    
    # 图2: 吞吐量 vs 帧大小
    ax = axes[1]
    for r in [25, 50]:
        sub = df[df['redundancy_pct'] == r]
        ax.plot(sub['frame_kb'], sub['encode_mbps'], 
               'o-', label=f'编码 (冗余{r}%)', linewidth=2, markersize=6)
        ax.plot(sub['frame_kb'], sub['decode_mbps'], 
               's--', label=f'解码 (冗余{r}%)', linewidth=2, markersize=6)
    
    ax.set_xlabel('帧大小 (KB)', fontproperties=CFONT, fontsize=12)
    ax.set_ylabel('吞吐量 (MB/s)', fontproperties=CFONT, fontsize=12)
    ax.set_title('(b) 编解码吞吐量', fontproperties=CFONT, fontsize=14)
    ax.legend(prop=CFONT, frameon=True, edgecolor='black', fontsize=9)
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.set_xscale('log')
    
    plt.tight_layout()
    plt.savefig('exp_d_performance.png', bbox_inches='tight', facecolor='white')
    print("保存: exp_d_performance.png")

if __name__ == "__main__":
    plot_exp_a()
    plot_exp_f()
    plot_exp_d()
