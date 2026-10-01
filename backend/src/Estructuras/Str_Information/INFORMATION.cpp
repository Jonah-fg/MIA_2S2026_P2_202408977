#include "INFORMATION.h"
#include <fstream>
#include <cstdio>

using namespace std;
namespace Estructuras{

    bool INFORMATION::Serialize(const string& path, long long offset, string& errMsg) {
        fstream archivo(path, ios::binary | ios::in | ios::out);
        if (!archivo.is_open()) {
            errMsg ="ERROR: No se pudo abrir el archivo al serializar INFORMATION";
            return false;
        }
        archivo.seekp(offset, ios::beg);
        archivo.write(reinterpret_cast<const char*>(this), sizeof(INFORMATION));
        if (!archivo) {
            errMsg = "ERROR: No se pudo escribir INFORMATION";
            return false;
        }
        errMsg.clear();
        return true;
    }

    bool INFORMATION::Deserialize(const string& path, long long offset, string& errMsg) {
        ifstream archivo(path, ios::binary);
        if (!archivo.is_open()) {
            errMsg ="ERROR: No se pudo abrir el archivo al deserializar INFORMATION";
            return false;
        }
        archivo.seekg(offset, ios::beg);
        archivo.read(reinterpret_cast<char*>(this), sizeof(INFORMATION));
        if (!archivo) {
            errMsg = "ERROR: No se pudo leer INFORMATION";
            return false;
        }
        errMsg.clear();
        return true;
    }

    void INFORMATION::Print() const{
        printf("i_operation: %.10s\n", i_operation);
        printf("i_path: %.32s\n", i_path);
        printf("i_content: %.64s\n", i_content);
        printf("i_date: %.0f\n", i_date);
    }
}