#include "05_adapters/espidf/zw_v1/hardware.hpp"
#include "05_adapters/espidf/zw_v1/audio.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if !CONFIG_IDF_TARGET_ESP32C3
#error "ZW V1.0 requires ESP32-C3"
#endif
#if !CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
#error "Use USB Serial/JTAG console: UART0 pins are wired to servo and ES8311"
#endif

namespace {
constexpr char tag[] = "zw_v1";
plant::zw_v1::Hardware hardware;
TaskHandle_t task{};
void irq(void*) {
    // Level IRQ must be masked until the owning task has acknowledged PCF8574.
    gpio_intr_disable(static_cast<gpio_num_t>(plant::bsp::zw_v1::Board::expander_interrupt));
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(task, &woken);
    portYIELD_FROM_ISR(woken);
}
}
extern "C" void app_main() {
    using namespace plant::bsp::zw_v1;
    task = xTaskGetCurrentTaskHandle();
    ESP_LOGI(tag, "ZW V1.0 hardware bring-up; wake=%d; actuators/charging disabled",
             static_cast<int>(esp_sleep_get_wakeup_cause()));
    const auto init = hardware.initialize();
    if (init != ESP_OK) {
        ESP_LOGE(tag, "initialization failed: %s; no actuator enable", esp_err_to_name(init));
        ESP_LOGE(tag, "See 00_docs/hardware/zw_v1_review.md and ZW_BOARD_POWER_PATH_VERIFIED");
        return;
    }
    const auto standby_err = plant::zw_v1::prepare_audio_standby(hardware);
    if (standby_err != ESP_OK) {
        ESP_LOGE(tag, "codec standby not confirmed: %s; check power/I2C before sleep testing", esp_err_to_name(standby_err));
        return;
    }
#if CONFIG_ZW_TEST_AUDIO
    std::uint16_t level{};
    const auto audio_err = plant::zw_v1::capture_audio_level(hardware, level);
    ESP_LOGI(tag, "ES8311 level=%u result=%s", level, esp_err_to_name(audio_err));
    if (!hardware.healthy()) return;
#endif
    const auto pin = static_cast<gpio_num_t>(Board::expander_interrupt);
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(pin, irq, nullptr));
    ESP_ERROR_CHECK(gpio_set_intr_type(pin, GPIO_INTR_LOW_LEVEL));
    ESP_ERROR_CHECK(gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL));
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
    esp_pm_config_t pm{};
    pm.max_freq_mhz = 160;
    pm.min_freq_mhz = 40;
    pm.light_sleep_enable = true;
    ESP_ERROR_CHECK(esp_pm_configure(&pm));
    // No unconditional 20ms loop. Idle task blocks until touch or sensor deadline.
    bool last_pressed = false;
    bool candidate = false;
    std::int64_t candidate_since = 0;
    std::int64_t next_sample = 0;
    const auto started = esp_timer_get_time();
    while (true) {
        bool pressed{};
        const auto touch_err = hardware.read_touch(pressed);
        auto now = esp_timer_get_time();
        if (touch_err != ESP_OK) {
            ESP_LOGE(tag, "touch I2C failed: %s", esp_err_to_name(touch_err));
            // A stuck LOW IRQ must not create an interrupt / light-sleep storm.
            gpio_intr_disable(pin);
            gpio_wakeup_disable(pin);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (pressed != candidate) { candidate = pressed; candidate_since = now; }
        if (candidate != last_pressed && now - candidate_since >= 30000) {
            last_pressed = candidate;
            ESP_LOGI(tag, "touch=%s", last_pressed ? "pressed" : "released");
        }
        // A sensor cycle writes the expander and may clear its interrupt. Touch
        // is sampled again next iteration; debounce uses actual input, not IRQ.
        if (now >= next_sample) {
            auto err = hardware.set_output(Output::PeripheralRail, true);
            plant::zw_v1::ClimateReading climate{};
            if (err == ESP_OK) err = hardware.read_climate(climate);
            if (err == ESP_OK) ESP_LOGI(tag, "temperature=%d centi-C humidity=%u tenths-percent", climate.temperature_centi_c, climate.humidity_tenths_percent);
            else ESP_LOGW(tag, "GXHT40 invalid: %s", esp_err_to_name(err));
            const auto off_err = hardware.all_loads_off();
            if (off_err != ESP_OK) {
                ESP_LOGE(tag, "cannot confirm loads off: %s; halted", esp_err_to_name(off_err));
                return;
            }
            plant::zw_v1::ChargerReading charger{};
            if (hardware.read_charger(charger) == ESP_OK) {
                ESP_LOGI(tag, "BQ24259 status=0x%02x latched_fault=0x%02x current_fault=0x%02x; SOC unavailable", charger.status, charger.latched_fault, charger.current_fault);
            }
            next_sample = esp_timer_get_time() + 30000000;
            continue; // re-read P5 after output writes before blocking
        }
#if CONFIG_ZW_TEST_DEEP_SLEEP
        if (!last_pressed && !candidate && now - started >= 30000000) {
            const auto err = hardware.enter_deep_sleep(60000000);
            ESP_LOGW(tag, "sleep deferred: %s", esp_err_to_name(err));
        }
#else
        (void)started;
#endif
        if (gpio_get_level(pin) == 0) {
            // A real edge races with read/ack too. Re-read once; if INT remains
            // low, back off instead of spinning with a permanently active wake.
            bool ignored{};
            hardware.read_touch(ignored);
            if (gpio_get_level(pin) == 0) {
                gpio_intr_disable(pin);
                gpio_wakeup_disable(pin);
                ESP_LOGW(tag, "PCF8574 INT remains low; retry in 1s");
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            continue;
        }
        ESP_ERROR_CHECK(gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL));
        ESP_ERROR_CHECK(gpio_intr_enable(pin));
        const auto remaining_ms = (next_sample - esp_timer_get_time()) / 1000;
        const auto wait_ms = candidate != last_pressed ? 10 : std::max<std::int64_t>(1, remaining_ms);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms) + 1);
    }
}
