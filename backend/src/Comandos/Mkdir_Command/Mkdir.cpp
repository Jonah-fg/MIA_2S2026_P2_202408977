#include "Mkdir.h"
#include "../../Global/Sesion.h"
#include "../../Global/MountedPartitions.h"
#include "../../Utils/Ext2Utils.h"
#include "../../Estructuras/Str_Folderblock/FOLDERBLOCK.h"
#include <regex>
#include <sstream>
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

    // Navega la ruta y devuelve el inodo de la carpeta padre + el nombre de la carpeta final
    static int buscarInodoPadre(const string& diskPath, const Estructuras::SUPERBLOCK& sb,
                                const string& ruta, string& nombreCarpeta, string& errMsg) {
        vector<string> partes = splitPath(ruta);
        if (partes.empty()) {
            errMsg = "Ruta vacía";
            return -1;
        }
        nombreCarpeta = partes.back();
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
            if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) return -1;
        }
        return inodoActual;
    }

    CommandResult Mkdir_Command(const vector<string>& tokens) {
        if (!Global::sesionActual.activa) {
            return {false, "ERROR: No hay sesión activa"};
        }

        string path;
        bool recursive = false;

        string atributos = joinTokens(tokens);
        static const regex lexic(R"(-path="[^"]+"|-path=[^\s]+|-p)", regex::icase);

        vector<string> found;
        auto begin = sregex_iterator(atributos.begin(), atributos.end(), lexic);
        auto end = sregex_iterator();
        for (auto it = begin; it != end; ++it)
            found.push_back(it->str());

        if (found.size() != tokens.size()) {
            for (const auto& token : tokens) {
                if (!regex_search(token, lexic)) {
                    return {false, "ERROR: Parámetro no reconocido: " + token + " en MKDIR"};
                }
            }
        }

        for (const auto& fun : found) {
            if (fun == "-p") {
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
            } else {
                return {false, "ERROR: parámetro desconocido: " + key};
            }
        }

        if (path.empty()) return {false, "ERROR: falta -path"};

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

        string nombreCarpeta;
        int inodoPadre = buscarInodoPadre(diskPath, sb, path, nombreCarpeta, errMsg);

        // Si la ruta padre no existe y NO es recursivo → error
        if (inodoPadre == -1 && !recursive) {
            return {false, "ERROR: Ruta no existe: " + errMsg};
        }

        // Si es recursivo → crear las carpetas intermedias
        if (inodoPadre == -1 && recursive) {
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

            // Crear la carpeta final
            int nuevo = Ext2Utils::CrearCarpetaEnPadre(diskPath, sb, inodoActual, nombreFinal, errMsg);
            if (nuevo == -1) {
                return {false, "ERROR: No se pudo crear carpeta final: " + errMsg};
            }
            carpetasCreadas++;

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

            return {true, "MKDIR: Carpeta '" + nombreFinal + "' creada recursivamente (inodo " + to_string(nuevo) + ")"};
        }

        // Caso normal: la ruta padre existe
        Estructuras::INODE inodePadre;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoPadre, inodePadre, errMsg)) {
            return {false, "ERROR: No se pudo leer inodo padre: " + errMsg};
        }

        // Verificar que la carpeta no exista
        int existente = Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inodePadre, nombreCarpeta, errMsg);
        if (existente != -1) {
            return {false, "ERROR: La carpeta '" + nombreCarpeta + "' ya existe"};
        }

        // Crear la carpeta
        int nuevo = Ext2Utils::CrearCarpetaEnPadre(diskPath, sb, inodoPadre, nombreCarpeta, errMsg);
        if (nuevo == -1) {
            return {false, "ERROR: " + errMsg};
        }

        // Actualizar superbloque (1 inodo + 1 bloque)
        sb.Sb_inodes_count++;
        sb.Sb_free_inodes_count--;
        sb.Sb_blocks_count++;
        sb.Sb_free_blocks_count--;
        if (!sb.Serialize(diskPath, mountedPart.Partition_start, errMsg)) {
            return {false, "ERROR: No se pudo actualizar superbloque: " + errMsg};
        }

        return {true, "MKDIR: Carpeta '" + nombreCarpeta + "' creada (inodo " + to_string(nuevo) + ")"};
    }

} // namespace Comandos