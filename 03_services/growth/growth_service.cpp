#include "03_services/growth/growth_service.hpp"

#include <algorithm>

namespace plant {

GrowthService::GrowthService(GrowthConfig config) noexcept : config_(config) {}

Status GrowthService::submit(GrowthSource source, std::uint64_t now_us) noexcept {
    if (source == GrowthSource::None || source == GrowthSource::InactivityDecay ||
        source_index(source) >= source_seen_.size()) {
        return Status::failure(ErrorCode::InvalidArgument);
    }
    last_interaction_us_ = now_us;
    interaction_time_initialized_ = true;
    decay_started_ = false;
    const std::size_t index = source_index(source);
    if (source_seen_[index] && now_us >= last_accepted_us_[index] &&
        now_us - last_accepted_us_[index] < config_.cooldown_us[index]) {
        return Status::failure(ErrorCode::Busy);
    }
    const std::size_t capacity = std::min<std::size_t>(
        config_.maximum_pending_credits, kMaximumPendingCredits);
    if (capacity == 0 || pending_count_ >= capacity) {
        return Status::failure(ErrorCode::Busy);
    }

    const std::size_t tail = (pending_head_ + pending_count_) % kMaximumPendingCredits;
    pending_sources_[tail] = source;
    pending_since_us_[tail] = now_us;
    ++pending_count_;
    refresh_pending_snapshot();
    source_seen_[index] = true;
    last_accepted_us_[index] = now_us;
    return Status::success();
}

Status GrowthService::evaluate(
    std::uint64_t now_us,
    bool foreground_available,
    bool inactivity_decay_available,
    const PositionSnapshot& position,
    GrowthDecision& decision) noexcept {
    decision = GrowthDecision{};
    if (!interaction_time_initialized_ || now_us < last_interaction_us_ ||
        (decay_started_ && now_us < last_decay_us_)) {
        last_interaction_us_ = now_us;
        last_decay_us_ = now_us;
        interaction_time_initialized_ = true;
        decay_started_ = false;
    }

    while (pending_count_ != 0) {
        const std::uint64_t pending_since = pending_since_us_[pending_head_];
        if (now_us >= pending_since &&
            (config_.pending_expiry_us == 0 ||
             now_us - pending_since < config_.pending_expiry_us)) {
            break;
        }
        pop_pending();
    }

    if (pending_count_ != 0) {
        if (!foreground_available) {
            return Status::success();
        }
        if (position.feedback != PositionFeedbackState::Valid ||
            position.actual_position > kNormalizedSensorMaximum) {
            pop_pending();
            return Status::failure(ErrorCode::MotionFailure);
        }

        decision.source = pending_sources_[pending_head_];
        snapshot_.recent_source = decision.source;
        pop_pending();

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

    if (!inactivity_decay_available || config_.decay_step == 0 ||
        config_.inactivity_before_decay_us == 0 || config_.decay_interval_us == 0) {
        return Status::success();
    }
    const bool decay_due = !decay_started_
                               ? now_us >= last_interaction_us_ &&
                                     now_us - last_interaction_us_ >=
                                         config_.inactivity_before_decay_us
                               : now_us >= last_decay_us_ &&
                                     now_us - last_decay_us_ >= config_.decay_interval_us;
    if (!decay_due) {
        return Status::success();
    }
    if (position.feedback != PositionFeedbackState::Valid ||
        position.actual_position > kNormalizedSensorMaximum) {
        return Status::failure(ErrorCode::MotionFailure);
    }

    decay_started_ = true;
    last_decay_us_ = now_us;
    const std::uint16_t minimum =
        std::min(config_.minimum_position, kNormalizedSensorMaximum);
    if (position.actual_position <= minimum) {
        return Status::success();
    }
    decision.action = GrowthAction::MoveToPosition;
    decision.source = GrowthSource::InactivityDecay;
    decision.target_position = static_cast<std::uint16_t>(
        position.actual_position - minimum <= config_.decay_step
            ? minimum
            : position.actual_position - config_.decay_step);
    snapshot_.recent_source = GrowthSource::InactivityDecay;
    snapshot_.at_limit = false;
    return Status::success();
}

void GrowthService::clear_runtime_state() noexcept {
    snapshot_ = GrowthSnapshot{};
    last_accepted_us_.fill(0);
    source_seen_.fill(false);
    pending_sources_.fill(GrowthSource::None);
    pending_since_us_.fill(0);
    pending_head_ = 0;
    pending_count_ = 0;
    last_interaction_us_ = 0;
    last_decay_us_ = 0;
    interaction_time_initialized_ = false;
    decay_started_ = false;
}

GrowthSnapshot GrowthService::snapshot() const noexcept {
    return snapshot_;
}

std::size_t GrowthService::source_index(GrowthSource source) noexcept {
    return static_cast<std::size_t>(source);
}

void GrowthService::refresh_pending_snapshot() noexcept {
    snapshot_.pending_count = static_cast<std::uint8_t>(pending_count_);
    snapshot_.pending = pending_count_ != 0;
    snapshot_.pending_source = snapshot_.pending
                                   ? pending_sources_[pending_head_]
                                   : GrowthSource::None;
}

void GrowthService::pop_pending() noexcept {
    if (pending_count_ == 0) {
        return;
    }
    pending_sources_[pending_head_] = GrowthSource::None;
    pending_since_us_[pending_head_] = 0;
    pending_head_ = (pending_head_ + 1) % kMaximumPendingCredits;
    --pending_count_;
    refresh_pending_snapshot();
}

}  // namespace plant
