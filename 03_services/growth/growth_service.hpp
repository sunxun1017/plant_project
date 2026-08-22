#pragma once

#include <array>
#include <cstdint>

#include "01_core/common/status.hpp"
#include "01_core/domain/configuration.hpp"
#include "01_core/domain/sensing.hpp"

namespace plant {

class GrowthService {
public:
    explicit GrowthService(GrowthConfig config = {}) noexcept;

    Status submit(GrowthSource source, std::uint64_t now_us) noexcept;
    Status evaluate(
        std::uint64_t now_us,
        bool foreground_available,
        bool inactivity_decay_available,
        const PositionSnapshot& position,
        GrowthDecision& decision) noexcept;
    void clear_runtime_state() noexcept;
    [[nodiscard]] GrowthSnapshot snapshot() const noexcept;

private:
    static constexpr std::size_t kMaximumPendingCredits = 4;
    [[nodiscard]] static std::size_t source_index(GrowthSource source) noexcept;
    void refresh_pending_snapshot() noexcept;
    void pop_pending() noexcept;

    GrowthConfig config_;
    GrowthSnapshot snapshot_{};
    std::array<std::uint64_t, 5> last_accepted_us_{};
    std::array<bool, 5> source_seen_{};
    std::array<GrowthSource, kMaximumPendingCredits> pending_sources_{};
    std::array<std::uint64_t, kMaximumPendingCredits> pending_since_us_{};
    std::size_t pending_head_{0};
    std::size_t pending_count_{0};
    std::uint64_t last_interaction_us_{0};
    std::uint64_t last_decay_us_{0};
    bool interaction_time_initialized_{false};
    bool decay_started_{false};
};

}  // namespace plant
