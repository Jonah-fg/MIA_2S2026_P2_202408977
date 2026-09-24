#ifndef REP_TREE_H
#define REP_TREE_H
#include "../../Estructuras/Str_Superblock/SUPERBLOCK.h"
#include <string>

namespace Reportes {
    bool ReporteTREE(const Estructuras::SUPERBLOCK& sb, const std::string& path, const std::string& diskPath, std::string& errMsg);
}
#endif