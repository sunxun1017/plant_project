#include "07_products/plant_v2/composition_root.hpp"

extern "C" void app_main() {
    plant::product::v2::initialize();
    plant::product::v2::run();
}
