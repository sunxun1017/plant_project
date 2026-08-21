idf_component_register(
    SRCS
        "main.cpp"
        "composition_root.cpp"
        "app/plant_application.cpp"
        "../../03_services/behavior/behavior_policy.cpp"
        "../../03_services/behavior/behavior_service.cpp"
        "../../03_services/communication/communication_service.cpp"
        "../../03_services/lifecycle/lifecycle_service.cpp"
        "../../03_services/ota/ota_service.cpp"
        "../../03_services/power/power_service.cpp"
        "../../04_protocol/codec/decoder.cpp"
        "../../04_protocol/codec/encoder.cpp"
        "../../05_adapters/espidf/ble/esp_ble_adapter.cpp"
        "../../05_adapters/espidf/motion/servo_adapter.cpp"
        "../../05_adapters/espidf/light/led_adapter.cpp"
        "../../05_adapters/espidf/haptic/vibration_adapter.cpp"
        "../../05_adapters/espidf/touch/touch_adapter.cpp"
        "../../05_adapters/espidf/power/esp_power_adapter.cpp"
        "../../05_adapters/espidf/ota/esp_ota_adapter.cpp"
    INCLUDE_DIRS
        "../.."
    REQUIRES
        app_update
        bt
        esp_driver_gpio
        esp_driver_ledc
        esp_hw_support
        esp_pm
        esp_timer
        freertos
        log
        nvs_flash
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
