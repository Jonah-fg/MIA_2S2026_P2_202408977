
#ifndef CHOWN_COMMAND_H
#define CHOWN_COMMAND_H
#include <string>
#include <vector>
#include "../CommandResult.h"

namespace Comandos{
    CommandResult Chown_Command(const std::vector<std::string>& tokens);
}
#endif