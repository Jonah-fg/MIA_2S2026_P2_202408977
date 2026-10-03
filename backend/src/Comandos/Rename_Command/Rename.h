#ifndef RENAME_COMMAND_H
#define RENAME_COMMAND_H
#include <string>
#include <vector>
#include "../CommandResult.h"

namespace Comandos{
    CommandResult Rename_Command(const std::vector<std::string>& tokens);
}
#endif