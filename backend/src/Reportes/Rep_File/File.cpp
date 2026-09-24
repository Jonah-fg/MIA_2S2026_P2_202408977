#include "File.h"
#include "../../Utils/Utilities.h"
#include "../../Utils/Ext2Utils.h"
#include "../../Estructuras/Str_Inode/INODE.h"
#include "../../Estructuras/Str_Folderblock/FOLDERBLOCK.h"
#include <fstream>
#include <sstream>
using namespace std;

namespace Reportes{
    //division de una ruta en componenes
    static vector<string> splitPath(const string& path) {
        vector<string> partes;
        stringstream ss(path);
        string item;
        while (getline(ss, item, '/')){
            if (!item.empty()) partes.push_back(item);
        }
        return partes;
    }


    static int obtenerInodoArchivo(const string& diskPath, const Estructuras::SUPERBLOCK& sb, const string& ruta, string& errMsg) {
        vector<string> partes = splitPath(ruta);
        if (partes.empty()) {
            errMsg ="Ruta vacía";
            return -1;
        }
        int inodoActual= 0;
        Estructuras::INODE inode;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)){
            return -1;
        }

        for (const string& componente : partes){
            int hijo = Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, componente, errMsg);
            if (hijo == -1) {
                errMsg = "Componente '" + componente + "' no encontrado";
                return -1;
            }
            inodoActual = hijo;
            if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) return -1;

        }
        if (inode.I_type[0] !='1') {
            errMsg = "No es un archivo";
            return -1;
        }
        return inodoActual;
    }

    bool ReporteFile(const Estructuras::SUPERBLOCK& sb, const string& path, const string& diskPath, const string& pathFile, string& errMsg) {
        if (!Utilities::CreateParentDir(path, errMsg)) {
            return false;
        }

        if (pathFile.empty()){
            errMsg = "Falta -path_file_ls para el reprte file";
            return false;
        }

        //Obtener el inodo del archivo
        int inodoNum= obtenerInodoArchivo(diskPath, sb, pathFile, errMsg);
        if (inodoNum ==-1){
            return false;
        }

        //Lectura del inodo
        Estructuras::INODE inode;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoNum, inode, errMsg)) {
            return false;
        }

        //lectuyra del contenido
        string contenido;
        if (!Ext2Utils::LeerArchivo(diskPath, sb, inode, contenido, errMsg)) {
            return false;
        }

        //reporte
        ofstream out(path);
        if (!out.is_open()){
            errMsg ="No se pudo crear el archivo de reporte";
            return false;
        }
        out << "------------REPORTE FILE---------\n";
        out<< "Nombre: "<< pathFile << "\n";
        out << "Inodo: "<< inodoNum << "\n";
        out<< "Tamaño: " << inode.I_size <<" bytes\n\n";
        out<< "--------CONTENIDO--------\n";
        out <<contenido << "\n";
        out.close();
        errMsg.clear();
        return true;
    }
}