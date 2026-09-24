#ifndef REP_INODE_H
#define REP_INODE_H
#include <string>
#include "../../Estructuras/Str_Superblock/SUPERBLOCK.h"

namespace Reportes {
    bool ReporteINODE(const Estructuras::SUPERBLOCK& sb,  const std::string& path, const std::string& diskPath, std::string& errMsg);
}
#endif