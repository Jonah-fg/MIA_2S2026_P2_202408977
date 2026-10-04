#ifndef FIND_COMMAND_H
#define FIND_COMMAND_H
#include <string>
#include <vector>
#include "../CommandResult.h"
namespace Comandos {
    CommandResult Find_Command(const std::vector<std::string>& tokens);
}

#endif