#include "Unmount.h"
#include "../../Global/MountedPartitions.h"
#include "../../Global/Sesion.h"
#include "../../Estructuras/Str_Mbr/MBR.h"
#include <regex>
#include <algorithm>
#include <cctype>
#include <cstring>

using namespace std;

namespace Comandos {

    static string toLowerStr(string s) {
        transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return tolower(c); });
        return s;
    }

    static string joinTokens(const vector<string>& tokens) {
        string result;
        for (size_t i = 0; i< tokens.size(); ++i) {
            if (i > 0)
                 result+=" ";
            result +=tokens[i];
        }
        return result;
    }

    CommandResult Unmount_Command(const vector<string>& tokens) {
        //Parseo -id
        string id;
        string atributos =joinTokens(tokens);
        static const regex lexic(R"(-id=[^\s]+)", regex::icase);

        auto begin=sregex_iterator(atributos.begin(), atributos.end(), lexic);
        auto end = sregex_iterator();
        for (auto it= begin; it != end; ++it) {
            string fun = it->str();
            size_t eq = fun.find('=');
            if(eq == string::npos) 
                continue;
                
            string key= toLowerStr(fun.substr(0, eq));
            string value=fun.substr(eq +1);
            if (value.size() >= 2 && value.front() =='"' && value.back() == '"') {
                value = value.substr(1, value.size()-2);
            }
            if (key == "-id") {
                id =value;
            }
        }
        if (id.empty()) {
            return {false, "ERROR: falta -id"};
        }

        //Verificacion dw que el ID esté montado
        auto it= Global::MountedPartitions.find(id);
        if (it == Global::MountedPartitions.end()){
            return {false, "ERROR: La partción con ID '" + id + "' no está montada"};
        }
        string diskPath = it->second;

        //Lectura del MBR y desmarcar la partición 
        Estructuras::MBR mbr;
        string errMsg;
        if (!mbr.DeserializeMBR(diskPath, errMsg)){
            return {false, "ERROR: No se pdo leer MBR: " + errMsg};
        }

        bool encontrada= false;
        for (int i = 0; i<4; ++i) {
            Estructuras::PARTITION& p= mbr.Mbr_partitions[i];

            string pid(p.Partition_id, sizeof(p.Partition_id));
            size_t nul =pid.find('\0');
            if (nul!= string::npos) {
                pid = pid.substr(0, nul);
            }

            if (pid== id){
                //reseteo del estado de la partición
                p.Partition_status[0]='0';
                p.Partition_number =0;
                memset(p.Partition_id, 0, sizeof(p.Partition_id));
                encontrada =true;
                break;
            }
        }

        if (!encontrada){
            return {false,"ERROR: La particin con ID '" + id + "' no existe en el MBR"};
        }

        //Guardar MBR
        if (!mbr.SerializeMBR(diskPath, errMsg)) {
            return {false, "ERROR: No se pudo escribir MBR: "+ errMsg};
        }

        if (Global::sesionActual.activa && Global::sesionActual.idParticion== id) {
            Global::CerrarSesion();
        }
        Global::MountedPartitions.erase(it);
        return {true, "UNMOUNT: Partición '" + id+ "' desmontada correctamente"};
    }

}