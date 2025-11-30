#ifndef ADAPTIVE_FEC_HH
#define ADAPTIVE_FEC_HH

/**
 * 自适应FEC冗余率控制器
 * 根据估计的丢包率动态调整冗余率
 */
class AdaptiveFEC {
public:
    // 冗余率档位
    static constexpr float LOW = 0.25f;    // 低丢包环境
    static constexpr float MEDIUM = 0.50f; // 中等丢包环境
    static constexpr float HIGH = 0.75f;   // 高丢包环境
    
    // 丢包率阈值
    static constexpr float THRESH_LOW = 0.05f;   // <5% 用低冗余
    static constexpr float THRESH_HIGH = 0.15f;  // >15% 用高冗余
    
    AdaptiveFEC() : current_redundancy_(MEDIUM), estimated_loss_rate_(0.0f) {}
    
    /**
     * 更新丢包率估计（使用EWMA平滑）
     * @param received 收到的包数
     * @param expected 期望的包数
     */
    void update(int received, int expected) {
        if (expected <= 0) return;
        
        float loss_rate = 1.0f - (float)received / expected;
        if (loss_rate < 0) loss_rate = 0;
        if (loss_rate > 1) loss_rate = 1;
        
        // EWMA 平滑
        estimated_loss_rate_ = ALPHA * loss_rate + (1 - ALPHA) * estimated_loss_rate_;
        
        // 根据丢包率调整冗余率
        if (estimated_loss_rate_ < THRESH_LOW) {
            current_redundancy_ = LOW;
        } else if (estimated_loss_rate_ > THRESH_HIGH) {
            current_redundancy_ = HIGH;
        } else {
            current_redundancy_ = MEDIUM;
        }
    }
    
    /**
     * 根据丢包率直接计算推荐冗余率
     */
    static float recommend(float loss_rate) {
        if (loss_rate < THRESH_LOW) return LOW;
        if (loss_rate > THRESH_HIGH) return HIGH;
        return MEDIUM;
    }
    
    float get_redundancy() const { return current_redundancy_; }
    float get_estimated_loss_rate() const { return estimated_loss_rate_; }
    
private:
    static constexpr float ALPHA = 0.3f;  // EWMA 平滑系数
    float current_redundancy_;
    float estimated_loss_rate_;
};

#endif // ADAPTIVE_FEC_HH
