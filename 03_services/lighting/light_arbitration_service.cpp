#include "03_services/lighting/light_arbitration_service.hpp"

#include <algorithm>

#include "01_core/domain/sensing.hpp"

namespace plant {

LightArbitrationService::LightArbitrationService(ILightPort& output) noexcept
    : output_(output) {}

Status LightArbitrationService::play(
    LightPattern pattern,
    std::uint32_t) {
    const LightRequestSource source = pattern == LightPattern::ErrorBlink
                                          ? LightRequestSource::Error
                                          : LightRequestSource::ForegroundBehavior;
    return request(source, pattern, foreground_intensity_, last_tick_us_);
}

Status LightArbitrationService::stop() {
    clear(LightRequestSource::ForegroundBehavior);
    clear(LightRequestSource::Error);
    return Status::success();
}

Status LightArbitrationService::tick(std::uint64_t now_us) {
    last_tick_us_ = now_us;
    for (Request& request : requests_) {
        if (request.active && request.expires_us != 0 && now_us >= request.expires_us) {
            request.active = false;
        }
    }

    bool found = false;
    LightRequestSource selected_source = LightRequestSource::Sunlight;
    Request selected{};
    for (std::size_t request_index = 0; request_index < requests_.size(); ++request_index) {
        if (!requests_[request_index].active) {
            continue;
        }
        const auto source = static_cast<LightRequestSource>(request_index);
        if (!found || priority(source) > priority(selected_source)) {
            found = true;
            selected_source = source;
            selected = requests_[request_index];
        }
    }

    if (!found) {
        if (output_active_) {
            output_active_ = false;
            return output_.stop();
        }
        return Status::success();
    }
    if (!output_active_ || selected_source != active_source_ ||
        selected.pattern != active_pattern_) {
        const Status intensity_status = output_.set_intensity(selected.intensity);
        if (!intensity_status.ok() && intensity_status.code() != ErrorCode::Unsupported) {
            return intensity_status;
        }
        const Status play_status = output_.play(selected.pattern, 0);
        if (!play_status.ok()) {
            return play_status;
        }
        output_active_ = true;
        active_source_ = selected_source;
        active_pattern_ = selected.pattern;
    } else {
        const Status intensity_status = output_.set_intensity(selected.intensity);
        if (!intensity_status.ok() && intensity_status.code() != ErrorCode::Unsupported) {
            return intensity_status;
        }
    }
    return output_.tick(now_us);
}

Status LightArbitrationService::set_intensity(std::uint16_t intensity) {
    foreground_intensity_ = std::min(intensity, kNormalizedSensorMaximum);
    return Status::success();
}

Status LightArbitrationService::request(
    LightRequestSource source,
    LightPattern pattern,
    std::uint16_t intensity,
    std::uint64_t now_us,
    std::uint64_t duration_us) {
    const std::size_t request_index = index(source);
    if (request_index >= requests_.size() || intensity > kNormalizedSensorMaximum) {
        return Status::failure(ErrorCode::InvalidArgument);
    }
    requests_[request_index] = Request{
        pattern,
        intensity,
        duration_us == 0 ? 0 : now_us + duration_us,
        true,
    };
    return Status::success();
}

void LightArbitrationService::clear(LightRequestSource source) noexcept {
    const std::size_t request_index = index(source);
    if (request_index < requests_.size()) {
        requests_[request_index].active = false;
    }
}

bool LightArbitrationService::emitting() const noexcept {
    return output_active_;
}

LightPattern LightArbitrationService::active_pattern() const noexcept {
    return active_pattern_;
}

bool LightArbitrationService::interferes_with_illumination() const noexcept {
    if (!output_active_) {
        return false;
    }
    // 仅冻结强烈或瞬态灯效。Speech/Climate/Sunlight 是可能长期存在的背景反馈；
    // 若把它们也作为遮罩，光照服务会永远无法累计“晒太阳”时间。
    return active_source_ == LightRequestSource::Touch ||
           active_source_ == LightRequestSource::ForegroundBehavior ||
           active_source_ == LightRequestSource::Growth ||
           active_source_ == LightRequestSource::Error;
}

std::size_t LightArbitrationService::index(LightRequestSource source) noexcept {
    return static_cast<std::size_t>(source);
}

std::uint8_t LightArbitrationService::priority(LightRequestSource source) noexcept {
    switch (source) {
        case LightRequestSource::Sunlight:
            return 10;
        case LightRequestSource::Climate:
            return 20;
        case LightRequestSource::Speech:
            return 30;
        case LightRequestSource::Touch:
            return 40;
        case LightRequestSource::ForegroundBehavior:
            return 50;
        case LightRequestSource::Growth:
            return 60;
        case LightRequestSource::Error:
            return 70;
    }
    return 0;
}

}  // namespace plant
