#ifndef REP_FILE_H
#define REP_FILE_H
#include <string>
#include "../../Estructuras/Str_Superblock/SUPERBLOCK.h"
namespace Reportes {
    bool ReporteFile(const Estructuras::SUPERBLOCK& sb, const std::string& path, const std::string& diskPath, const std::string& pathFile, std::string& errMsg);
}
#endif