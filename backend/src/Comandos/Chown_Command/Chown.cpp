
#include "Chown.h"
#include "../../Global/Sesion.h"
#include "../../Global/MountedPartitions.h"
#include "../../Utils/Ext2Utils.h"
#include "../../Utils/JournalUtils.h"
#include "../../Utils/UsersUtils.h"
#include "../../Estructuras/Str_Folderblock/FOLDERBLOCK.h"
#include <regex>
#include <sstream>
#include <ctime>

using namespace std;
namespace Comandos {
    static string toLowerStr(string s) {
        transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return tolower(c); });
        return s;
    }

    static string joinTokens(const vector<string>& tokens) {
        string result;
        for (size_t i= 0; i<tokens.size(); ++i) {
            if (i > 0) {
                result += " ";
            }
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

    //Aplicacion chown recursivamente al inodo y a sus descendientes.
    static bool aplicarChown(const string& diskPath, Estructuras::SUPERBLOCK& sb, int inodoNum, int nuevoUID, bool recursivo, int uidActual, bool esRoot, string& errMsg) {
        Estructuras::INODE inodo;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoNum, inodo, errMsg)){
            return false;
        } 

        //regla de solo root o el dueño pueden cambiar el propietario
        if (!esRoot && inodo.I_uid !=uidActual){
            errMsg ="Sin permiso para cambiar propietario de este inodo";
            return false;
        }
        inodo.I_uid=nuevoUID;
        inodo.I_mtime = static_cast<float>(time(nullptr));
        if (!Ext2Utils::EscribirInodo(diskPath, sb, inodoNum, inodo, errMsg)) {
            return false;
        }

        //si es carpeta y recursivo, se aplñican a hijos
        if(recursivo && inodo.I_type[0]== '0'){
            for (int b=0; b< 12; ++b){
                if (inodo.I_block[b] ==-1) 
                    continue;

                Estructuras::FOLDERBLOCK fb;
                long long off = sb.Sb_block_start +(inodo.I_block[b] * sb.Sb_block_size);
                if(!fb.Deserialize(diskPath, off, errMsg)) {
                    continue;
                }

                for (int i = 0; i<4; ++i){
                    string nombre(fb.B_content[i].B_name);
                    nombre=nombre.c_str();
                    if(nombre.empty() || nombre =="." || nombre == ".." ||
                        fb.B_content[i].B_inodo == -1){
                            continue;
                        }
                    string e;
                    aplicarChown(diskPath, sb, fb.B_content[i].B_inodo, nuevoUID, true, uidActual, esRoot, e);
                }
            }
        }
        return true;
    }

    //Busca inodo por ruta completa
    static int buscarInodoPorRuta(const string& diskPath, const Estructuras::SUPERBLOCK& sb, const string& ruta, string& errMsg) {
        if(ruta == "/" || ruta.empty()){
            return 0;
        }

        vector<string> partes= splitPath(ruta);
        int inodoActual=0;
        Estructuras::INODE inode;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) {
            return -1;
        }

        for(const string& comp : partes){
            int hijo= Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, comp, errMsg);
            if (hijo ==-1) {
                errMsg = "'"+ comp +"'no encontrado";
                return -1;
            }
            inodoActual=hijo;
            if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)){
                return -1;
            }
        }
        return inodoActual;
    }

    CommandResult Chown_Command(const vector<string>& tokens){
        if (!Global::sesionActual.activa){
            return {false, "ERROR: No hay sesión aciva"};
        }

        //ParseO -path, -r, -usuario
        string path, nombreUsuario;
        bool recursivo =false;
        string atributos = joinTokens(tokens);
        static const regex lexic(R"(-path="[^"]+"|-path=[^\s]+|-usuario="[^"]+"|-usuario=[^\s]+|-r)", regex::icase);

        auto begin= sregex_iterator(atributos.begin(), atributos.end(), lexic);
        auto end =sregex_iterator();
        for (auto it=begin; it != end; ++it) {
            string fun =it->str();
            if (toLowerStr(fun)=="-r"){
                recursivo=true;
                continue;
            }
            size_t eq=fun.find('=');
            if (eq ==string::npos) {
                continue;
            }

            string key= toLowerStr(fun.substr(0, eq));
            string value =fun.substr(eq + 1);
            if (value.size() >= 2 && value.front()== '"' && value.back()=='"'){
                value = value.substr(1, value.size() - 2);
            }
            if (key== "-path"){
                path = value;
            }
            else if(key == "-usuario"){
                nombreUsuario = value; 
            } 
        }

        if (path.empty()) 
        {
            return {false, "ERROR: falta -path"};
        }
        if (nombreUsuario.empty()) {
            return {false, "ERROR: falta -usuario"};
        }

        //partición y sb
        string diskPath = Global::sesionActual.diskPath;
        string id=Global::sesionActual.idParticion;
        Estructuras::PARTITION mountedPart;
        string errMsg;
        if (!Global::GetMountedPartition(id, mountedPart, diskPath, errMsg)) {
            return {false, "ERROR: "+ errMsg};
        }
        Estructuras::SUPERBLOCK sb;
        if (!Ext2Utils::LeerSuperbloque(diskPath, mountedPart.Partition_start, sb, errMsg)) {
            return {false, "ERROR: No se pudo leer suprbloque: "+ errMsg};
        }

        //Lectura users.txt para buscar el UID 
        Estructuras::INODE inodeUsers;
        if (!Ext2Utils::LeerInodo(diskPath, sb, 1, inodeUsers, errMsg)) {
            return {false, "ERROR: No se pudo leer users.txt: " + errMsg};
        }
        string contenidoUsers;
        if (!Ext2Utils::LeerArchivo(diskPath, sb, inodeUsers, contenidoUsers, errMsg)) {
            return {false, "ERROR: No se pudo leer users.txt: " + errMsg};
        }
        vector<UsersUtils::Grupo> grupos;
        vector<UsersUtils::Usuario> usuarios;
        if (!UsersUtils::ParsearUsersTXT(contenidoUsers, grupos, usuarios, errMsg)) {
            return {false, "ERROR: "+ errMsg};
        }

        int nuevoUID=UsersUtils::BuscarUIDPorNombre(usuarios, nombreUsuario);
        if (nuevoUID == -1) {
            return{false, "ERROR: Usuario '" + nombreUsuario + "' no existe"};
        }

        //inodo del path 
        int inodoObjetivo =buscarInodoPorRuta(diskPath, sb, path, errMsg);
        if (inodoObjetivo == -1){
            return {false, "ERROR: Ruta no exste: " + errMsg};
        }

        //Aplicacion chown 
        string errChown;
        if (!aplicarChown(diskPath, sb, inodoObjetivo, nuevoUID, recursivo, Global::sesionActual.uid, Global::sesionActual.esRoot, errChown)) {
            return {false,"ERROR: " + errChown};
        }
        {
            string journalErr;
            JournalUtils::RegistrarOperacion(diskPath, sb, "chown", path, nombreUsuario, journalErr);
        }
        return {true, "CHOWN: propietario de '" +path+ "' cambiado a '" + nombreUsuario+ "'" + (recursivo ?" (recursivo)" : "")};
    }

}