#ifndef UNMOUNT_COMMAND_H
#define UNMOUNT_COMMAND_H
#include <string>
#include <vector>
#include "../CommandResult.h"
namespace Comandos {
    CommandResult Unmount_Command(const std::vector<std::string>& tokens);
}
#endif