#ifndef REP_COMMAND_H
#define REP_COMMAND_H
#include <string>
#include <vector>
#include "../CommandResult.h"
namespace Comandos{
    struct REP{
        std::string Name;   // mbr, disk, inode, block, bm_inode, bm_block, tree, sb, file, ls
        std::string Path;     
        std::string Id;        
        std::string Path_file; 
    };
    CommandResult Rep_Command(const std::vector<std::string>& tokens);
}
#endif