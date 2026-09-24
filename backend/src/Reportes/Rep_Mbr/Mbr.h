#ifndef REP_MBR_H
#define REP_MBR_H
#include <string>
#include "../../Estructuras/Str_Mbr/MBR.h"

namespace Reportes{
    bool ReporteMBR(const Estructuras::MBR& mbr, const std::string& path, const std::string& diskPath, std::string& errMsg);
}
#endif