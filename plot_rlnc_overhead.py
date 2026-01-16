#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RLNC编码性能开销绘图脚本 - 符合国标规范（黑白版）
"""

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
rcParams['legend.fontsize'] = 12
rcParams['xtick.labelsize'] = 10
rcParams['ytick.labelsize'] = 10
rcParams['figure.dpi'] = 150
rcParams['savefig.dpi'] = 300
rcParams['axes.linewidth'] = 0.8
rcParams['lines.linewidth'] = 1.2
rcParams['lines.markersize'] = 6

def plot_encoding_time():
    """码率 vs 编码时间"""
    fig, ax = plt.subplots(figsize=(8, 5))
    
    # 数据：分辨率 -> (码率列表, 平均编码时间us)
    data = {
        '360p':  ([300, 500, 1000], [5.08, 7.66, 17.89]),
        '480p':  ([500, 1000, 2000], [9.59, 18.34, 66.35]),
        '720p':  ([1000, 2500, 5000], [20.52, 91.46, 294.32]),
        '1080p': ([2000, 5000, 10000], [55.12, 356.43, 1187.08]),
        '4K':    ([8000, 15000], [833.97, 3129.25]),
    }
    
    markers = ['o', 's', '^', 'D', 'v']
    linestyles = ['-', '--', '-.', ':', '-']
    colors = ['#000000', '#303030', '#606060', '#909090', '#B0B0B0']
    
    for i, (res, (bitrates, times)) in enumerate(data.items()):
        ax.plot(bitrates, times,
                color=colors[i],
                linestyle=linestyles[i],
                marker=markers[i],
                markerfacecolor='white',
                markeredgecolor=colors[i],
                markeredgewidth=1.2,
                label=res,
                linewidth=1.5,
                markersize=7)
    
    ax.set_xlabel('码率/(kbps)')
    ax.set_ylabel('编码时间/(μs)')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, linestyle='--', alpha=0.5, color='gray', which='both')
    ax.legend(loc='upper left', framealpha=0.95, edgecolor='black')
    
    plt.tight_layout()
    plt.savefig('results/fig_rlnc_overhead.png', bbox_inches='tight', facecolor='white')
    plt.savefig('results/fig_rlnc_overhead.pdf', bbox_inches='tight', facecolor='white')
    plt.close()
    print("已保存: fig_rlnc_overhead.png/pdf")

if __name__ == '__main__':
    plot_encoding_time()
