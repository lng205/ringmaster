#include "redundancy_controller.hh"

#include <algorithm>
#include <cmath>

using namespace std;

float RedundancyController::update(const uint32_t packets_sent,
                                   const uint32_t acks_received)
{
  if (packets_sent == 0) {
    // No data sent, retain current state or return 0?
    // Returning 0 redundancy is safer to avoid unnecessary overhead when idle.
    return 0.0f; 
  }

  // 1. Calculate Instantaneous Loss
  double sample_loss = 1.0 - static_cast<double>(acks_received) / packets_sent;
  sample_loss = max(0.0, min(1.0, sample_loss));

  // 2. Update EWMA
  if (not ewma_loss_rate_) {
    ewma_loss_rate_ = sample_loss;
  } else {
    ewma_loss_rate_ = ALPHA * sample_loss + (1.0 - ALPHA) * (*ewma_loss_rate_);
  }

  double loss = *ewma_loss_rate_;

  // 3. Calculate Redundancy
  // R = (L * Factor) / (1 - (L * Factor))
  double target_loss = loss * SAFETY_FACTOR;
  
  // Protect against division by zero or negative denominator if loss is very high
  if (target_loss >= 1.0) {
    return MAX_REDUNDANCY;
  }

  double redundancy = target_loss / (1.0 - target_loss);

  // 4. Clamp
  if (redundancy < 0.0) redundancy = 0.0;
  if (redundancy > MAX_REDUNDANCY) redundancy = MAX_REDUNDANCY;

  return static_cast<float>(redundancy);
}

void RedundancyController::reset()
{
  ewma_loss_rate_.reset();
}
