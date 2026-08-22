#include "03_services/sensing/illumination_service.hpp"

#include <algorithm>

namespace plant {

IlluminationService::IlluminationService(IlluminationDetectionConfig config) noexcept
    : config_(config) {}

bool IlluminationService::process(
    std::uint64_t now_us,
    const IlluminationSample& sample,
    bool self_light_masked) noexcept {
    snapshot_.self_light_masked = self_light_masked;
    if (!sample.valid || sample.relative_level > kNormalizedSensorMaximum) {
        snapshot_.state = IlluminationState::SensorFault;
        snapshot_.relative_level = 0;
        reset_exposure();
        return false;
    }
    snapshot_.relative_level = sample.relative_level;
    if (self_light_masked) {
        // RGB 可能照到 GL5528；屏蔽时保留当前显示状态，但不推进确认或照射时长。
        bright_candidate_active_ = false;
        exit_candidate_active_ = false;
        if (snapshot_.state == IlluminationState::BrightExposure) {
            exposure_started_us_ = now_us;
            last_credit_us_ = now_us;
            snapshot_.bright_duration_ms = 0;
        }
        return false;
    }

    if (snapshot_.state == IlluminationState::SensorFault) {
        snapshot_.state = non_bright_state(sample.relative_level);
    }

    if (snapshot_.state != IlluminationState::BrightExposure) {
        snapshot_.state = non_bright_state(sample.relative_level);
        if (sample.relative_level >= config_.bright_enter_threshold) {
            if (!bright_candidate_active_) {
                bright_candidate_active_ = true;
                bright_candidate_since_us_ = now_us;
            }
            if (now_us - bright_candidate_since_us_ >= config_.bright_confirm_us) {
                snapshot_.state = IlluminationState::BrightExposure;
                exposure_started_us_ = now_us;
                last_credit_us_ = now_us;
                snapshot_.bright_duration_ms = 0;
                bright_candidate_active_ = false;
            }
        } else {
            bright_candidate_active_ = false;
        }
        return false;
    }

    if (sample.relative_level <= config_.bright_exit_threshold) {
        if (!exit_candidate_active_) {
            exit_candidate_active_ = true;
            exit_candidate_since_us_ = now_us;
        }
        if (now_us - exit_candidate_since_us_ >= config_.bright_exit_hold_us) {
            snapshot_.state = non_bright_state(sample.relative_level);
            reset_exposure();
            return false;
        }
    } else {
        exit_candidate_active_ = false;
    }

    snapshot_.bright_duration_ms = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        (now_us - exposure_started_us_) / 1000ULL, UINT32_MAX));
    if (config_.exposure_credit_interval_us != 0 &&
        now_us - last_credit_us_ >= config_.exposure_credit_interval_us) {
        last_credit_us_ = now_us;
        return true;
    }
    return false;
}

void IlluminationService::reset_exposure() noexcept {
    snapshot_.bright_duration_ms = 0;
    bright_candidate_active_ = false;
    exit_candidate_active_ = false;
    exposure_started_us_ = 0;
    last_credit_us_ = 0;
}

IlluminationSnapshot IlluminationService::snapshot() const noexcept {
    return snapshot_;
}

IlluminationState IlluminationService::non_bright_state(std::uint16_t level) const noexcept {
    return level <= config_.dark_threshold ? IlluminationState::Dark
                                           : IlluminationState::Ambient;
}

}  // namespace plant
