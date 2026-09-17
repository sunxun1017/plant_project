idf_component_register(
    SRCS "main.cpp" "../../05_adapters/espidf/zw_v1/hardware.cpp"
         "../../05_adapters/espidf/zw_v1/audio.cpp"
         "../../08_tests/unit/zw_v1_board_test.cpp"
    INCLUDE_DIRS "../.."
    REQUIRES esp_adc esp_driver_gpio esp_driver_i2c esp_driver_i2s esp_pm esp_timer freertos log esp_codec_dev
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
