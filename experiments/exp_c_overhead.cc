// 实验C: 带宽开销分析
// 计算不同冗余率的带宽开销

#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>

using namespace std;

int main() {
    vector<int> redundancy_pcts = {10, 25, 50, 75, 100};
    int frame_size = 10 * 1024;  // 10KB
    int mtu = 1200;
    
    cout << "实验C: 带宽开销分析\n";
    cout << "====================\n\n";
    cout << "帧大小: " << frame_size/1024 << "KB, MTU: " << mtu << "B\n\n";
    
    ofstream csv("exp_c_results.csv");
    csv << "redundancy_pct,k,m,data_bytes,total_bytes,overhead_pct\n";
    
    cout << "| 冗余率 | k | m | 数据量 | 总传输量 | 带宽开销 |\n";
    cout << "|--------|---|---|--------|----------|----------|\n";
    
    for (int r : redundancy_pcts) {
        int k = (frame_size + mtu - 1) / mtu;
        int m = k * r / 100;
        if (m < 1) m = 1;
        
        int data_bytes = k * mtu;
        int total_bytes = (k + m) * mtu;
        double overhead = (double)total_bytes / data_bytes * 100;
        
        cout << "| " << setw(6) << r << "% | " 
             << setw(1) << k << " | " 
             << setw(1) << m << " | "
             << setw(6) << data_bytes << " | "
             << setw(8) << total_bytes << " | "
             << fixed << setprecision(1) << setw(7) << overhead << "% |\n";
        
        csv << r << "," << k << "," << m << "," 
            << data_bytes << "," << total_bytes << "," << overhead << "\n";
    }
    
    cout << "\n结论: 带宽开销 = (k+m)/k = 1 + 冗余率\n";
    csv.close();
    cout << "\n结果已保存到 exp_c_results.csv\n";
    return 0;
}
