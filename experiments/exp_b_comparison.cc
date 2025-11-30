// 实验B: FEC效果对比
// 比较无FEC vs 有FEC在不同丢包率下的帧恢复率

#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <cstring>
#include <iomanip>
#include "jerasure.hh"

using namespace std;

const int FRAME_SIZE = 10 * 1024;
const int MTU = 1200;
const int RUNS = 200;

// 模拟丢包，返回是否能恢复
bool simulate(int k, int m, double loss_rate, mt19937& gen) {
    uniform_real_distribution<> dis(0, 1);
    int lost = 0;
    for (int i = 0; i < k + m; i++) {
        if (dis(gen) < loss_rate) lost++;
    }
    return lost <= m;  // m个冗余包能恢复m个丢失
}

int main() {
    vector<double> loss_rates = {0, 0.05, 0.10, 0.15, 0.20, 0.25, 0.30};
    
    int k = (FRAME_SIZE + MTU - 1) / MTU;  // k=9
    
    cout << "实验B: FEC效果对比\n";
    cout << "==================\n\n";
    cout << "帧大小: " << FRAME_SIZE/1024 << "KB, k=" << k << ", 测试次数=" << RUNS << "\n\n";
    
    ofstream csv("exp_b_results.csv");
    csv << "loss_rate,method,recovery_rate\n";
    
    random_device rd;
    mt19937 gen(rd());
    
    cout << "| 丢包率 | 无FEC | FEC-25% | FEC-50% |\n";
    cout << "|--------|-------|---------|----------|\n";
    
    for (double loss : loss_rates) {
        // 无FEC: 任何丢包都无法恢复
        int no_fec_ok = 0;
        for (int i = 0; i < RUNS; i++) {
            if (simulate(k, 0, loss, gen)) no_fec_ok++;
        }
        double no_fec_rate = (double)no_fec_ok / RUNS * 100;
        
        // FEC-25%
        int m25 = k * 25 / 100;
        int fec25_ok = 0;
        for (int i = 0; i < RUNS; i++) {
            if (simulate(k, m25, loss, gen)) fec25_ok++;
        }
        double fec25_rate = (double)fec25_ok / RUNS * 100;
        
        // FEC-50%
        int m50 = k * 50 / 100;
        int fec50_ok = 0;
        for (int i = 0; i < RUNS; i++) {
            if (simulate(k, m50, loss, gen)) fec50_ok++;
        }
        double fec50_rate = (double)fec50_ok / RUNS * 100;
        
        cout << "| " << fixed << setprecision(0) << setw(5) << loss*100 << "% | "
             << setw(5) << no_fec_rate << "% | "
             << setw(7) << fec25_rate << "% | "
             << setw(8) << fec50_rate << "% |\n";
        
        csv << loss << ",no_fec," << no_fec_rate << "\n";
        csv << loss << ",fec_25," << fec25_rate << "\n";
        csv << loss << ",fec_50," << fec50_rate << "\n";
    }
    
    csv.close();
    cout << "\n结果已保存到 exp_b_results.csv\n";
    return 0;
}
