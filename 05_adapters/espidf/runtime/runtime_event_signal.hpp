#pragma once

namespace plant {

using EspRuntimeEventCallback = void (*)(void* context, bool from_isr) noexcept;

struct EspRuntimeEventSignal final {
    EspRuntimeEventCallback callback{nullptr};
    void* context{nullptr};

    void notify(bool from_isr) const noexcept {
        if (callback != nullptr) {
            callback(context, from_isr);
        }
    }

    [[nodiscard]] bool configured() const noexcept { return callback != nullptr; }
};

}  // namespace plant
