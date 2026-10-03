#ifndef JOURNALING_COMMAND_H
#define JOURNALING_COMMAND_H

#include <string>
#include <vector>
#include "../CommandResult.h"
namespace Comandos{
    CommandResult Journaling_Command(const std::vector<std::string>& tokens);
}
#endif