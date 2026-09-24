#ifndef REP_LS_H
#define REP_LS_H

#include <string>
#include "../../Estructuras/Str_Superblock/SUPERBLOCK.h"

namespace Reportes{
    bool ReporteLS(const Estructuras::SUPERBLOCK& sb, const std::string& path,const std::string& diskPath, const std::string& pathFile, std::string& errMsg);
}
#endif