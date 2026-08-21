#pragma once

#include "01_core/domain/command.hpp"

namespace plant::protocol {

struct CommandMessage {
    Command command;
};

}  // namespace plant::protocol
