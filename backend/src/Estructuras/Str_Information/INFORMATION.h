#ifndef INFORMATION_H
#define INFORMATION_H
#include <string>
#include <cstdint>

using namespace std;
namespace Estructuras{

#pragma pack(push, 1)
    struct INFORMATION {
        char  i_operation[10];  //operación realizada 
        char  i_path[32];       
        char  i_content[64]; //contenido del archivo (
        float i_date;    
        
        void Print() const;
        bool Serialize(const string& path, long long offset, string& errMsg);
        bool Deserialize(const string& path, long long offset, string& errMsg);
    };
#pragma pack(pop)

}

#endif