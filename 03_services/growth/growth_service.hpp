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
        const PositionSnapshot& position,
        GrowthDecision& decision) noexcept;
    void clear_runtime_state() noexcept;
    [[nodiscard]] GrowthSnapshot snapshot() const noexcept;

private:
    [[nodiscard]] static std::size_t source_index(GrowthSource source) noexcept;

    GrowthConfig config_;
    GrowthSnapshot snapshot_{};
    std::array<std::uint64_t, 5> last_accepted_us_{};
    std::array<bool, 5> source_seen_{};
    std::uint64_t pending_since_us_{0};
};

}  // namespace plant
