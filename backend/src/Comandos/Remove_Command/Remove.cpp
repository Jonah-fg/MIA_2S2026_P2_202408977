#include "Remove.h"
#include "../../Global/Sesion.h"
#include "../../Global/MountedPartitions.h"
#include "../../Utils/Ext2Utils.h"
#include "../../Utils/JournalUtils.h"
#include <regex>
#include "../../Utils/PermisosUtils.h"
#include "../../Estructuras/Str_Folderblock/FOLDERBLOCK.h"
#include <sstream>
#include <cstring>

using namespace std;
namespace Comandos{

    static string toLowerStr(string s){
        transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return tolower(c); });
        return s;
    }

    static string joinTokens(const vector<string>& tokens) {
        string result;
        for (size_t i = 0; i<tokens.size(); ++i) {
            if(i >0) 
                result += " ";

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

    //verificacion recursiva que todo se puede eliminarr
    static bool verificarPuedeEliminar(const string& diskPath, const Estructuras::SUPERBLOCK& sb, int inodoNum,  bool esRoot, int uidActual, string& errMsg) {
        Estructuras::INODE inodo;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoNum, inodo, errMsg)){
            return false;
        }

        if (!PermisosUtils::PuedeEscribir(inodo, esRoot, uidActual)) {
            errMsg= "Sin permiso de escritura";
            return false;
        }

        if (inodo.I_type[0] == '1')
            return true;

        // Es carpeta: verifica cada hijo
        for (int b =0; b< 12; ++b) {
            if (inodo.I_block[b]== -1){
                continue;
            }

            Estructuras::FOLDERBLOCK fb;
            long long off =sb.Sb_block_start + (inodo.I_block[b] * sb.Sb_block_size);
            if (!fb.Deserialize(diskPath, off, errMsg)) {
                return false;
            }

            for (int i =0; i<4; ++i) {
                string nombre(fb.B_content[i].B_name);
                nombre= nombre.c_str();
                if (nombre.empty() || nombre=="." || nombre == ".." ||
                    fb.B_content[i].B_inodo == -1) {
                    continue;
                }
                if (!verificarPuedeEliminar(diskPath, sb, fb.B_content[i].B_inodo, esRoot, uidActual, errMsg)) {
                    errMsg = "Sin permiso sobre '" +nombre + "': "+errMsg;
                    return false;
                }
            }
        }
        return true;
    }


    //eliminacion recursivamente (ya validado)
    static bool eliminarRecursivo(const string& diskPath, Estructuras::SUPERBLOCK& sb, int inodoNum, string& errMsg) {
        Estructuras::INODE inodo;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoNum, inodo, errMsg)) 
        {
            return false;
        }

        //Si es carpeta, se elimna los hijos primero creo
        if (inodo.I_type[0] == '0'){
            for (int b =0; b < 12; ++b){
                if (inodo.I_block[b]==-1) {
                    continue;
                }

                Estructuras::FOLDERBLOCK fb;
                long long off=sb.Sb_block_start+ (inodo.I_block[b] * sb.Sb_block_size);
                if (!fb.Deserialize(diskPath, off, errMsg)){
                    return false;
                }

                for (int i=0; i < 4; ++i) {
                    string nombre(fb.B_content[i].B_name);
                    nombre = nombre.c_str();
                    if (nombre.empty() || nombre =="." || nombre == ".." ||
                        fb.B_content[i].B_inodo ==-1) {
                        continue;
                    }
                    if(!eliminarRecursivo(diskPath, sb, fb.B_content[i].B_inodo, errMsg)){
                        return false;
                    }
                }
            }
        }

        //Liberacion de todos los bloques del inodo 
        for (int b = 0; b < 12; ++b){
            if (inodo.I_block[b]>0) {
                string e;
                Ext2Utils::LiberarBloque(diskPath, sb, inodo.I_block[b], e);
                inodo.I_block[b] =-1;
            }
        }

        if(!Ext2Utils::LiberarInodo(diskPath, sb, inodoNum, errMsg)){
            return false;
        }
        return true;
    }

    static bool quitarEntradaPadre(const string& diskPath, const Estructuras::SUPERBLOCK& sb, Estructuras::INODE& inodoPadre, int inodoHijo, const string& nombreHijo, string& errMsg) {
        for(int b= 0; b <12; ++b){
            if (inodoPadre.I_block[b]== -1)
                 continue;

            Estructuras::FOLDERBLOCK fb;
            long long off = sb.Sb_block_start+ (inodoPadre.I_block[b] * sb.Sb_block_size);
            if (!fb.Deserialize(diskPath, off, errMsg)){
                continue;
            }

            for (int i =0; i < 4; ++i) {
                string nombre(fb.B_content[i].B_name);
                nombre=nombre.c_str();
                if (nombre ==nombreHijo && fb.B_content[i].B_inodo == inodoHijo){
                    //Vaciar la entrada
                    memset(fb.B_content[i].B_name, 0, sizeof(fb.B_content[i].B_name));
                    fb.B_content[i].B_name[0] ='-';
                    fb.B_content[i].B_inodo= -1;
                    return fb.Serialize(diskPath, off, errMsg);
                }
            }
        }
        errMsg = "No se encotró la entrada en el padre";
        return false;
    }


    // Buscar inodo del padre y del hijo
    static int buscarInodoHijo(const string& diskPath, const Estructuras::SUPERBLOCK& sb, const vector<string>& partes, int& inodoPadreOut, string& errMsg) {
        if (partes.empty()) { 
            errMsg= "Ruta vacía"; 
            return -1; 
        }

        int inodoActual= 0;
        Estructuras::INODE inode;
        if(!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) 
        {
            return -1;
        }

        for (size_t i=0; i +1 < partes.size(); ++i) {
            int hijo = Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, partes[i], errMsg);
            if (hijo ==-1){
                errMsg ="Carpeta '"+ partes[i] +"' no encontrada";
                return -1;
            }
            inodoActual =hijo;
            if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)){
                return -1;
            }
        }
        inodoPadreOut= inodoActual;

        int hijo=Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, partes.back(), errMsg);
        if (hijo ==-1){
            errMsg ="Achivo o carpeta '" +partes.back()+ "' no encontrado";
            return -1;
        }
        return hijo;
    }

    //Comando principal
    CommandResult Remove_Command(const vector<string>& tokens) {
        if(!Global::sesionActual.activa) {
            return {false, "ERROR: No hay sesión activa"};
        }

        //Parseo-path
        string path;
        string atributos= joinTokens(tokens);
        static const regex lexic(R"(-path="[^"]+"|-path=[^\s]+)", regex::icase);

        auto begin = sregex_iterator(atributos.begin(), atributos.end(), lexic);
        auto end= sregex_iterator();
        for (auto it=begin; it != end; ++it) {
            string fun =it->str();
            size_t eq=fun.find('=');
            if (eq==string::npos) 
                continue;

            string key= toLowerStr(fun.substr(0, eq));
            string value = fun.substr(eq + 1);
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value =value.substr(1, value.size()-2);
            }

            if (key== "-path"){
                path=value;
            }
        }
        if (path.empty())
            return {false, "ERROR: falta -path"};

        if (path == "/" || path =="")
            return {false, "ERROR: No se puede eliminar la raíz"};

        //Obtencion partición y sbb
        string diskPath =Global::sesionActual.diskPath;
        string id =Global::sesionActual.idParticion;
        Estructuras::PARTITION mountedPart;
        string errMsg;
        if (!Global::GetMountedPartition(id, mountedPart, diskPath, errMsg)) {
            return {false, "ERROR: "+ errMsg};
        }

        Estructuras::SUPERBLOCK sb;
        if (!Ext2Utils::LeerSuperbloque(diskPath, mountedPart.Partition_start, sb, errMsg)) {
            return {false, "ERROR: No se pudo leer suprbloque: "+errMsg};
        }

        vector<string> partes= splitPath(path);
        int inodoPadre =-1;
        int inodoHijo= buscarInodoHijo(diskPath, sb, partes, inodoPadre, errMsg);
        if (inodoHijo== -1){
            return {false, "ERROR: " + errMsg};
        }

     //verificacion que todo se puede eliminar 
        string verifErr;
        if(!verificarPuedeEliminar(diskPath, sb, inodoHijo, Global::sesionActual.esRoot, Global::sesionActual.uid, verifErr)) {
            return {false, "ERROR: No se puede eliminar '" +partes.back() + "': " + verifErr};
        }

        //eliminacin recursiva
        if (!eliminarRecursivo(diskPath, sb, inodoHijo, errMsg)) {
            return {false, "ERROR durante elimiación: " +errMsg};
        }

        Estructuras::INODE inodoPadreObj;
        if(!Ext2Utils::LeerInodo(diskPath, sb, inodoPadre, inodoPadreObj, errMsg)) {
            return {false, "ERROR: No se pudo leer inodo padre: "+ errMsg};
        }

        if (!quitarEntradaPadre(diskPath, sb, inodoPadreObj, inodoHijo, partes.back(), errMsg)) {
            return {false, "ERROR: " + errMsg};
        }

        //Guardar superbloque actualizado 
        if(!sb.Serialize(diskPath, mountedPart.Partition_start, errMsg)) {
            return {false, "ERROR: No se pudo actualizar superbloque: "+ errMsg};
        }
        {
            string journalErr;
            JournalUtils::RegistrarOperacion(diskPath, sb, "remove",path, "", journalErr);
        }

        return {true, "REMOVE: '"+ partes.back() +"' eliminado correctamente"};
    }

}