#include "Mkfile.h"
#include "../../Global/Sesion.h"
#include "../../Global/MountedPartitions.h"
#include "../../Utils/Ext2Utils.h"
#include "../../Estructuras/Str_Folderblock/FOLDERBLOCK.h"
#include "../../Estructuras/Str_Fileblock/FILEBLOCK.h"
#include <iostream>
#include <regex>
#include <sstream>
#include <fstream>
#include <cstring>
#include <ctime>

using namespace std;

namespace Comandos {

    static string toLowerStr(string s) {
        transform(s.begin(), s.end(), s.begin(),
                  [](unsigned char c) { return tolower(c); });
        return s;
    }

    static string joinTokens(const vector<string>& tokens) {
        string result;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i > 0) result += " ";
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

    // Navega la ruta y devuelve el inodo de la carpeta padre + el nombre del archivo final
    static int buscarInodoPorRuta(const string& diskPath, const Estructuras::SUPERBLOCK& sb,
                                  const string& ruta, int& inodoPadre,
                                  string& nombreArchivo, string& errMsg) {
        vector<string> partes = splitPath(ruta);
        if (partes.empty()) {
            errMsg = "Ruta vacía";
            return -1;
        }

        nombreArchivo = partes.back();
        partes.pop_back();

        int inodoActual = 0;
        Estructuras::INODE inode;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) {
            return -1;
        }

