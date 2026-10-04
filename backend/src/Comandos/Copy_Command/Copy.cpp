#include "Copy.h"
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

    static string toLowerStr(string s) {
        transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return tolower(c); });
        return s;
    }

    static string joinTokens(const vector<string>& tokens) {
        string result;
        for (size_t i=0; i< tokens.size(); ++i) {
            if (i > 0) 
                result += " ";

            result += tokens[i];
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

    //duplicado recursivamente un inodo 
    static int copiarRecursivo(const string& diskPath, Estructuras::SUPERBLOCK& sb, int inodoOrigen, bool esRoot, int uidActual, string& errMsg) {
        //Lectura inodo origen
        Estructuras::INODE inodoOrigenObj;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoOrigen, inodoOrigenObj, errMsg)) {
            return -1;
        }

        //Reservacion inodo nuevo
        int nuevoInodo= Ext2Utils::BuscarInodoLibre(diskPath, sb, errMsg);
        if (nuevoInodo ==-1)
            return -1;

        Estructuras::INODE nuevoInode= inodoOrigenObj;
        for (int i =0; i <15; ++i){
           nuevoInode.I_block[i]= -1; 
        }
        nuevoInode.I_ctime= static_cast<float>(time(nullptr));
        nuevoInode.I_mtime =static_cast<float>(time(nullptr));

        //Caso ARCHIVO
        if (inodoOrigenObj.I_type[0]== '1') {
            string contenido;
            if (!Ext2Utils::LeerArchivo(diskPath, sb, inodoOrigenObj, contenido, errMsg)) {
                return -1;
            }

            if (!Ext2Utils::EscribirInodo(diskPath, sb, nuevoInodo, nuevoInode, errMsg)) {
                return -1;
            }

            if (!contenido.empty()){
                if(!Ext2Utils::EscribirArchivo(diskPath, sb, nuevoInodo, nuevoInode, contenido, errMsg)) {
                    return -1;
                }
            }
            //Marca inodo como usado
            if (!Ext2Utils::MarcarInodoUsado(diskPath, sb, nuevoInodo, errMsg)) {
                return -1;
            }
            return nuevoInodo;
        }

        //Caso CARPETA
        int bloqueNuevo=Ext2Utils::AsignarBloqueLibre(diskPath, sb, errMsg);
        if (bloqueNuevo == -1)
            return -1;

        nuevoInode.I_block[0] = bloqueNuevo;

        //Escritura folderblock con "." y".."
        Estructuras::FOLDERBLOCK fbNuevo;
        memset(&fbNuevo, 0, sizeof(fbNuevo));
        fbNuevo.B_content[0].B_name[0]= '.';
        fbNuevo.B_content[0].B_inodo = nuevoInodo;
        fbNuevo.B_content[1].B_name[0] = '.';
        fbNuevo.B_content[1].B_name[1]= '.';
        fbNuevo.B_content[1].B_inodo = 0;
        fbNuevo.B_content[2].B_name[0]='-';
        fbNuevo.B_content[2].B_inodo = -1;
        fbNuevo.B_content[3].B_name[0] = '-';
        fbNuevo.B_content[3].B_inodo =-1;

        long long offsetBloque =sb.Sb_block_start + (bloqueNuevo * sb.Sb_block_size);
        if (!fbNuevo.Serialize(diskPath, offsetBloque, errMsg)){
            return -1;
        }

        //Escritura inodo y marcado como usado
        if (!Ext2Utils::EscribirInodo(diskPath, sb, nuevoInodo, nuevoInode, errMsg)){
            return -1;
        }
        if (!Ext2Utils::MarcarInodoUsado(diskPath, sb, nuevoInodo, errMsg)) {
            return -1;
        }

        //Recorre hijos del origen
        for (int b =0; b < 12; ++b) {
            if (inodoOrigenObj.I_block[b] == -1){
                continue;
            }
            Estructuras::FOLDERBLOCK fbOrigen;
            long long offOrigen = sb.Sb_block_start+ (inodoOrigenObj.I_block[b] * sb.Sb_block_size);
            if (!fbOrigen.Deserialize(diskPath, offOrigen, errMsg)){
                continue;
            }

            for (int i = 0; i<4; ++i) {
                string nombre(fbOrigen.B_content[i].B_name);
                nombre = nombre.c_str();
                if (nombre.empty() || nombre == "." || nombre == ".." ||
                    fbOrigen.B_content[i].B_inodo == -1) {
                    continue;
                }

                int inodoHijoOrigen = fbOrigen.B_content[i].B_inodo;
                Estructuras::INODE hijoOrigen;
                if (!Ext2Utils::LeerInodo(diskPath, sb, inodoHijoOrigen, hijoOrigen, errMsg)) {
                    continue;
                }

                // Verificar permiso de lectura -> si no, saltar silenciosamente
                if (!PermisosUtils::PuedeLeer(hijoOrigen, esRoot, uidActual)) {
                    continue;
                }

                string errHijo;
                int inodoCopia =copiarRecursivo(diskPath, sb, inodoHijoOrigen, esRoot, uidActual, errHijo);
                if (inodoCopia ==-1){
                    continue;
                }
                Estructuras::INODE nuevoInodeActual;
                if(!Ext2Utils::LeerInodo(diskPath, sb, nuevoInodo, nuevoInodeActual, errMsg)) {
                    continue;
                }

                int slot;
                long long offSlot;
                if (!Ext2Utils::EncontrarSlotEnCarpeta(diskPath, sb, nuevoInodeActual, slot, offSlot, errMsg)) {
                    continue;
                }
                if(!Ext2Utils::EscribirInodo(diskPath, sb, nuevoInodo, nuevoInodeActual, errMsg)) {
                    continue;
                }

                Estructuras::FOLDERBLOCK fbSlot;
                if (!fbSlot.Deserialize(diskPath, offSlot, errMsg)){
                    continue;
                }
                fbSlot.B_content[slot].B_inodo=inodoCopia;
                memset(fbSlot.B_content[slot].B_name, 0, sizeof(fbSlot.B_content[slot].B_name));
                strncpy(fbSlot.B_content[slot].B_name, nombre.c_str(), sizeof(fbSlot.B_content[slot].B_name) - 1);
                if(!fbSlot.Serialize(diskPath, offSlot, errMsg)){
                    continue;
                }
            }
        }
        return nuevoInodo;
    }

    //Busca inodo de un path (devuelve el inodo del último componente)
    static int buscarInodoPorRuta(const string& diskPath, const Estructuras::SUPERBLOCK& sb, const string& ruta, string& errMsg) {
        if (ruta =="/" || ruta.empty()) 
            return 0;

        vector<string> partes= splitPath(ruta);
        int inodoActual =0;
        Estructuras::INODE inode;
        if(!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)){
            return -1;
        }

        for (const string& comp : partes){
            int hijo =Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, comp, errMsg);
            if (hijo==-1){
                errMsg= "'" +comp + "' no encontrado";
                return -1;
            }
            inodoActual=hijo;
            if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) 
                return -1;
        }
        return inodoActual;
    }

    CommandResult Copy_Command(const vector<string>& tokens){
        if (!Global::sesionActual.activa) {
            return {false, "ERROR: No hay ssión activa"};
        }

        //Parseo -path y -destino
        string path, destino;
        string atributos = joinTokens(tokens);
        static const regex lexic(R"(-path="[^"]+"|-path=[^\s]+|-destino="[^"]+"|-destino=[^\s]+)", regex::icase);

        auto begin =sregex_iterator(atributos.begin(), atributos.end(), lexic);
        auto end = sregex_iterator();
        for (auto it= begin; it != end; ++it) {
            string fun = it->str();
            size_t eq=fun.find('=');
            if (eq == string::npos) 
                continue;

            string key =toLowerStr(fun.substr(0, eq));
            string value= fun.substr(eq + 1);
            if (value.size() >= 2 && value.front() == '"' && value.back() =='"') {
                value = value.substr(1, value.size() - 2);
            }
            if (key=="-path"){
               path =value; 
            }
            else if(key== "-destino") {
                destino=value;
            }
        }
        if (path.empty()) {
            return {false, "ERROR: falta -path"};
        }
        
        if (destino.empty()){
            return {false, "ERROR: falta -destino"};
        }

        string diskPath = Global::sesionActual.diskPath;
        string id= Global::sesionActual.idParticion;
        Estructuras::PARTITION mountedPart;
        string errMsg;
        if (!Global::GetMountedPartition(id, mountedPart, diskPath, errMsg)) {
            return {false, "ERROR: " +errMsg};
        }

        Estructuras::SUPERBLOCK sb;
        if (!Ext2Utils::LeerSuperbloque(diskPath, mountedPart.Partition_start, sb, errMsg)) {
            return {false, "ERROR: No se pudo leer superbloque: " + errMsg};
        }

        //Localizacion del origen
        int inodoOrigen=buscarInodoPorRuta(diskPath, sb, path, errMsg);
        if (inodoOrigen ==-1){
            return{false, "ERROR: Ruta origen no existe: "+errMsg};
        }

        //verificacion de permiso de lectura sobre el origen
        Estructuras::INODE inodoOrigenObj;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoOrigen, inodoOrigenObj, errMsg)){
            return{false, "ERROR: " + errMsg};
        }
        if (!PermisosUtils::PuedeLeer(inodoOrigenObj, Global::sesionActual.esRoot, Global::sesionActual.uid)) {
            return {false, "ERROR: Sin permiso de lectura sobre '" + path + "'"};
        }

        //Localizacion del destino
        int inodoDestino = buscarInodoPorRuta(diskPath, sb, destino, errMsg);
        if (inodoDestino ==-1) {
            return {false, "ERROR: Carpeta destino no existe: " + errMsg};
        }
        Estructuras::INODE inodoDestinoObj;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoDestino, inodoDestinoObj, errMsg)) {
            return {false, "ERROR: "+ errMsg};
        }
        if (inodoDestinoObj.I_type[0] !='0') {
            return {false, "ERROR: El destino no es una carpeta"};
        }
        if (!PermisosUtils::PuedeEscribir(inodoDestinoObj, Global::sesionActual.esRoot, Global::sesionActual.uid)){
            return {false, "ERROR: Sin permso de escriura sobre el destino"};
        }

        string nombreOrigen;
        {
            vector<string> partes= splitPath(path);
            if (partes.empty()){
                return {false, "ERROR: path inválido"};
            }
            nombreOrigen= partes.back();
        }

        int existente= Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inodoDestinoObj, nombreOrigen, errMsg);
        if (existente != -1){
            return {false, "ERROR: Ya exste '" + nombreOrigen + "' en el destino"};
        }

        //Copia recursiva 
        string errCopia;
        int inodoCopia =copiarRecursivo(diskPath, sb, inodoOrigen, Global::sesionActual.esRoot, Global::sesionActual.uid, errCopia);
        if (inodoCopia== -1) {
            return {false, "ERROR: "+errCopia};
        }

        //Agregacion de entrada en el folderblock del destino
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoDestino, inodoDestinoObj, errMsg)) {
            return {false, "ERROR: " +errMsg};
        }

        int slot;
        long long offSlot;
        if (!Ext2Utils::EncontrarSlotEnCarpeta(diskPath, sb, inodoDestinoObj, slot, offSlot, errMsg)) {
            return {false, "ERROR: " +errMsg};
        }
        if (!Ext2Utils::EscribirInodo(diskPath, sb, inodoDestino, inodoDestinoObj, errMsg)) {
            return {false, "ERROR: "+ errMsg};
        }
        Estructuras::FOLDERBLOCK fbDestino;
        if (!fbDestino.Deserialize(diskPath, offSlot, errMsg)) {
            return {false, "ERROR: " + errMsg};
        }

        fbDestino.B_content[slot].B_inodo= inodoCopia;
        memset(fbDestino.B_content[slot].B_name, 0, sizeof(fbDestino.B_content[slot].B_name));
        strncpy(fbDestino.B_content[slot].B_name, nombreOrigen.c_str(), sizeof(fbDestino.B_content[slot].B_name)- 1);
        if (!fbDestino.Serialize(diskPath, offSlot, errMsg)) {
            return{false, "ERROR: "+errMsg};
        }

        inodoDestinoObj.I_mtime =static_cast<float>(time(nullptr));
        Ext2Utils::EscribirInodo(diskPath, sb, inodoDestino, inodoDestinoObj, errMsg);

        //Guardado superbloque actualizado
        if (!sb.Serialize(diskPath, mountedPart.Partition_start, errMsg)){
            return {false, "ERROR: No se pudo actualizar superbloque: " +errMsg};
        }
        {
            string journalErr;
            JournalUtils::RegistrarOperacion(diskPath, sb, "copy", path, destino, journalErr);
        }
        return {true,"COPY: '" + nombreOrigen +"' copiado a '" + destino + "'"};
    }

}