#ifndef JOURNAL_H
#define JOURNAL_H
#include <string>
#include <cstdint>
#include "../Str_Information/INFORMATION.h"

using namespace std;
namespace Estructuras {
#pragma pack(push, 1)
    struct JOURNAL{
        int32_t j_count; 
        INFORMATION j_content; //contenido de la acción

        bool Serialize(const string& path, long long offset, string& errMsg);
        bool Deserialize(const string& path, long long offset, string& errMsg);
        void Print() const;
    };
#pragma pack(pop)
}
#endif