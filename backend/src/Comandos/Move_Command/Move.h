#ifndef MOVE_COMMAND_H
#define MOVE_COMMAND_H
#include <string>
#include <vector>
#include "../CommandResult.h"

namespace Comandos{
    CommandResult Move_Command(const std::vector<std::string>& tokens);
}
#endif