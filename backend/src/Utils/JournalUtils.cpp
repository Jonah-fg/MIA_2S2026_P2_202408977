#include "JournalUtils.h"
#include "../Estructuras/Str_Journal/JOURNAL.h"
#include <fstream>
#include <cstring>
#include <ctime>

using namespace std;
namespace JournalUtils{

    bool EsExt3(const Estructuras::SUPERBLOCK& sb){
        return sb.Sb_filesystem_type== 3;
    }

    bool RegistrarOperacion(const string& diskPath, const Estructuras::SUPERBLOCK& sb, const string& operacion, const string& path, const string& contenido, string& errMsg) {

        //Si no es EXT3, no hay journaling.
        if (!EsExt3(sb)) {
            errMsg.clear();
            return true;
        }

        if (sb.Sb_journal_count <= 0|| sb.Sb_journal_start <= 0) {
            errMsg.clear();
            return true;
        }

        //Abre el disco una sola vez
        fstream archivo(diskPath, ios::binary | ios::in | ios::out);
        if(!archivo.is_open()){
            errMsg = "ERROR: No se pudo abrir el dsco para registrar journal";
            return false;
        }

        //bvusca el primer slot libre (j_count ==0)
        Estructuras::JOURNAL entrada;
        long long offset =sb.Sb_journal_start;

        for (int32_t i = 0; i< sb.Sb_journal_count; ++i) {
            archivo.seekg(offset, ios::beg);
            archivo.read(reinterpret_cast<char*>(&entrada), sizeof(Estructuras::JOURNAL));
            if (!archivo) {
                errMsg="ERROR: No se pudo leer una entrada del journal";
                return false;
            }
            if (entrada.j_count== 0) {
                //slot encontrado
                memset(&entrada, 0, sizeof(entrada));
                entrada.j_count=i +1;  

                strncpy(entrada.j_content.i_operation, operacion.c_str(), 9);
                strncpy(entrada.j_content.i_path, path.c_str(), 31);
                strncpy(entrada.j_content.i_content,contenido.c_str(), 63);

                entrada.j_content.i_date =static_cast<float>(time(nullptr));

                //se escribe la entrada en su slot
                archivo.clear(); 
                archivo.seekp(offset, ios::beg);
                archivo.write(reinterpret_cast<const char*>(&entrada),sizeof(Estructuras::JOURNAL));
                if (!archivo){
                    errMsg ="ERROR: No se pudo escribir en el journal";
                    return false;
                }
                errMsg.clear();
                return true;
            }
            offset += sizeof(Estructuras::JOURNAL);
        }
        //sin slot libre
        errMsg = "Journal llno (máximo " + to_string(sb.Sb_journal_count)+" entradas)";
        return false;
    }
}