#pragma once

#include "01_core/common/error.hpp"

namespace plant {

class Status {
public:
    constexpr Status() noexcept = default;
    constexpr explicit Status(ErrorCode code) noexcept : code_(code) {}

    [[nodiscard]] static constexpr Status success() noexcept { return Status{}; }
    [[nodiscard]] static constexpr Status failure(ErrorCode code) noexcept {
        return Status{code};
    }

    [[nodiscard]] constexpr bool ok() const noexcept { return code_ == ErrorCode::None; }
    [[nodiscard]] constexpr ErrorCode code() const noexcept { return code_; }
    constexpr explicit operator bool() const noexcept { return ok(); }

private:
    ErrorCode code_{ErrorCode::None};
};

}  // namespace plant
