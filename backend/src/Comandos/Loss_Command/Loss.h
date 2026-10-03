#ifndef LOSS_COMMAND_H
#define LOSS_COMMAND_H
#include <vector>
#include <string>
#include "../CommandResult.h"

namespace Comandos{
    CommandResult Loss_Command(const std::vector<std::string>& tokens);
}
#endif