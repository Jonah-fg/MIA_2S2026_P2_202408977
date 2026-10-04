#include "Move.h"
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
namespace Comandos {
    static string toLowerStr(string s){
        transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return tolower(c); });
        return s;
    }

    static string joinTokens(const vector<string>& tokens) {
        string result;
        for (size_t i =0; i<tokens.size(); ++i) {
            if (i >0) result+= " ";
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

    //Navega y devuelve el inodo del padre + nombre del hijo
    static int buscarInodoHijo(const string& diskPath, const Estructuras::SUPERBLOCK& sb, const vector<string>& partes, int& inodoPadreOut, string& errMsg) {
        if (partes.empty()){
            errMsg = "Ruta vacía";
            return -1; 
        }

        int inodoActual =0;
        Estructuras::INODE inode;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) 
            return -1;

        for (size_t i = 0; i+1 < partes.size(); ++i) {
            int hijo = Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, partes[i], errMsg);
            if (hijo ==-1) {
                errMsg = "Carpeta '" + partes[i] + "'no encntrada";
                return -1;
            }
            inodoActual = hijo;
            if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) {
                return -1;
            }
        }
        inodoPadreOut= inodoActual;

        int hijo = Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, partes.back(), errMsg);
        if (hijo == -1) {
            errMsg = "Archivo o carpeta '" + partes.back() + "' no encontrado";
            return -1;
        }
        return hijo;
    }

    // Quita la entrada del folderblock del padre
    static bool quitarEntrada(const string& diskPath, const Estructuras::SUPERBLOCK& sb, Estructuras::INODE& inodoPadre, int inodoHijo, const string& nombreHijo, string& errMsg) {
        for (int b=0; b <12; ++b) {
            if (inodoPadre.I_block[b] ==-1){
                continue;
            }

            Estructuras::FOLDERBLOCK fb;
            long long off = sb.Sb_block_start + (inodoPadre.I_block[b] * sb.Sb_block_size);
            if (!fb.Deserialize(diskPath, off, errMsg)) {
                continue;
            }

            for (int i =0; i<4; ++i) {
                string nombre(fb.B_content[i].B_name);
                nombre =nombre.c_str();
                if (nombre == nombreHijo && fb.B_content[i].B_inodo== inodoHijo){
                    memset(fb.B_content[i].B_name, 0, sizeof(fb.B_content[i].B_name));
                    fb.B_content[i].B_name[0] ='-';
                    fb.B_content[i].B_inodo =-1;
                    return fb.Serialize(diskPath, off, errMsg);
                }
            }
        }
        errMsg ="No se encontró la entrda en el padre origen";
        return false;
    }

    CommandResult Move_Command(const vector<string>& tokens) {
        if (!Global::sesionActual.activa) {
            return {false, "ERROR: No hay sesión activa"};
        }

        //Parseo -path y -destino 
        string path, destino;
        string atributos = joinTokens(tokens);
        static const regex lexic(R"(-path="[^"]+"|-path=[^\s]+|-destino="[^"]+"|-destino=[^\s]+)", regex::icase);

        auto begin= sregex_iterator(atributos.begin(), atributos.end(), lexic);
        auto end =sregex_iterator();
        for (auto it=begin; it != end; ++it) {
            string fun = it->str();
            size_t eq= fun.find('=');
            if (eq==string::npos)
                continue;

            string key = toLowerStr(fun.substr(0, eq));
            string value= fun.substr(eq + 1);
            if (value.size()>= 2 && value.front() =='"' && value.back() == '"') {
                value =value.substr(1, value.size()- 2);
            }
            if (key== "-path") 
                path = value;

            else if (key=="-destino"){
               destino = value; 
            }
        }

        if (path.empty()) {
            return {false, "ERROR: falta -path"};
        }

        if (destino.empty()) {
            return {false, "ERROR: falta -destino"};
        }

        string diskPath = Global::sesionActual.diskPath;
        string id =Global::sesionActual.idParticion;
        Estructuras::PARTITION mountedPart;
        string errMsg;
        if (!Global::GetMountedPartition(id, mountedPart, diskPath, errMsg)) {
            return {false, "ERROR: "+ errMsg};
        }

        Estructuras::SUPERBLOCK sb;
        if (!Ext2Utils::LeerSuperbloque(diskPath, mountedPart.Partition_start, sb, errMsg)) {
            return {false, "ERROR: No se pudo leer superbloque: "+errMsg};
        }

        //Encontrar origen
        vector<string> partesOrigen =splitPath(path);
        int inodoPadreOrigen = -1;
        int inodoOrigen =buscarInodoHijo(diskPath, sb, partesOrigen, inodoPadreOrigen, errMsg);
        if (inodoOrigen ==-1) {
            return {false, "ERROR: "+ errMsg};
        }

        Estructuras::INODE inodoOrigenObj;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoOrigen, inodoOrigenObj, errMsg)) {
            return {false, "ERROR: No se pudo leer inodo origen: " +errMsg};
        }
        if (!PermisosUtils::PuedeEscribir(inodoOrigenObj, Global::sesionActual.esRoot, Global::sesionActual.uid)) {
            return {false, "ERROR: Sin permiso de escrtura sobre '" + partesOrigen.back() +"'"};
        }

        //encontrando carpeta destino
        int inodoDestino =0;
        if (destino != "/" && !destino.empty()) {
            vector<string> partesDestino = splitPath(destino);
            int padreFalso = -1;
            int inodoActual=0;
            Estructuras::INODE inode;
            if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) {
                return {false, "ERROR: No se pudo leer raíz: " + errMsg};
            }
            for (const string& comp : partesDestino) {
                int hijo = Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, comp, errMsg);
                if (hijo ==-1){
                    return {false, "ERROR: Carpeta destino no existe: '" + comp +"' no encontrado"};
                }
                inodoActual = hijo;
                if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) {
                    return {false, "ERROR: "+ errMsg};
                }
            }
            inodoDestino = inodoActual;
        }

        //Verificacion que el destino sea carpeta
        Estructuras::INODE inodoDestinoObj;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoDestino, inodoDestinoObj, errMsg)){
            return {false, "ERROR: No se pudo leer inodo destino: " + errMsg};
        }
        if(inodoDestinoObj.I_type[0] !='0'){
            return{false, "ERROR: El destino no es una carpeta"};
        }

        //Verificar que no exista ya en destino
        int existente =Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inodoDestinoObj, partesOrigen.back(), errMsg);
        if (existente != -1){
            return {false, "ERROR: Ya existe un archivo o carpeta '" +partesOrigen.back() + "' en el destino"};
        }

        //quitar entrada del padre origen
        Estructuras::INODE inodoPadreOrigenObj;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoPadreOrigen, inodoPadreOrigenObj, errMsg)) {
            return {false, "ERROR: "+ errMsg};
        }
        if (!quitarEntrada(diskPath, sb, inodoPadreOrigenObj, inodoOrigen, partesOrigen.back(), errMsg)) {
            return{false, "ERROR: " + errMsg};
        }

        //agregando entrada al folderblock del padre destino
        int slotLibre;
        long long blockOffsetSlot;
        if (!Ext2Utils::EncontrarSlotEnCarpeta(diskPath, sb, inodoDestinoObj, slotLibre, blockOffsetSlot, errMsg)) {
            return {false, "ERROR: "+errMsg};
        }
        if (!Ext2Utils::EscribirInodo(diskPath, sb, inodoDestino, inodoDestinoObj, errMsg)) {
            return {false, "ERROR: "+ errMsg};
        }

        Estructuras::FOLDERBLOCK fbDestino;
        if (!fbDestino.Deserialize(diskPath, blockOffsetSlot, errMsg)) {
            return {false, "ERROR: " +errMsg};
        }
        fbDestino.B_content[slotLibre].B_inodo= inodoOrigen;
        memset(fbDestino.B_content[slotLibre].B_name, 0, sizeof(fbDestino.B_content[slotLibre].B_name));
        strncpy(fbDestino.B_content[slotLibre].B_name, partesOrigen.back().c_str(), sizeof(fbDestino.B_content[slotLibre].B_name) - 1);
        if (!fbDestino.Serialize(diskPath, blockOffsetSlot, errMsg)){
            return {false, "ERROR: "+ errMsg};
        }
 //actualazcion del time
        time_t ahora=time(nullptr);
        inodoPadreOrigenObj.I_mtime= static_cast<float>(ahora);
        inodoDestinoObj.I_mtime =static_cast<float>(ahora);

        if(!Ext2Utils::EscribirInodo(diskPath, sb, inodoPadreOrigen, inodoPadreOrigenObj, errMsg)) {
            return {false, "ERROR: "+ errMsg};
        }
        if (!Ext2Utils::EscribirInodo(diskPath, sb, inodoDestino, inodoDestinoObj, errMsg)) {
            return {false, "ERROR: " + errMsg};
        }
        {
            string journalErr;
            JournalUtils::RegistrarOperacion(diskPath, sb, "move", path, destino, journalErr);
        }
        return {true, "MOVE: '" + partesOrigen.back() +"' movido a '" + destino + "'"};
    }

}