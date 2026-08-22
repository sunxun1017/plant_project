#include "03_services/sensing/acoustic_service.hpp"

#include <algorithm>

namespace plant {

AcousticService::AcousticService(AcousticDetectionConfig config) noexcept : config_(config) {
    snapshot_.noise_floor = config_.initial_noise_floor;
}

void AcousticService::set_enabled(bool enabled) noexcept {
    snapshot_.enabled = enabled;
    if (!enabled) {
        snapshot_.volume_level = 0;
        snapshot_.interference_masked = false;
        reset_activity();
    }
}

bool AcousticService::process(
    std::uint64_t now_us,
    const AcousticSample& sample,
    bool interference_masked) noexcept {
    snapshot_.interference_masked = interference_masked;
    if (!snapshot_.enabled) {
        reset_activity();
        return false;
    }
    if (!sample.valid || sample.volume_level > kNormalizedSensorMaximum) {
        snapshot_.state = AcousticState::SensorFault;
        snapshot_.volume_level = 0;
        time_initialized_ = false;
        return false;
    }

    snapshot_.volume_level = sample.volume_level;
    if (!time_initialized_ || now_us < last_sample_us_) {
        last_sample_us_ = now_us;
        window_started_us_ = now_us;
        time_initialized_ = true;
    }
    const std::uint64_t elapsed_us = now_us - last_sample_us_;
    last_sample_us_ = now_us;

    if (interference_masked) {
        // 执行器自噪声期间不学习底噪，也不延续一次用户讲话活动。
        reset_activity();
        last_sample_us_ = now_us;
        time_initialized_ = true;
        return false;
    }

    if (snapshot_.state == AcousticState::SensorFault) {
        enter_quiet();
        window_started_us_ = now_us;
    }

    if (snapshot_.state == AcousticState::Quiet) {
        snapshot_.noise_floor = static_cast<std::uint16_t>(
            (static_cast<std::uint32_t>(snapshot_.noise_floor) * 31U +
             sample.volume_level) /
            32U);
        if (sample.volume_level >= start_threshold()) {
            if (!start_candidate_active_) {
                start_candidate_active_ = true;
                start_candidate_us_ = now_us;
            }
            if (now_us - start_candidate_us_ >= config_.minimum_speech_us) {
                snapshot_.state = AcousticState::Speaking;
                speaking_accumulated_us_ = now_us - start_candidate_us_;
                window_started_us_ = start_candidate_us_;
                start_candidate_active_ = false;
            }
        } else {
            start_candidate_active_ = false;
        }
        return false;
    }

    if (now_us - window_started_us_ >= config_.sustained_window_us) {
        window_started_us_ = now_us;
        speaking_accumulated_us_ = 0;
    } else {
        speaking_accumulated_us_ += elapsed_us;
    }
    snapshot_.speaking_duration_ms = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(speaking_accumulated_us_ / 1000ULL, UINT32_MAX));

    if (sample.volume_level <= stop_threshold()) {
        if (!below_stop_active_) {
            below_stop_active_ = true;
            below_stop_since_us_ = now_us;
        }
        if (now_us - below_stop_since_us_ >= config_.speech_end_hold_us) {
            enter_quiet();
            return false;
        }
    } else {
        below_stop_active_ = false;
    }

    if (snapshot_.state == AcousticState::Speaking &&
        speaking_accumulated_us_ >= config_.sustained_required_us) {
        snapshot_.state = AcousticState::SustainedSpeech;
        return true;
    }
    return false;
}

AcousticSnapshot AcousticService::snapshot() const noexcept {
    return snapshot_;
}

void AcousticService::reset_activity() noexcept {
    snapshot_.state = AcousticState::Quiet;
    snapshot_.speaking_duration_ms = 0;
    start_candidate_active_ = false;
    below_stop_active_ = false;
    speaking_accumulated_us_ = 0;
    time_initialized_ = false;
}

void AcousticService::enter_quiet() noexcept {
    snapshot_.state = AcousticState::Quiet;
    snapshot_.speaking_duration_ms = 0;
    start_candidate_active_ = false;
    below_stop_active_ = false;
    speaking_accumulated_us_ = 0;
}

std::uint16_t AcousticService::start_threshold() const noexcept {
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(
        static_cast<std::uint32_t>(snapshot_.noise_floor) + config_.speech_start_margin,
        kNormalizedSensorMaximum));
}

std::uint16_t AcousticService::stop_threshold() const noexcept {
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(
        static_cast<std::uint32_t>(snapshot_.noise_floor) + config_.speech_stop_margin,
        kNormalizedSensorMaximum));
}

}  // namespace plant
