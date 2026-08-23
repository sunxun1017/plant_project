#include "03_services/lighting/light_arbitration_service.hpp"

#include <algorithm>

#include "01_core/domain/sensing.hpp"

namespace plant {

LightArbitrationService::LightArbitrationService(ILightPort& output) noexcept
    : output_(output) {}

Status LightArbitrationService::play(LightCue cue, std::uint32_t) {
    const LightLayer layer = cue == LightCue::Error
                                 ? LightLayer::Error
                                 : LightLayer::ForegroundBehavior;
    return request(layer, cue, foreground_intensity_, last_tick_us_);
}

Status LightArbitrationService::stop() {
    clear(LightLayer::ForegroundBehavior);
    clear(LightLayer::Error);
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
    LightLayer selected_layer = LightLayer::Sunlight;
    Request selected{};
    for (std::size_t request_index = 0; request_index < requests_.size(); ++request_index) {
        if (!requests_[request_index].active) {
            continue;
        }
        const auto layer = static_cast<LightLayer>(request_index);
        if (!found || priority(layer) > priority(selected_layer)) {
            found = true;
            selected_layer = layer;
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
    if (!output_active_ || selected_layer != active_layer_ ||
        selected.cue != active_cue_) {
        const Status intensity_status = output_.set_intensity(selected.intensity);
        if (!intensity_status.ok() && intensity_status.code() != ErrorCode::Unsupported) {
            return intensity_status;
        }
        const Status play_status = output_.play(selected.cue, 0);
        if (!play_status.ok()) {
            return play_status;
        }
        output_active_ = true;
        active_layer_ = selected_layer;
        active_cue_ = selected.cue;
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
    LightLayer layer,
    LightCue cue,
    std::uint16_t intensity,
    std::uint64_t now_us,
    std::uint64_t duration_us) {
    const std::size_t request_index = index(layer);
    if (request_index >= requests_.size() || intensity > kNormalizedSensorMaximum) {
        return Status::failure(ErrorCode::InvalidArgument);
    }
    requests_[request_index] = Request{
        cue,
        intensity,
        duration_us == 0 ? 0 : now_us + duration_us,
        true,
    };
    return Status::success();
}

void LightArbitrationService::clear(LightLayer layer) noexcept {
    const std::size_t request_index = index(layer);
    if (request_index < requests_.size()) {
        requests_[request_index].active = false;
    }
}

bool LightArbitrationService::emitting() const noexcept {
    return output_active_;
}

LightCue LightArbitrationService::active_cue() const noexcept {
    return active_cue_;
}

bool LightArbitrationService::interferes_with_illumination() const noexcept {
    if (!output_active_) {
        return false;
    }
    // 仅冻结强烈或瞬态灯效。Speech/Climate/Sunlight 是可能长期存在的背景反馈；
    // 若把它们也作为遮罩，光照服务会永远无法累计“晒太阳”时间。
    return active_layer_ == LightLayer::Touch ||
           active_layer_ == LightLayer::ForegroundBehavior ||
           active_layer_ == LightLayer::Growth ||
           active_layer_ == LightLayer::Error;
}

std::size_t LightArbitrationService::index(LightLayer layer) noexcept {
    return static_cast<std::size_t>(layer);
}

std::uint8_t LightArbitrationService::priority(LightLayer layer) noexcept {
    switch (layer) {
        case LightLayer::Sunlight:
            return 10;
        case LightLayer::Climate:
            return 20;
        case LightLayer::Speech:
            return 30;
        case LightLayer::Touch:
            return 40;
        case LightLayer::ForegroundBehavior:
            return 50;
        case LightLayer::Growth:
            return 60;
        case LightLayer::Error:
            return 70;
    }
    return 0;
}

}  // namespace plant
