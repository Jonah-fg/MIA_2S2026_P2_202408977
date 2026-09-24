#include "MountedPartitions.h"
#include <algorithm>

namespace Global {
    std::map<std::string, std::string> MountedPartitions;

    bool GetMountedPartition(const std::string& id,
                             Estructuras::PARTITION& partOut,
                             std::string& diskPathOut,
                             std::string& errMsg) {
        auto it = MountedPartitions.find(id);
        if (it == MountedPartitions.end()) {
            errMsg = "No se encontró la partición montada con id: " + id;
            return false;
        }

        diskPathOut = it->second;
        Estructuras::MBR mbr;
        if (!mbr.DeserializeMBR(diskPathOut, errMsg)) {
            return false;
        }

        const Estructuras::PARTITION* found = nullptr;
        for (int i = 0; i < 4; ++i) {
            std::string existing(mbr.Mbr_partitions[i].Partition_id, sizeof(mbr.Mbr_partitions[i].Partition_id));
            existing.erase(existing.find_last_not_of('\0') + 1);
            if (existing == id) {
                found = &mbr.Mbr_partitions[i];
                break;
            }
        }

        if (!found) {
            errMsg = "La partición montada no existe en el disco: " + id;
            return false;
        }

        partOut = *found;
        errMsg.clear();
        return true;
    }

    bool GetEssentialRep(const std::string& id,
                        Estructuras::MBR& mbr,
                        std::string& diskPathOut,
                        std::string& errMsg) {
        auto it = MountedPartitions.find(id);
        if (it == MountedPartitions.end()) {
            errMsg = "No se encontró la partición montada con id: " + id;
            return false;
        }

        diskPathOut = it->second;
        if (!mbr.DeserializeMBR(diskPathOut, errMsg)) {
            return false;
        }

        errMsg.clear();
        return true;
    }
}
