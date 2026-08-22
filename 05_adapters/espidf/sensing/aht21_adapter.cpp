#include "05_adapters/espidf/sensing/aht21_adapter.hpp"

#include <array>
#include <limits>

#include "06_bsp/plant_v2/plant_v2_board.hpp"
#include "esp_timer.h"

namespace plant {
namespace {

using Config = bsp::v2::BoardConfig;
constexpr std::uint8_t kReadyMask = 0x18;
constexpr std::uint8_t kBusyMask = 0x80;

}  // namespace

Aht21Adapter::Aht21Adapter(V2I2cBus& bus) noexcept : bus_(bus) {}

Status Aht21Adapter::initialize() {
    const Status status = bus_.add_device(
        Config::Climate::address,
        Config::Climate::i2c_frequency_hz,
        device_);
    if (!status.ok()) {
        return status;
    }
    initialized_ = true;
    enabled_ = true;
    state_ = State::PowerUp;
    due_us_ = static_cast<std::uint64_t>(esp_timer_get_time()) +
              Config::Climate::power_up_delay_ms * 1000ULL;
    return Status::success();
}

Status Aht21Adapter::set_enabled(bool enabled) {
    if (!initialized_) {
        return Status::failure(ErrorCode::InvalidState);
    }
    enabled_ = enabled;
    retry_count_ = 0;
    state_ = State::PowerUp;
    due_us_ = enabled
                  ? static_cast<std::uint64_t>(esp_timer_get_time()) +
                        Config::Climate::power_up_delay_ms * 1000ULL
                  : 0;
    return Status::success();
}

Status Aht21Adapter::poll(
    std::uint64_t now_us,
    ClimateSample& sample,
    bool& available) {
    sample = ClimateSample{};
    available = false;
    if (!initialized_) {
        return Status::failure(ErrorCode::InvalidState);
    }
    if (!enabled_ || now_us < due_us_) {
        return Status::success();
    }

    if (state_ == State::PowerUp) {
        std::uint8_t status_byte = 0;
        const Status status = read_status(status_byte);
        if (!status.ok()) {
            ++retry_count_;
            if (retry_count_ >= Config::Climate::maximum_retries) {
                sample = ClimateSample{};
                available = true;
                retry_count_ = 0;
            }
            state_ = State::PowerUp;
            due_us_ = now_us + Config::Climate::power_up_delay_ms * 1000ULL;
            return Status::success();
        }
        if ((status_byte & kReadyMask) != kReadyMask) {
            // AHT21 的上电状态不满足 0x18 时先软复位并重新走非阻塞上电窗口；
            // 不复用其他系列器件的 0xBE 初始化命令。
            constexpr std::uint8_t reset_command = 0xBA;
            if (i2c_master_transmit(
                    device_,
                    &reset_command,
                    1,
                    Config::Climate::transfer_timeout_ms) != ESP_OK) {
                record_failure(now_us, sample, available);
                return Status::success();
            }
            ++retry_count_;
            if (retry_count_ >= Config::Climate::maximum_retries) {
                sample = ClimateSample{};
                available = true;
                retry_count_ = 0;
            }
            state_ = State::PowerUp;
            due_us_ = now_us + Config::Climate::power_up_delay_ms * 1000ULL;
            return Status::success();
        }
        state_ = State::Idle;
        due_us_ = now_us;
        retry_count_ = 0;
    }

    if (state_ == State::Idle) {
        const Status status = start_measurement();
        if (!status.ok()) {
            record_failure(now_us, sample, available);
            return Status::success();
        }
        state_ = State::Measuring;
        due_us_ = now_us + Config::Climate::measurement_time_ms * 1000ULL;
        return Status::success();
    }

    const Status status = finish_measurement(sample);
    if (!status.ok()) {
        record_failure(now_us, sample, available);
        return Status::success();
    }
    retry_count_ = 0;
    available = true;
    state_ = State::Idle;
    due_us_ = now_us + Config::Climate::sample_period_ms * 1000ULL;
    return Status::success();
}

Status Aht21Adapter::read_status(std::uint8_t& status) const {
    constexpr std::uint8_t command = 0x71;
    return i2c_master_transmit_receive(
               device_,
               &command,
               1,
               &status,
               1,
               Config::Climate::transfer_timeout_ms) == ESP_OK
               ? Status::success()
               : Status::failure(ErrorCode::SensorFailure);
}

Status Aht21Adapter::start_measurement() {
    constexpr std::array<std::uint8_t, 3> command{0xAC, 0x33, 0x00};
    return i2c_master_transmit(
               device_,
               command.data(),
               command.size(),
               Config::Climate::transfer_timeout_ms) == ESP_OK
               ? Status::success()
               : Status::failure(ErrorCode::SensorFailure);
}

Status Aht21Adapter::finish_measurement(ClimateSample& sample) {
    std::array<std::uint8_t, 7> data{};
    if (i2c_master_receive(
            device_, data.data(), data.size(), Config::Climate::transfer_timeout_ms) != ESP_OK ||
        (data[0] & (kBusyMask | kReadyMask)) != kReadyMask ||
        crc8(data.data(), 6) != data[6]) {
        return Status::failure(ErrorCode::SensorFailure);
    }

    const std::uint32_t raw_humidity =
        (static_cast<std::uint32_t>(data[1]) << 12U) |
        (static_cast<std::uint32_t>(data[2]) << 4U) |
        (static_cast<std::uint32_t>(data[3]) >> 4U);
    const std::uint32_t raw_temperature =
        (static_cast<std::uint32_t>(data[3] & 0x0FU) << 16U) |
        (static_cast<std::uint32_t>(data[4]) << 8U) |
        data[5];
    const std::int32_t temperature =
        static_cast<std::int32_t>(raw_temperature * 20000ULL / 1048576ULL) - 5000;
    if (temperature < std::numeric_limits<std::int16_t>::min() ||
        temperature > std::numeric_limits<std::int16_t>::max()) {
        return Status::failure(ErrorCode::SensorFailure);
    }
    sample.temperature_centi_c = static_cast<std::int16_t>(temperature);
    sample.relative_humidity_tenths_percent = static_cast<std::uint16_t>(
        raw_humidity * 1000ULL / 1048576ULL);
    sample.valid = sample.temperature_centi_c >= -4000 &&
                   sample.temperature_centi_c <= 8500 &&
                   sample.relative_humidity_tenths_percent <= 1000;
    return sample.valid ? Status::success() : Status::failure(ErrorCode::SensorFailure);
}

void Aht21Adapter::record_failure(
    std::uint64_t now_us,
    ClimateSample& sample,
    bool& available) noexcept {
    ++retry_count_;
    if (retry_count_ >= Config::Climate::maximum_retries) {
        sample = ClimateSample{};
        available = true;
        retry_count_ = 0;
        due_us_ = now_us + Config::Climate::sample_period_ms * 1000ULL;
    } else {
        due_us_ = now_us + Config::Climate::measurement_time_ms * 1000ULL;
    }
    state_ = State::Idle;
}

std::uint8_t Aht21Adapter::crc8(const std::uint8_t* data, std::size_t size) noexcept {
    std::uint8_t crc = 0xFF;
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit) {
            crc = static_cast<std::uint8_t>(
                (crc & 0x80U) != 0 ? (crc << 1U) ^ 0x31U : crc << 1U);
        }
    }
    return crc;
}

}  // namespace plant
