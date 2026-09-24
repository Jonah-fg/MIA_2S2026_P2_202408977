#pragma once
#include <map>
#include <string>
#include "../Estructuras/Str_Mbr/MBR.h"
#include "../Estructuras/Str_Partition/PARTITION.h"

namespace Global {
    extern std::map<std::string, std::string> MountedPartitions;

    bool GetMountedPartition(const std::string& id,
                             Estructuras::PARTITION& partOut,
                             std::string& diskPathOut,
                             std::string& errMsg);

    bool GetEssentialRep(const std::string& id,
                        Estructuras::MBR& mbr,
                        std::string& diskPathOut,
                        std::string& errMsg);
}
