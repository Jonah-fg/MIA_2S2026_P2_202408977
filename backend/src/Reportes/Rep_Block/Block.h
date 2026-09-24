#ifndef REP_BLOCK_H
#define REP_BLOCK_H
#include <string>
#include "../../Estructuras/Str_Superblock/SUPERBLOCK.h"

namespace Reportes{
    bool ReporteBLOCK(const Estructuras::SUPERBLOCK& sb,const std::string& path, const std::string& diskPath, std::string& errMsg);
}
#endif