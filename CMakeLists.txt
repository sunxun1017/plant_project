cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(ESP_PLATFORM)
    set(PLANT_PRODUCT "zw_v1" CACHE STRING "Plant firmware product")
    if(PLANT_PRODUCT STREQUAL "zw_v1")
        set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/07_products/plant_zw_v1")
        set(COMPONENTS plant_zw_v1)
        set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/10_config/zw_v1/sdkconfig.defaults")
    elseif(PLANT_PRODUCT STREQUAL "v2")
        set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/07_products/plant_v2")
        message(WARNING "v2 is the legacy hypothetical board, NOT the ZW V1.0 release PCB. Use -DPLANT_PRODUCT=zw_v1 for ZW hardware bring-up.")
    else()
        message(FATAL_ERROR "PLANT_PRODUCT must be v2 or zw_v1")
    endif()
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(plant_${PLANT_PRODUCT})
    if(PLANT_PRODUCT STREQUAL "zw_v1")
        # esp_codec_dev 1.5.3 SPI source uses IDF version macros without
        # including their header. IDF 6.1 no longer supplies it transitively.
        # Keep the registry component immutable; scope compatibility to it.
        idf_component_get_property(zw_codec_lib espressif__esp_codec_dev COMPONENT_LIB)
        target_compile_options(${zw_codec_lib} PRIVATE -include esp_idf_version.h)
    endif()
else()
    project(plant_project LANGUAGES CXX)

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

add_library(plant_domain STATIC
    03_services/behavior/behavior_policy.cpp
    03_services/behavior/behavior_service.cpp
    03_services/communication/communication_service.cpp
    03_services/lifecycle/lifecycle_service.cpp
    03_services/ota/ota_service.cpp
    03_services/power/power_service.cpp
    03_services/growth/growth_service.cpp
    03_services/lighting/light_arbitration_service.cpp
    03_services/sensing/acoustic_service.cpp
    03_services/sensing/battery_service.cpp
    03_services/sensing/climate_service.cpp
    03_services/sensing/illumination_service.cpp
    04_protocol/codec/decoder.cpp
    04_protocol/codec/encoder.cpp
    07_products/plant_v1/app/plant_application.cpp
    07_products/plant_v2/app/plant_v2_application.cpp
)

target_include_directories(plant_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(plant_domain PRIVATE -Wall -Wextra -Wpedantic -Werror)

include(CTest)

if(BUILD_TESTING)
    add_executable(plant_domain_tests
        08_tests/unit/zw_v1_board_test.cpp
        08_tests/unit/behavior_service_test.cpp
        08_tests/unit/ota_power_service_test.cpp
        08_tests/integration/plant_v2_application_test.cpp
        08_tests/unit/protocol_codec_test.cpp
        08_tests/unit/sensing_growth_service_test.cpp
    )
    target_link_libraries(plant_domain_tests PRIVATE plant_domain)
    target_compile_options(plant_domain_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME plant_domain_tests COMMAND plant_domain_tests)
endif()
endif()
