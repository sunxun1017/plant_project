#include "07_products/plant_v1/composition_root.hpp"

extern "C" void app_main() {
    plant::product::v1::initialize();
    plant::product::v1::run();
}
