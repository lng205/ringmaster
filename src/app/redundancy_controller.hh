#ifndef REDUNDANCY_CONTROLLER_HH
#define REDUNDANCY_CONTROLLER_HH

#include <cstdint>
#include <optional>

class RedundancyController
{
public:
  RedundancyController() = default;

  /**
   * Update the controller with the latest statistics and calculate the new redundancy.
   *
   * @param packets_sent Number of packets sent in the last interval.
   * @param acks_received Number of ACKs received in the last interval.
   * @return The calculated redundancy ratio (0.0 to 2.0).
   */
  float update(uint32_t packets_sent, uint32_t acks_received);

  /**
   * Get the current smoothed loss rate.
   * @return The EWMA loss rate (0.0 to 1.0).
   */
  double loss_rate() const { return ewma_loss_rate_.value_or(0.0); }

  /**
   * Reset the controller state.
   */
  void reset();

private:
  // EWMA parameters
  static constexpr double ALPHA = 0.2;
  static constexpr double SAFETY_FACTOR = 1.1;
  static constexpr float MAX_REDUNDANCY = 0.5f;

  // State
  std::optional<double> ewma_loss_rate_ {};
};

#endif /* REDUNDANCY_CONTROLLER_HH */
