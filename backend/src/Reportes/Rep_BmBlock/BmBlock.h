#pragma once
#include <string>
#include "../../Estructuras/Str_Superblock/SUPERBLOCK.h"

namespace Reportes {
    bool ReporteBmBlock(const Estructuras::SUPERBLOCK& sb, const std::string& path, const std::string& diskPath, std::string& errMsg);
}