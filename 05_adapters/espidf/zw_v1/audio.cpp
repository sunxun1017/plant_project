#include "05_adapters/espidf/zw_v1/audio.hpp"
#include <array>
#include <cmath>
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace plant::zw_v1 {
namespace {
using namespace bsp::zw_v1;
struct AudioSession {
    i2s_chan_handle_t rx{};
    const audio_codec_ctrl_if_t* ctrl{};
    const audio_codec_data_if_t* data{};
    const audio_codec_gpio_if_t* gpio{};
    const audio_codec_if_t* codec{};
    esp_codec_dev_handle_t device{};
    ~AudioSession() {
        if (device) { esp_codec_dev_close(device); esp_codec_dev_delete(device); }
        if (codec) audio_codec_delete_codec_if(codec);
        if (data) audio_codec_delete_data_if(data);
        if (ctrl) audio_codec_delete_ctrl_if(ctrl);
        if (gpio) audio_codec_delete_gpio_if(gpio);
        if (rx) { i2s_channel_disable(rx); i2s_del_channel(rx); }
        for (const int pin : {Board::audio_mclk, Board::audio_bclk, Board::audio_lrck}) {
            gpio_reset_pin(static_cast<gpio_num_t>(pin));
            gpio_set_level(static_cast<gpio_num_t>(pin), 0);
            gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_OUTPUT);
            gpio_set_pull_mode(static_cast<gpio_num_t>(pin), GPIO_FLOATING);
        }
        gpio_reset_pin(static_cast<gpio_num_t>(Board::audio_din));
        gpio_set_pull_mode(static_cast<gpio_num_t>(Board::audio_din), GPIO_FLOATING);
    }
    esp_err_t capture(Hardware& hardware, std::uint16_t& level, bool measure) {
        i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
        auto err = i2s_new_channel(&channel, nullptr, &rx);
        if (err != ESP_OK) return err;
        i2s_std_config_t config{};
        config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000);
        config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
        config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
        config.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
        config.gpio_cfg.mclk = static_cast<gpio_num_t>(Board::audio_mclk);
        config.gpio_cfg.bclk = static_cast<gpio_num_t>(Board::audio_bclk);
        config.gpio_cfg.ws = static_cast<gpio_num_t>(Board::audio_lrck);
        config.gpio_cfg.dout = I2S_GPIO_UNUSED;
        config.gpio_cfg.din = static_cast<gpio_num_t>(Board::audio_din);
        if ((err = i2s_channel_init_std_mode(rx, &config)) != ESP_OK) return err;
        audio_codec_i2c_cfg_t control{};
        control.port = I2C_NUM_0;
        control.addr = Board::codec_address << 1; // esp_codec_dev expects 8-bit address
        control.bus_handle = hardware.bus();
        ctrl = audio_codec_new_i2c_ctrl(&control);
        audio_codec_i2s_cfg_t transport{};
        transport.port = I2S_NUM_0;
        transport.rx_handle = rx;
        data = audio_codec_new_i2s_data(&transport);
        gpio = audio_codec_new_gpio();
        if (!ctrl || !data || !gpio) return ESP_ERR_NO_MEM;
        es8311_codec_cfg_t codec_config{};
        codec_config.ctrl_if = ctrl;
        codec_config.gpio_if = gpio;
        codec_config.codec_mode = ESP_CODEC_DEV_WORK_MODE_ADC;
        codec_config.use_mclk = true;
        codec_config.pa_pin = -1;
        codec_config.mclk_div = 256;
        codec = es8311_codec_new(&codec_config);
        if (!codec) return ESP_FAIL;
        esp_codec_dev_cfg_t dev_config{};
        dev_config.dev_type = ESP_CODEC_DEV_TYPE_IN;
        dev_config.codec_if = codec;
        dev_config.data_if = data;
        device = esp_codec_dev_new(&dev_config);
        if (!device) return ESP_ERR_NO_MEM;
        esp_codec_dev_sample_info_t sample{};
        sample.bits_per_sample = 16;
        sample.channel = 1;
        sample.sample_rate = 16000;
        if (esp_codec_dev_open(device, &sample) != ESP_CODEC_DEV_OK) return ESP_FAIL;
        if (!measure) return codec->enable(codec, false) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
        if (esp_codec_dev_set_in_gain(device, 24.0f) != ESP_CODEC_DEV_OK) return ESP_FAIL;
        // Bounded read even if no microphone data arrives. Read directly from
        // IDF after codec configuration so a broken clock cannot block forever.
        std::array<std::int16_t, 1600> pcm{};
        std::size_t bytes{};
        if ((err = i2s_channel_read(rx, pcm.data(), sizeof(pcm), &bytes, 1000)) != ESP_OK) return err;
        if (bytes != sizeof(pcm)) return ESP_ERR_INVALID_SIZE;
        // Discard the first block after startup; DC-block the second block.
        if ((err = i2s_channel_read(rx, pcm.data(), sizeof(pcm), &bytes, 1000)) != ESP_OK) return err;
        if (bytes != sizeof(pcm)) return ESP_ERR_INVALID_SIZE;
        double mean = 0;
        for (auto value : pcm) mean += value;
        mean /= pcm.size();
        double sum = 0;
        for (auto value : pcm) { const double centered = value - mean; sum += centered * centered; }
        level = static_cast<std::uint16_t>(std::min(1000.0, std::sqrt(sum / pcm.size()) * 1000.0 / 32768.0));
        return codec->enable(codec, false) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
    }
};
}
esp_err_t capture_audio_level(Hardware& hardware, std::uint16_t& level) {
    level = 0;
    auto err = hardware.set_output(bsp::zw_v1::Output::Audio, true);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(30) + 1);
    {
        AudioSession session;
        err = session.capture(hardware, level, true);
    } // codec standby + stop I2S + clocks low; keep DVDD live alongside AVDD/I2C
    return err;
}
esp_err_t prepare_audio_standby(Hardware& hardware) {
    std::uint16_t ignored{};
    AudioSession session;
    return session.capture(hardware, ignored, false);
}
}
