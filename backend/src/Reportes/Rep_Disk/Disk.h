#ifndef REP_DISK_H
#define REP_DISK_H
#include <string>
#include "../../Estructuras/Str_Mbr/MBR.h"
namespace Reportes {
    bool ReporteDISK(const Estructuras::MBR& mbr, const std::string& path, const std::string& diskPath, std::string& errMsg);
}
#endif