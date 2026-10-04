#include "Find.h"
#include "../../Global/Sesion.h"
#include "../../Global/MountedPartitions.h"
#include <regex>
#include <sstream>
#include "../../Utils/Ext2Utils.h"
#include "../../Utils/JournalUtils.h"
#include "../../Utils/PermisosUtils.h"
#include "../../Estructuras/Str_Folderblock/FOLDERBLOCK.h"
#include <cstring>

using namespace std;
namespace Comandos {

    static string toLowerStr(string s) {
        transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return tolower(c); });
        return s;
    }

    static string joinTokens(const vector<string>& tokens) {
        string result;
        for (size_t i=0; i <tokens.size(); ++i) {
            if (i >0)
                result +=" ";
            result+= tokens[i];
        }
        return result;
    }

    static vector<string> splitPath(const string& path) {
        vector<string> partes;
        stringstream ss(path);
        string item;
        while (getline(ss, item, '/')){
            if (!item.empty()) partes.push_back(item);
        }
        return partes;
    }


  //Compara un nombre contra un patrón con comodines
    static bool coincidePatron(const string& patron, const string& nombre) {
        if (patron.empty()){
            return nombre.empty();
        }
        char pc= patron[0];

        if (pc== '?') {
            if (nombre.empty()) 
                return false;

            return coincidePatron(patron.substr(1), nombre.substr(1));
        }

        if (pc == '*'){
            // '*' se consume creo 1 carácter
            for (size_t i=1; i <= nombre.size(); ++i) {
                if (coincidePatron(patron.substr(1), nombre.substr(i)))
                    return true;
            }
            return false;
        }

        if(nombre.empty() || nombre[0] != pc){
            return false;
        }
        return coincidePatron(patron.substr(1), nombre.substr(1));
    }

    // Recorrido recursivo. Acumula las coincidencias con su ruta completa.
    static void buscarRecursivo(const string& diskPath, const Estructuras::SUPERBLOCK& sb, int inodoNum, const string& rutaActual, const string& patron, bool esRoot, int uidActual, vector<string>& resultados) {
        Estructuras::INODE inodo;
        string errMsg;
        if(!Ext2Utils::LeerInodo(diskPath, sb, inodoNum, inodo, errMsg)) {
            return;
        }

        //para recorrer
        if (inodo.I_type[0] !='0'){
            return;
        }


        for (int b =0; b< 12;++b) {
            if (inodo.I_block[b] == -1){
                continue;
            }

            Estructuras::FOLDERBLOCK fb;
            long long off = sb.Sb_block_start+(inodo.I_block[b] * sb.Sb_block_size);
            if (!fb.Deserialize(diskPath, off, errMsg))
                continue;

            for (int i = 0; i<4; ++i) {
                string nombre(fb.B_content[i].B_name);
                nombre= nombre.c_str();
                if (nombre.empty() || nombre =="." || nombre == ".." ||
                    fb.B_content[i].B_inodo== -1) {
                    continue;
                }

                int inodoHijo = fb.B_content[i].B_inodo;
                Estructuras::INODE hijo;
                if (!Ext2Utils::LeerInodo(diskPath, sb, inodoHijo, hijo, errMsg)) {
                    continue;
                }

                //verificacion permiso de lectura
                if (!PermisosUtils::PuedeLeer(hijo, esRoot, uidActual)) 
                    continue;

                //construccion ruta completa del hijo
                string rutaHijo =rutaActual;
                if (rutaHijo.back() != '/') rutaHijo += "/";
                rutaHijo +=nombre;

                if (coincidePatron(patron, nombre)){
                    char tipo = (hijo.I_type[0] =='0') ? 'C' : 'A';
                    string etiqueta = (tipo == 'C') ? "[Carpeta]" : "[Archivo]";
                    resultados.push_back(etiqueta + " "+ rutaHijo);
                }
                //mantiene recursividad si es carpeta
                if (hijo.I_type[0] == '0') {
                    buscarRecursivo(diskPath, sb, inodoHijo, rutaHijo, patron, esRoot, uidActual, resultados);
                }
            }
        }
    }

    //Navega hasta el inodo de la carpeta inicial
    static int obtenerInodoInicio(const string& diskPath, const Estructuras::SUPERBLOCK& sb, const string& ruta, string& errMsg) {
        if (ruta == "/" || ruta.empty()) {
            return 0;
        }

        vector<string> partes=splitPath(ruta);
        int inodoActual =0;
        Estructuras::INODE inode;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)){
            return -1;
        }

        for (const string& comp : partes){
            int hijo= Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, comp, errMsg);
            if (hijo== -1) {
                errMsg = "Ruta inicial no existe: '" +comp + "' no encontrado";
                return -1;
            }
            inodoActual=hijo;
            if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) 
                return -1;
        }
        if (inode.I_type[0]!= '0') {
            errMsg = "La ruta inicial no es una carpeta";
            return -1;
        }
        return inodoActual;
    }

    //Comando principal
    CommandResult Find_Command(const vector<string>& tokens) {
        if (!Global::sesionActual.activa) {
            return {false, "ERROR: No hay seión activa"};
        }

        //Parseo -path y -name 
        string path, patron;
        string atributos= joinTokens(tokens);
        static const regex lexic(R"(-path="[^"]+"|-path=[^\s]+|-name="[^"]+"|-name=[^\s]+)",regex::icase);

        auto begin= sregex_iterator(atributos.begin(), atributos.end(), lexic);
        auto end = sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            string fun=it->str();
            size_t eq=fun.find('=');
            if (eq == string::npos)
                continue;

            string key =toLowerStr(fun.substr(0, eq));
            string value = fun.substr(eq + 1);
            if (value.size() >= 2&& value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size()- 2);
            }
            if (key =="-path") 
                path = value;

            else if(key == "-name") {
                patron = value;
            }
        }

        if (path.empty()){
            return {false, "ERROR: falta -path"};

        }

        if (patron.empty()){
            return {false, "ERROR: falta -name"};
        }

        //partición y sb 
        string diskPath = Global::sesionActual.diskPath;
        string id = Global::sesionActual.idParticion;
        Estructuras::PARTITION mountedPart;
        string errMsg;
        if (!Global::GetMountedPartition(id, mountedPart, diskPath, errMsg)) {
            return {false, "ERROR: "+errMsg};
        }

        Estructuras::SUPERBLOCK sb;
        if (!Ext2Utils::LeerSuperbloque(diskPath, mountedPart.Partition_start, sb, errMsg)) {
            return {false, "ERROR: No se pudo leer superbloque: "+ errMsg};
        }

        //inodo de inicio ----
        int inodoInicio = obtenerInodoInicio(diskPath, sb, path, errMsg);
        if (inodoInicio== -1) {
            return {false, "ERROR: " + errMsg};
        }

        //Buscando recurs
        vector<string>resultados;
        buscarRecursivo(diskPath, sb, inodoInicio, path, patron, Global::sesionActual.esRoot, Global::sesionActual.uid, resultados);

        ostringstream salida;
        salida <<"\n============= FIND ===============================\n";
        salida << "Patron: " <<patron << "  |  Desde: "<< path << "\n\n";

        if (resultados.empty()) {
            salida << "(sin coincidencias)\n";
        } 
        else{
            for(const string& r : resultados){
                salida <<"  "<< r << "\n";
            }
            salida <<"\nTotal: " << resultados.size() <<" coincidencia(s).\n";
        }

        {
            string journalErr;
            JournalUtils::RegistrarOperacion(diskPath, sb, "find", path, patron, journalErr);
        }
        return {true, salida.str()};
    }

}