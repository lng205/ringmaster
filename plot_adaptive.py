#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
自适应冗余实验绘图脚本 - 符合国标规范（黑白版）
"""

import os
import re
import numpy as np
import matplotlib.pyplot as plt
from matplotlib import rcParams

# 设置国标规范字体和样式
plt.style.use('default')
rcParams['font.family'] = ['SimSun', 'SimHei', 'DejaVu Sans', 'sans-serif']
rcParams['font.sans-serif'] = ['SimHei', 'SimSun', 'DejaVu Sans']
rcParams['axes.unicode_minus'] = False
rcParams['font.size'] = 10.5
rcParams['axes.labelsize'] = 10.5
rcParams['legend.fontsize'] = 9
rcParams['xtick.labelsize'] = 10
rcParams['ytick.labelsize'] = 10
rcParams['figure.dpi'] = 150
rcParams['savefig.dpi'] = 300
rcParams['axes.linewidth'] = 0.8
rcParams['lines.linewidth'] = 1.2

RESULTS_DIR = "results_adaptive"

def parse_log(filepath):
    """解析发送端日志，提取丢包率和冗余度"""
    times = []
    loss_ewma = []
    redundancy = []
    
    if not os.path.exists(filepath):
        print(f"Warning: {filepath} not found")
        return times, loss_ewma, redundancy

    with open(filepath, 'r') as f:
        for line in f:
            if "Loss rate" in line and "New Redundancy" in line:
                try:
                    match = re.search(r'Loss rate \(sample/EWMA\): [\d\.]+/([\d\.]+)', line)
                    if match:
                        loss_ewma.append(float(match.group(1)))
                    
                    match = re.search(r'New Redundancy: ([\d\.]+)', line)
                    if match:
                        redundancy.append(float(match.group(1)))
                        
                    times.append(len(times))
                except:
                    pass
                    
    return times, loss_ewma, redundancy

def plot_redundancy_loss():
    """绘制丢包率与冗余度响应曲线"""
    times, loss, redundancy = parse_log(f'{RESULTS_DIR}/adaptive.log')
    
    if not times:
        print("No data found.")
        return

    fig, ax1 = plt.subplots(figsize=(8, 5))

    # 丢包率曲线
    ax1.set_xlabel('时间/s')
    ax1.set_ylabel('估计丢包率')
    ax1.plot(times, loss, color='black', linestyle='--', label='估计丢包率', linewidth=1.2)
    ax1.tick_params(axis='y')
    ax1.set_ylim(0, 0.35)

    # 冗余度曲线（右轴）
    ax2 = ax1.twinx()
    ax2.set_ylabel('冗余度')
    ax2.plot(times, redundancy, color='#404040', linestyle='-', label='冗余度', linewidth=1.5)
    ax2.tick_params(axis='y')
    ax2.set_ylim(0, 0.35)

    # 标注高丢包区域（使用斜线填充，黑白可见）
    ax1.axvspan(30, 40, facecolor='none', edgecolor='black', hatch='///', alpha=0.8, label='高丢包区域')
    ax1.axvspan(70, 80, facecolor='none', edgecolor='black', hatch='///', alpha=0.8)

    # 图例
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1 + lines2, labels1 + labels2, loc='upper left', framealpha=0.95, edgecolor='black')
    
    ax1.grid(True, linestyle='--', alpha=0.5, color='gray')
    
    plt.tight_layout()
    plt.savefig(f'{RESULTS_DIR}/fig1_redundancy.png', bbox_inches='tight', facecolor='white')
    plt.savefig(f'{RESULTS_DIR}/fig1_redundancy.pdf', bbox_inches='tight', facecolor='white')
    plt.close()
    print("已保存: fig1_redundancy.png/pdf")

if __name__ == "__main__":
    plot_redundancy_loss()
