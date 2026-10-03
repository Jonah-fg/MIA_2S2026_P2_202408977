#include "Journaling.h"
#include "../../Global/MountedPartitions.h"
#include "../../Estructuras/Str_Superblock/SUPERBLOCK.h"
#include "../../Estructuras/Str_Journal/JOURNAL.h"
#include "../../Utils/JournalUtils.h"
#include "../../Utils/Ext2Utils.h"
#include <regex>
#include <sstream>
#include <fstream>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

using namespace std;
namespace Comandos {
    static string toLowerStr(string s){
        transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return tolower(c); });
        return s;
    }

    static string joinTokens(const vector<string>& tokens) {
        string result;
        for (size_t i = 0; i<tokens.size(); ++i){
            if (i >0) {
                result+= " ";
            }
            result+= tokens[i];
        }
        return result;
    }

    static string formatearFecha(float unixTime){
        time_t t =static_cast<time_t>(unixTime);
        struct tm tmRes;
        localtime_r(&t, &tmRes);
        char buf[32];
        strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", &tmRes);
        return string(buf);
    }

    //extraccion de solo los caracteres útiles de un arreglo char[]
    static string limpiarCampo(const char* data, size_t maxLen){
        string s(data, maxLen);
        size_t nul =s.find('\0');
        if(nul != string::npos)
            s =s.substr(0, nul);

        while(!s.empty() && (s.back()==' ' || s.back() == '\r' || s.back()== '\n')) {
            s.pop_back();
        }
        return s;
    }

    CommandResult Journaling_Command(const vector<string>& tokens) {
        //Parse -id 
        string id;
        string atributos= joinTokens(tokens);
        static const regex lexic(R"(-id=[^\s]+)", regex::icase);

        auto begin= sregex_iterator(atributos.begin(), atributos.end(), lexic);
        auto end =sregex_iterator();
        for (auto it =begin; it != end; ++it) {
            string fun = it->str();
            size_t eq= fun.find('=');
            string key= toLowerStr(fun.substr(0, eq));
            string value=fun.substr(eq + 1);
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            if (key =="-id"){
                id = value;
            }
        }

        if (id.empty()){
            return {false, "ERROR: falta -id"};
        }

        //Obtencion partición y superbloqu
        Estructuras::PARTITION mountedPart;
        string diskPath;
        string errMsg;
        if (!Global::GetMountedPartition(id, mountedPart, diskPath, errMsg)){
            return {false, "ERROR: " + errMsg};
        }

        Estructuras::SUPERBLOCK sb;
        if (!Ext2Utils::LeerSuperbloque(diskPath, mountedPart.Partition_start, sb, errMsg)) {
            return {false, "ERROR: No se pudo leer superbloque: "+errMsg};
        }

        //verificacion que sea EXT3 
        if (!JournalUtils::EsExt3(sb)) {
            return {false, "ERROR: La partición " +id + " no es EXT3 (no tiene journaing)"};
        }

        if (sb.Sb_journal_count <= 0 || sb.Sb_journal_start <= 0) {
            return {false, "ERROR: La partición no tiene journaling inicializado"};
        }

        //Lectura entradas del journal
        ifstream archivo(diskPath, ios::binary);
        if (!archivo.is_open()) {
            return {false, "ERROR: No se pudo abrir el disco para leer journal"};
        }

        ostringstream salida;
        salida << "\n==========JOURNALING====================\n";
        salida << "Partición: "<< id << "  |  Entradas: "<<sb.Sb_journal_count<< "\n\n";
        salida << "No.     Operacion      Path                               Contenido                           Fecha\n";
        salida << "---     ---------      ------------------------------     -------------------------------  -   -----------------\n";

        long long offset= sb.Sb_journal_start;
        int mostradas=0;

        for (int32_t i= 0; i< sb.Sb_journal_count; ++i) {
            Estructuras::JOURNAL entrada;
            archivo.seekg(offset, ios::beg);
            archivo.read(reinterpret_cast<char*>(&entrada), sizeof(Estructuras::JOURNAL));
            if (!archivo) 
                break;

            if (entrada.j_count != 0){
                string oper =limpiarCampo(entrada.j_content.i_operation, 10);
                string path= limpiarCampo(entrada.j_content.i_path, 32);
                string cont = limpiarCampo(entrada.j_content.i_content, 64);
                string fecha=formatearFecha(entrada.j_content.i_date);

                //Truncar para que quepa en columnas
                if (path.size() > 30) {
                    path = path.substr(0, 27)+ "...";
                }

                if (cont.size()> 31){
                    cont= cont.substr(0, 28)+ "...";
                }

                char linea[256];
                snprintf(linea, sizeof(linea), "%-3d  %-9s   %-30s  %-31s  %s\n", entrada.j_count, oper.c_str(), path.c_str(),  cont.c_str(), fecha.c_str());
                salida << linea;
                mostradas++;
            }
            offset += sizeof(Estructuras::JOURNAL);
        }

        if(mostradas== 0){
            salida << "(no hay operaiones registradas aún)\n";
        } 
        else {
            salida<< "\nTotal de operaciones registradas: " << mostradas << "\n";
        }
        archivo.close();
        return {true, salida.str()};
    }
}