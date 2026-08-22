#include "03_services/growth/growth_service.hpp"

#include <algorithm>

namespace plant {

GrowthService::GrowthService(GrowthConfig config) noexcept : config_(config) {}

Status GrowthService::submit(GrowthSource source, std::uint64_t now_us) noexcept {
    if (source == GrowthSource::None || source_index(source) >= source_seen_.size()) {
        return Status::failure(ErrorCode::InvalidArgument);
    }
    const std::size_t index = source_index(source);
    if (source_seen_[index] && now_us >= last_accepted_us_[index] &&
        now_us - last_accepted_us_[index] < config_.cooldown_us[index]) {
        return Status::failure(ErrorCode::Busy);
    }
    if (snapshot_.pending) {
        return Status::failure(ErrorCode::Busy);
    }

    snapshot_.pending = true;
    snapshot_.pending_source = source;
    pending_since_us_ = now_us;
    source_seen_[index] = true;
    last_accepted_us_[index] = now_us;
    return Status::success();
}

Status GrowthService::evaluate(
    std::uint64_t now_us,
    bool foreground_available,
    const PositionSnapshot& position,
    GrowthDecision& decision) noexcept {
    decision = GrowthDecision{};
    if (!snapshot_.pending) {
        return Status::success();
    }
    if (now_us < pending_since_us_ ||
        (config_.pending_expiry_us != 0 &&
         now_us - pending_since_us_ >= config_.pending_expiry_us)) {
        snapshot_.pending = false;
        snapshot_.pending_source = GrowthSource::None;
        return Status::success();
    }
    if (!foreground_available) {
        return Status::success();
    }
    if (position.feedback != PositionFeedbackState::Valid ||
        position.actual_position > kNormalizedSensorMaximum) {
        snapshot_.pending = false;
        snapshot_.pending_source = GrowthSource::None;
        return Status::failure(ErrorCode::MotionFailure);
    }

    decision.source = snapshot_.pending_source;
    snapshot_.recent_source = snapshot_.pending_source;
    snapshot_.pending = false;
    snapshot_.pending_source = GrowthSource::None;

    const std::uint16_t limit =
        std::min(config_.maximum_position, kNormalizedSensorMaximum);
    if (position.actual_position >= limit ||
        limit - position.actual_position <= config_.limit_tolerance) {
        snapshot_.at_limit = true;
        decision.action = GrowthAction::ShowLimit;
        decision.target_position = position.actual_position;
        return Status::success();
    }

    snapshot_.at_limit = false;
    decision.action = GrowthAction::MoveToPosition;
    decision.target_position = static_cast<std::uint16_t>(std::min<std::uint32_t>(
        static_cast<std::uint32_t>(position.actual_position) + config_.step,
        limit));
    return Status::success();
}

void GrowthService::clear_runtime_state() noexcept {
    snapshot_ = GrowthSnapshot{};
    last_accepted_us_.fill(0);
    source_seen_.fill(false);
    pending_since_us_ = 0;
}

GrowthSnapshot GrowthService::snapshot() const noexcept {
    return snapshot_;
}

std::size_t GrowthService::source_index(GrowthSource source) noexcept {
    return static_cast<std::size_t>(source);
}

}  // namespace plant
