#ifndef REMOVE_COMMAND_H
#define REMOVE_COMMAND_H
#include <string>
#include <vector>
#include "../CommandResult.h"

namespace Comandos{
    CommandResult Remove_Command(const std::vector<std::string>& tokens);
}
#endif