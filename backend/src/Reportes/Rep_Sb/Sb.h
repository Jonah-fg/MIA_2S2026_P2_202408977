#pragma once
#include <string>
#include "../../Estructuras/Str_Superblock/SUPERBLOCK.h"

namespace Reportes {
    bool ReporteSB(const Estructuras::SUPERBLOCK& sb, const std::string& path, std::string& errMsg);
}