#ifndef COPY_COMMAND_H
#define COPY_COMMAND_H
#include <string>
#include <vector>
#include "../CommandResult.h"
namespace Comandos {
    CommandResult Copy_Command(const std::vector<std::string>& tokens);
}
#endif