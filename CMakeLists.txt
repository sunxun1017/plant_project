cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(ESP_PLATFORM)
    set(PLANT_PRODUCT "v1" CACHE STRING "Plant firmware product: v1 or v2")
    set_property(CACHE PLANT_PRODUCT PROPERTY STRINGS v1 v2)
    if(NOT PLANT_PRODUCT STREQUAL "v1" AND NOT PLANT_PRODUCT STREQUAL "v2")
        message(FATAL_ERROR "PLANT_PRODUCT must be v1 or v2")
    endif()
    set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/07_products/plant_${PLANT_PRODUCT}")
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(plant_${PLANT_PRODUCT})
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
        08_tests/unit/behavior_service_test.cpp
        08_tests/unit/ota_power_service_test.cpp
        08_tests/integration/plant_application_test.cpp
        08_tests/integration/plant_v2_application_test.cpp
        08_tests/unit/protocol_codec_test.cpp
        08_tests/unit/sensing_growth_service_test.cpp
    )
    target_link_libraries(plant_domain_tests PRIVATE plant_domain)
    target_compile_options(plant_domain_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME plant_domain_tests COMMAND plant_domain_tests)
endif()
endif()
