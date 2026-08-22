#pragma once

#include <cstdint>

namespace plant {

struct BoundedRetryDecision final {
    std::uint8_t next_failure_count{0};
    std::uint32_t delay_ms{0};
    bool report_failure{false};
};

constexpr BoundedRetryDecision bounded_retry_backoff(
    std::uint8_t current_failure_count,
    std::uint8_t maximum_failures,
    std::uint32_t retry_delay_ms,
    std::uint32_t fault_retry_delay_ms) noexcept {
    if (maximum_failures == 0 ||
        current_failure_count >= static_cast<std::uint8_t>(maximum_failures - 1U)) {
        return BoundedRetryDecision{0, fault_retry_delay_ms, true};
    }
    return BoundedRetryDecision{
        static_cast<std::uint8_t>(current_failure_count + 1U), retry_delay_ms, false};
}

}  // namespace plant
