#include <cstddef>
#include <cstdint>

#include "03_services/ota/ota_service.hpp"
#include "03_services/power/power_service.hpp"

namespace plant::test {

namespace {

class FakePower final : public IPowerPort {
public:
    Status enter_light_sleep() override {
        ++light_sleep_count;
        return next_status;
    }

    Status enter_deep_sleep(std::uint64_t timer_wakeup_us) override {
        ++deep_sleep_count;
        last_timer_wakeup_us = timer_wakeup_us;
        return next_status;
    }

    WakeSource wake_source() const override { return source; }

    Status next_status{};
    WakeSource source{WakeSource::Touch};
    int light_sleep_count{0};
    int deep_sleep_count{0};
    std::uint64_t last_timer_wakeup_us{0};
};

class FakeOta final : public IOtaPort {
public:
    std::size_t available_image_space() const override { return capacity; }

    Status begin(const OtaImageMetadata& metadata) override {
        ++begin_count;
        last_metadata = metadata;
        return begin_status;
    }

    Status write(std::size_t offset, const std::uint8_t*, std::size_t size) override {
        last_offset = offset;
        bytes_written += size;
        return write_status;
    }

    Status verify_and_activate(const OtaImageMetadata&) override {
        ++verify_count;
        return verify_status;
    }

    Status abort() override {
        ++abort_count;
        return Status::success();
    }

    std::size_t capacity{1024};
    Status begin_status{};
    Status write_status{};
    Status verify_status{};
    OtaImageMetadata last_metadata{};
    std::size_t last_offset{0};
    std::size_t bytes_written{0};
    int begin_count{0};
    int verify_count{0};
    int abort_count{0};
};

int failures = 0;

#define CHECK_LOCAL(condition)                                                               \
    do {                                                                                     \
        if (!(condition)) {                                                                  \
            ++failures;                                                                      \
        }                                                                                    \
    } while (false)

LifecycleService sleeping_lifecycle() {
    LifecycleService lifecycle;
    (void)lifecycle.finish_boot(true);
    (void)lifecycle.begin_behavior(Behavior::Sleep);
    (void)lifecycle.apply_behavior_outcome(BehaviorOutcome::CompletedSleeping);
    return lifecycle;
}

void test_power_rejects_deep_sleep_while_busy() {
    auto lifecycle = sleeping_lifecycle();
    FakePower port;
    PowerService power{lifecycle, port};

    CHECK_LOCAL(power.request_deep_sleep({true, false, false}).code() == ErrorCode::Busy);
    CHECK_LOCAL(port.deep_sleep_count == 0);
    CHECK_LOCAL(lifecycle.snapshot().power_mode == PowerMode::Active);
}

void test_power_enters_deep_sleep_and_records_wake() {
    auto lifecycle = sleeping_lifecycle();
    FakePower port;
    LowPowerConfig config{};
    config.timer_wakeup_us = 9000000;
    PowerService power{lifecycle, port, config};

    CHECK_LOCAL(power.request_deep_sleep({}).ok());
    CHECK_LOCAL(lifecycle.snapshot().power_mode == PowerMode::DeepSleep);
    CHECK_LOCAL(port.last_timer_wakeup_us == config.timer_wakeup_us);
    CHECK_LOCAL(power.handle_wake().ok());
    CHECK_LOCAL(power.last_wake_source() == WakeSource::Touch);
    CHECK_LOCAL(lifecycle.snapshot().state == DeviceState::Booting);
}

void test_power_port_failure_restores_active_mode() {
    auto lifecycle = sleeping_lifecycle();
    FakePower port;
    port.next_status = Status::failure(ErrorCode::InternalFailure);
    PowerService power{lifecycle, port};

    CHECK_LOCAL(power.request_light_sleep().code() == ErrorCode::InternalFailure);
    CHECK_LOCAL(lifecycle.snapshot().state == DeviceState::Sleeping);
    CHECK_LOCAL(lifecycle.snapshot().power_mode == PowerMode::Active);
}

OtaImageMetadata valid_metadata() {
    return OtaImageMetadata{0x504C414E, 1, 2, 4, true};
}

void test_ota_happy_path() {
    LifecycleService lifecycle;
    (void)lifecycle.finish_boot(true);
    FakeOta port;
    OtaService ota{lifecycle, port, {0x504C414E, 1, 1}};
    const std::uint8_t first[]{1, 2};
    const std::uint8_t second[]{3, 4};

    CHECK_LOCAL(ota.begin(valid_metadata()).ok());
    CHECK_LOCAL(lifecycle.snapshot().state == DeviceState::Updating);
    CHECK_LOCAL(ota.write_chunk(0, first, sizeof(first)).ok());
    CHECK_LOCAL(ota.write_chunk(2, second, sizeof(second)).ok());
    CHECK_LOCAL(ota.finish().ok());
    CHECK_LOCAL(ota.snapshot().state == OtaState::ReadyToReboot);
    CHECK_LOCAL(port.verify_count == 1);
    CHECK_LOCAL(lifecycle.snapshot().state == DeviceState::Booting);
}

void test_ota_rejects_unsigned_downgrade_and_wrong_offset() {
    LifecycleService lifecycle;
    (void)lifecycle.finish_boot(true);
    FakeOta port;
    OtaService ota{lifecycle, port, {0x504C414E, 1, 2}};
    auto metadata = valid_metadata();
    metadata.signed_image = false;
    CHECK_LOCAL(ota.begin(metadata).code() == ErrorCode::InvalidArgument);

    metadata.signed_image = true;
    CHECK_LOCAL(ota.begin(metadata).code() == ErrorCode::InvalidArgument);

    metadata.firmware_version = 3;
    CHECK_LOCAL(ota.begin(metadata).ok());
    const std::uint8_t bytes[]{1, 2};
    CHECK_LOCAL(ota.write_chunk(1, bytes, sizeof(bytes)).code() == ErrorCode::InvalidArgument);
    CHECK_LOCAL(port.bytes_written == 0);
}

void test_ota_verification_failure_keeps_current_firmware() {
    LifecycleService lifecycle;
    (void)lifecycle.finish_boot(true);
    FakeOta port;
    port.verify_status = Status::failure(ErrorCode::OtaFailure);
    OtaService ota{lifecycle, port, {0x504C414E, 1, 1}};
    const std::uint8_t image[]{1, 2, 3, 4};

    CHECK_LOCAL(ota.begin(valid_metadata()).ok());
    CHECK_LOCAL(ota.write_chunk(0, image, sizeof(image)).ok());
    CHECK_LOCAL(ota.finish().code() == ErrorCode::OtaFailure);
    CHECK_LOCAL(ota.snapshot().state == OtaState::Failed);
    CHECK_LOCAL(port.abort_count == 1);
    CHECK_LOCAL(lifecycle.snapshot().state == DeviceState::Idle);
}

}  // namespace

int run_ota_power_service_tests() {
    test_power_rejects_deep_sleep_while_busy();
    test_power_enters_deep_sleep_and_records_wake();
    test_power_port_failure_restores_active_mode();
    test_ota_happy_path();
    test_ota_rejects_unsigned_downgrade_and_wrong_offset();
    test_ota_verification_failure_keeps_current_firmware();
    return failures;
}

}  // namespace plant::test