        for (const string& carpeta : partes) {
            int hijo = Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, carpeta, errMsg);
            if (hijo == -1) {
                errMsg = "Carpeta '" + carpeta + "' no encontrada";
                return -1;
            }
            inodoActual = hijo;
            if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) {
                return -1;
            }
        }
        inodoPadre = inodoActual;
        return inodoActual;
    }

    CommandResult Mkfile_Command(const vector<string>& tokens) {
        if (!Global::sesionActual.activa) {
            return {false, "ERROR: No hay sesión activa"};
        }

        string path;
        bool recursive = false;
        int size = -1;
        string contPath;

        string atributos = joinTokens(tokens);
        static const regex lexic(
            R"(-path="[^"]+"|-path=[^\s]+|-r|-size=-?\d+|-cont="[^"]+"|-cont=[^\s]+)",
            regex::icase);

        vector<string> found;
        auto begin = sregex_iterator(atributos.begin(), atributos.end(), lexic);
        auto end = sregex_iterator();
        for (auto it = begin; it != end; ++it)
            found.push_back(it->str());

        if (found.size() != tokens.size()) {
            for (const auto& token : tokens) {
                if (!regex_search(token, lexic)) {
                    return {false, "ERROR: Parámetro no reconocido: " + token + " en MKFILE"};
                }
            }
        }

        for (const auto& fun : found) {
            if (fun == "-r") {
                recursive = true;
                continue;
            }
            size_t eqPos = fun.find('=');
            if (eqPos == string::npos) {
                return {false, "ERROR: formato inválido: " + fun};
            }
            string key = toLowerStr(fun.substr(0, eqPos));
            string value = fun.substr(eqPos + 1);
            if (!value.empty() && value.front() == '"' && value.back() == '"')
                value = value.substr(1, value.size() - 2);

            if (key == "-path") {
                if (value.empty()) return {false, "ERROR: path vacío"};
                path = value;
            } else if (key == "-size") {
                try {
                    size = stoi(value);
                    if (size < 0) return {false, "ERROR: size debe ser >= 0"};
                } catch (...) {
                    return {false, "ERROR: size debe ser un número"};
                }
            } else if (key == "-cont") {
                if (value.empty()) return {false, "ERROR: cont vacío"};
                contPath = value;
            } else {
                return {false, "ERROR: parámetro desconocido: " + key};
            }
        }

        if (path.empty()) {
            return {false, "ERROR: falta -path"};
        }

        string diskPath = Global::sesionActual.diskPath;
        string id = Global::sesionActual.idParticion;
        Estructuras::PARTITION mountedPart;
        string errMsg;
        if (!Global::GetMountedPartition(id, mountedPart, diskPath, errMsg)) {
            return {false, "ERROR: " + errMsg};
        }

        Estructuras::SUPERBLOCK sb;
        if (!Ext2Utils::LeerSuperbloque(diskPath, mountedPart.Partition_start, sb, errMsg)) {
            return {false, "ERROR: No se pudo leer superbloque: " + errMsg};
        }

        int inodoPadre = -1;
        string nombreArchivo;
        int padre = buscarInodoPorRuta(diskPath, sb, path, inodoPadre, nombreArchivo, errMsg);

        // Si la ruta padre no existe
        if (padre == -1) {
            if (!recursive) {
                return {false, "ERROR: Ruta no existe: " + errMsg};
            }

            // Es recursivo → crear las carpetas intermedias
            vector<string> partes = splitPath(path);
            string nombreFinal = partes.back();
            partes.pop_back();

            int inodoActual = 0;
            int carpetasCreadas = 0;

            for (const string& comp : partes) {
                Estructuras::INODE inodeActual;
                if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inodeActual, errMsg)) {
                    return {false, "ERROR: " + errMsg};
                }

                int hijo = Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inodeActual, comp, errMsg);
                if (hijo != -1) {
                    inodoActual = hijo;
                } else {
                    int nuevo = Ext2Utils::CrearCarpetaEnPadre(diskPath, sb, inodoActual, comp, errMsg);
                    if (nuevo == -1) {
                        return {false, "ERROR: No se pudo crear carpeta '" + comp + "': " + errMsg};
                    }
                    inodoActual = nuevo;
                    carpetasCreadas++;
                }
            }

            // Actualizar superbloque por las carpetas creadas
            if (carpetasCreadas > 0) {
                sb.Sb_inodes_count += carpetasCreadas;
                sb.Sb_free_inodes_count -= carpetasCreadas;
                sb.Sb_blocks_count += carpetasCreadas;
                sb.Sb_free_blocks_count -= carpetasCreadas;
                if (!sb.Serialize(diskPath, mountedPart.Partition_start, errMsg)) {
                    return {false, "ERROR: No se pudo actualizar superbloque: " + errMsg};
                }
            }

            inodoPadre = inodoActual;
            nombreArchivo = nombreFinal;
        }

        // Leer inodo de la carpeta padre
        Estructuras::INODE inodePadreObj;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoPadre, inodePadreObj, errMsg)) {
            return {false, "ERROR: No se pudo leer inodo padre: " + errMsg};
        }

        // Verificar que el archivo no exista
        int existente = Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inodePadreObj, nombreArchivo, errMsg);
        if (existente != -1) {
            return {false, "ERROR: El archivo '" + nombreArchivo + "' ya existe en la carpeta"};
        }

        // Buscar slot libre (extendiendo si es necesario)
        int slotLibre;
        long long blockOffsetSlot;
        if (!Ext2Utils::EncontrarSlotEnCarpeta(diskPath, sb, inodePadreObj, slotLibre, blockOffsetSlot, errMsg)) {
            return {false, "ERROR: " + errMsg};
        }

        // Re-escribir inodo padre (por si se extendió)
        if (!Ext2Utils::EscribirInodo(diskPath, sb, inodoPadre, inodePadreObj, errMsg)) {
            return {false, "ERROR: " + errMsg};
        }

        // Leer el bloque específico donde está el slot
        Estructuras::FOLDERBLOCK folderBlockSlot;
        if (!folderBlockSlot.Deserialize(diskPath, blockOffsetSlot, errMsg)) {
            return {false, "ERROR: " + errMsg};
        }

        // Contenido del archivo
        string contenido;
        if (!contPath.empty()) {
            ifstream fileCont(contPath);
            if (!fileCont.is_open()) {
                return {false, "ERROR: No se pudo abrir archivo de contenido: " + contPath};
            }
            stringstream buffer;
            buffer << fileCont.rdbuf();
            contenido = buffer.str();
        } else if (size >= 0) {
            contenido.reserve(size);
            for (int i = 0; i < size; ++i) {
                contenido.push_back('0' + (i % 10));
            }
        } else {
            contenido = "";
        }

        // Buscar inodo libre
        int nuevoInodo = Ext2Utils::BuscarInodoLibre(diskPath, sb, errMsg);
        if (nuevoInodo == -1) {
            return {false, "ERROR: No hay inodos libres: " + errMsg};
        }

        // Crear inodo del archivo
        Estructuras::INODE newInode;
        memset(&newInode, 0, sizeof(newInode));
        newInode.I_uid = Global::sesionActual.uid;
        newInode.I_gid = Global::sesionActual.gid;
        newInode.I_size = contenido.size();
        newInode.I_atime = static_cast<float>(time(nullptr));
        newInode.I_ctime = static_cast<float>(time(nullptr));
        newInode.I_mtime = static_cast<float>(time(nullptr));
        newInode.I_type[0] = '1';
        newInode.I_perm[0] = '6';
        newInode.I_perm[1] = '6';
        newInode.I_perm[2] = '4';
        for (int i = 0; i < 15; ++i) newInode.I_block[i] = -1;

        // Escribir contenido (asigna bloques) o solo el inodo si está vacío
        if (!contenido.empty()) {
            if (!Ext2Utils::EscribirInodo(diskPath, sb, nuevoInodo, newInode, errMsg)) {
                return {false, "ERROR: No se pudo escribir inodo: " + errMsg};
            }
            if (!Ext2Utils::EscribirArchivo(diskPath, sb, nuevoInodo, newInode, contenido, errMsg)) {
                return {false, "ERROR: No se pudo escribir contenido: " + errMsg};
            }
        } else {
            if (!Ext2Utils::EscribirInodo(diskPath, sb, nuevoInodo, newInode, errMsg)) {
                return {false, "ERROR: No se pudo escribir inodo: " + errMsg};
            }
        }

        // Marcar inodo como usado
        if (!Ext2Utils::MarcarInodoUsado(diskPath, sb, nuevoInodo, errMsg)) {
            return {false, "ERROR: " + errMsg};
        }

        // Actualizar superbloque
        sb.Sb_inodes_count++;
        sb.Sb_free_inodes_count--;
        if (!sb.Serialize(diskPath, mountedPart.Partition_start, errMsg)) {
            return {false, "ERROR: No se pudo actualizar superbloque: " + errMsg};
        }

        // Agregar entrada en la carpeta padre
        folderBlockSlot.B_content[slotLibre].B_inodo = nuevoInodo;
        strncpy(folderBlockSlot.B_content[slotLibre].B_name, nombreArchivo.c_str(), 12);
        if (!folderBlockSlot.Serialize(diskPath, blockOffsetSlot, errMsg)) {
            return {false, "ERROR: No se pudo actualizar carpeta padre: " + errMsg};
        }

        // Actualizar fecha del padre
        inodePadreObj.I_mtime = static_cast<float>(time(nullptr));
        if (!Ext2Utils::EscribirInodo(diskPath, sb, inodoPadre, inodePadreObj, errMsg)) {
            return {false, "ERROR: No se pudo actualizar inodo padre: " + errMsg};
        }

        return {true, "MKFILE: Archivo '" + nombreArchivo + "' creado exitosamente (inodo " + to_string(nuevoInodo) + ")"};
    }
} 