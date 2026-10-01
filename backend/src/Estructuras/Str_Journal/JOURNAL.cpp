#include "JOURNAL.h"
#include <fstream>
#include <cstdio>

using namespace std;
namespace Estructuras {

    bool JOURNAL::Serialize(const string& path, long long offset, string& errMsg){
        fstream archivo(path, ios::binary | ios::in | ios::out);
        if (!archivo.is_open()) {
            errMsg ="ERROR: No se pudo abrir el archvo al serializar JOURNAL";
            return false;
        }
        archivo.seekp(offset, ios::beg);
        archivo.write(reinterpret_cast<const char*>(this), sizeof(JOURNAL));
        if (!archivo) {
            errMsg = "ERROR: No se pudo escribir JORNAL";
            return false;
        }
        errMsg.clear();
        return true;
    }

    bool JOURNAL::Deserialize(const string& path, long long offset, string& errMsg) {
        ifstream archivo(path, ios::binary);
        if (!archivo.is_open()){
            errMsg = "ERROR: No se pudo abrir el archivo al deserializar JOURNAL";
            return false;
        }
        archivo.seekg(offset, ios::beg);
        archivo.read(reinterpret_cast<char*>(this), sizeof(JOURNAL));
        if (!archivo){
            errMsg= "ERROR: No se pudo leer JOURNAL";
            return false;
        }
        errMsg.clear();
        return true;
    }
    void JOURNAL::Print() const{
        printf("--- ---------JOURNAL---------\n");
        printf("j_count: %d\n", j_count);
        j_content.Print();
    }
}
