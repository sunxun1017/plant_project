cmake_minimum_required(VERSION 3.16)

if(ESP_PLATFORM)
    set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/07_products/plant_v1")
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(plant_v1)
else()
    project(plant_project LANGUAGES CXX)

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

add_library(plant_domain STATIC
    03_services/behavior/behavior_policy.cpp
    03_services/behavior/behavior_service.cpp
    03_services/lifecycle/lifecycle_service.cpp
    03_services/ota/ota_service.cpp
    03_services/power/power_service.cpp
    07_products/plant_v1/app/plant_application.cpp
)

target_include_directories(plant_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(plant_domain PRIVATE -Wall -Wextra -Wpedantic -Werror)

include(CTest)

if(BUILD_TESTING)
    add_executable(plant_domain_tests
        08_tests/unit/behavior_service_test.cpp
        08_tests/unit/ota_power_service_test.cpp
        08_tests/integration/plant_application_test.cpp
    )
    target_link_libraries(plant_domain_tests PRIVATE plant_domain)
    target_compile_options(plant_domain_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME plant_domain_tests COMMAND plant_domain_tests)
endif()
endif()
