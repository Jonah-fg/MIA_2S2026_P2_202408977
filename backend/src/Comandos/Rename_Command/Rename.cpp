#include "Rename.h"
#include "../../Global/Sesion.h"
#include "../../Global/MountedPartitions.h"
#include "../../Utils/Ext2Utils.h"
#include "../../Utils/JournalUtils.h"
#include "../../Utils/PermisosUtils.h"
#include "../../Estructuras/Str_Folderblock/FOLDERBLOCK.h"
#include <regex>
#include <sstream>
#include <cstring>
#include <ctime>
using namespace std;

namespace Comandos{
    static string toLowerStr(string s) {
        transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return tolower(c); });
        return s;
    }

    static string joinTokens(const vector<string>& tokens){
        string result;
        for (size_t i =0; i< tokens.size(); ++i) {
            if (i>0) 
                result+= " ";
            result+= tokens[i];
        }
        return result;
    }

    static vector<string> splitPath(const string& path) {
        vector<string> partes;
        stringstream ss(path);
        string item;
        while (getline(ss, item, '/')) {
            if (!item.empty()) partes.push_back(item);
        }
        return partes;
    }

    static int buscarInodoHijo(const string& diskPath, const Estructuras::SUPERBLOCK& sb, const vector<string>& partes, int& inodoPadreOut, string& errMsg) {
        if (partes.empty()) {
            errMsg="Ruta vacía";
            return -1;
        }

        int inodoActual=0;
        Estructuras::INODE inode;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)){
            return -1;
        }

        //navegacion hasta el padre (todas las partes menos la última)
        for (size_t i= 0; i+1< partes.size(); ++i) {
            int hijo=Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, partes[i], errMsg);
            if (hijo== -1) {
                errMsg = "Carpeta '" +partes[i] + "' no encontrada";
                return -1;
            }
            inodoActual = hijo;
            if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)){
                return -1;
            } 
        }

        inodoPadreOut = inodoActual;

        int hijo =Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, partes.back(), errMsg);
        if (hijo ==-1) {
            errMsg = "Archivo o carpeta '" + partes.back() + "'no encontrado";
            return -1;
        }
        return hijo;
    }

    CommandResult Rename_Command(const vector<string>& tokens) {
        //Verificacion sesión 
        if (!Global::sesionActual.activa) {
            return {false, "ERROR: No hay seión aciva"};
        }

        //Parseo -path y -name
        string path;
        string nuevoNombre;
        string atributos =joinTokens(tokens);
        static const regex lexic(R"(-path="[^"]+"|-path=[^\s]+|-name="[^"]+"|-name=[^\s]+)", regex::icase);

        auto begin =sregex_iterator(atributos.begin(), atributos.end(), lexic);
        auto end = sregex_iterator();
        for (auto it=begin; it != end; ++it) {
            string fun = it->str();
            size_t eq=fun.find('=');
            if (eq == string::npos) 
                continue;

            string key =toLowerStr(fun.substr(0, eq));
            string value= fun.substr(eq+1);
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size()- 2);
            }
            if(key=="-path")
                path = value;

            else if(key == "-name"){
                nuevoNombre = value;
            }
        }

        if (path.empty()) {
            return {false, "ERROR: falta -path"};
        }
        if (nuevoNombre.empty())
            return {false, "ERROR: falta -name"};

        // Obtencion partición y superbloque
        string diskPath =Global::sesionActual.diskPath;
        string id =Global::sesionActual.idParticion;
        Estructuras::PARTITION mountedPart;
        string errMsg;
        if (!Global::GetMountedPartition(id, mountedPart, diskPath, errMsg)) {
            return {false, "ERROR: "+errMsg};
        }

        Estructuras::SUPERBLOCK sb;
        if (!Ext2Utils::LeerSuperbloque(diskPath, mountedPart.Partition_start, sb, errMsg)) {
            return {false, "ERROR: No se pudo leer superloque: " + errMsg};
        }

        vector<string> partes = splitPath(path);
        if (partes.empty()) {
           return {false, "ERROR: path inválido"}; 
        }

        int inodoPadre=-1;
        int inodoHijo =buscarInodoHijo(diskPath, sb, partes, inodoPadre, errMsg);
        if (inodoHijo== -1){
            return {false, "ERROR: "+ errMsg};
        }

        //Verificar permisos de escritura sobre el hijo
        Estructuras::INODE inodoHijoObj;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoHijo, inodoHijoObj, errMsg)) {
            return {false, "ERROR: No se pudo leer inodo hijo: " +errMsg};
        }

        if (!PermisosUtils::PuedeEscribir(inodoHijoObj, Global::sesionActual.esRoot, Global::sesionActual.uid)) {
            return {false, "ERROR: No tiene permisos de escriura sobre '" + partes.back()+ "'"};
        }

        //verificacopnm que no exista otro con el nuevo nombre 
        Estructuras::INODE inodoPadreObj;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoPadre, inodoPadreObj, errMsg)) {
            return {false, "ERROR: No se pudo leer inodo padre: "+ errMsg};
        }

        int existente=Ext2Utils::BuscarHijoEnCarpeta(
            diskPath, sb, inodoPadreObj, nuevoNombre, errMsg);
        if (existente != -1){
            return {false, "ERROR: Ya existe un arcivo o carpeta llamado '" + nuevoNombre + "'"};
        }

        // la Modificacion de la entrada del folderblock
        bool modificado= false;
        for (int b =0; b < 12; ++b){
            if (inodoPadreObj.I_block[b]==-1)
                continue;

            Estructuras::FOLDERBLOCK fb;
            long long offset= sb.Sb_block_start + (inodoPadreObj.I_block[b] * sb.Sb_block_size);
            if (!fb.Deserialize(diskPath, offset, errMsg)){
                continue;
            }
  
            for (int i = 0; i<4; ++i) {
                string nombre(fb.B_content[i].B_name);
                nombre =nombre.c_str();
                if(nombre == partes.back() && fb.B_content[i].B_inodo== inodoHijo) {
                    //cambio de nombre si lo encuentraaaa
                    memset(fb.B_content[i].B_name, 0, sizeof(fb.B_content[i].B_name));
                    strncpy(fb.B_content[i].B_name, nuevoNombre.c_str(), sizeof(fb.B_content[i].B_name) - 1);

                    if (!fb.Serialize(diskPath, offset, errMsg)){
                        return {false, "ERROR: No se pudo escribir folderblock: " + errMsg};
                    }
                    modificado=true;
                    break;
                }
            }
            if (modificado) 
                break;
        }

        if (!modificado){
            return {false, "ERROR: No se pudo modificar el nombre (entrada no encotrada)"};
        }

        //actualizacion
        inodoHijoObj.I_mtime = static_cast<float>(time(nullptr));
        if (!Ext2Utils::EscribirInodo(diskPath, sb, inodoHijo, inodoHijoObj, errMsg)) {
            return {false, "ERROR: No se pudo actualizar inodo: " +errMsg};
        }

        { //Journal
            string journalErr;
            JournalUtils::RegistrarOperacion(diskPath, sb, "rename", path, nuevoNombre, journalErr);
        }
        return {true, "RENAME: '" + partes.back()+ "' renombrado a '" + nuevoNombre+ "'"};
    }

}