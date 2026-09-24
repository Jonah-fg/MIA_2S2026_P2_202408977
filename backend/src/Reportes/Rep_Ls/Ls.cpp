#include "Ls.h"
#include "../../Estructuras/Str_Inode/INODE.h"
#include <cstdlib>
#include <ctime>
#include <fstream>
#include "../../Estructuras/Str_Folderblock/FOLDERBLOCK.h"
#include "../../Utils/Utilities.h"
#include <sstream>
#include <vector>
#include "../../Utils/Ext2Utils.h"

using namespace std;

namespace Reportes{
    namespace{
        string formatUnixDate(float unixTime){
            time_t t = static_cast<time_t>(unixTime);
            struct tm tmResult;
            localtime_r(&t, &tmResult);
            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmResult);
            return string(buf);
        }

        vector<string> splitPath(const string& p) {
            vector<string> partes;
            stringstream ss(p);
            string item;
            while (getline(ss, item, '/')) {
                if(!item.empty()) partes.push_back(item);
            }
            return partes;
        }
    }

    bool ReporteLS(const Estructuras::SUPERBLOCK& sb, const string& path, const string& diskPath, const string& pathFile, string& errMsg) {
        if(!Utilities::CreateParentDir(path, errMsg)) 
            return false;

        string dotFileName, outputImage;
        Utilities::GetFileNames(path, dotFileName, outputImage);

        //se Determinar el inodo de la carpeta a listar
        int inodoObjetivo =0;
        string rutaBusqueda =pathFile.empty() ?"/" : pathFile;

        vector<string> partes = splitPath(rutaBusqueda);
        Estructuras::INODE inode;
        if (!inode.Deserialize(diskPath, sb.Sb_inode_start + 0 * sb.Sb_inode_size, errMsg)) {
            return false;
        }

        for (const auto& comp : partes){
            int hijo =Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, comp, errMsg);
            if (hijo == -1) {
                errMsg ="Ruta no encontrada: " + comp;
                return false;
            }
            inodoObjetivo = hijo;
            if (!inode.Deserialize(diskPath, sb.Sb_inode_start + inodoObjetivo * sb.Sb_inode_size, errMsg)) {
                return false;
            }
        }
        // Verificacion inodo objetivo sea crpeta
        if (inode.I_type[0] !='0'){
            errMsg = "La ruta no es una carpeta";
            return false;
        }

        Estructuras::FOLDERBLOCK folderBlock;
        long long off =sb.Sb_block_start+ (inode.I_block[0] * sb.Sb_block_size);
        if (!folderBlock.Deserialize(diskPath, off, errMsg))
            return false;

        //construccion tabla
        ostringstream dot;
        dot<< "digraph G {\n" << "\tlabelloc=\"t\";\n"
          << "\tlabel = \"Reporte LS: " <<rutaBusqueda<< "\";\n"
          << "\tnode [shape=plaintext];\n\n"
          << "\ttabla [label=<\n"
          << "\t<table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"5\" bgcolor=\"#F5F5F5\">\n"
         << "\t<tr bgcolor=\"#2C3E50\" color=\"white\">\n"
          << "\t\t<td><b>Permisos</b></td>\n"
          << "\t\t<td><b>Owner</b></td>\n"
          << "\t\t<td><b>Grupo</b></td>\n"
          << "\t\t<td><b>Size (bytes)</b></td>\n"
          << "\t\t<td><b>Fecha</b></td>\n"
          << "\t\t<td><b>Hora</b></td>\n"
          << "\t\t<td><b>Tipo</b></td>\n"
         << "\t\t<td><b>Name</b></td>\n"
          << "\t</tr>\n";

        for (int b = 0; b < 12; ++b) {
            if (inode.I_block[b] == -1) continue;

            Estructuras::FOLDERBLOCK folderBlock;
            long long off = sb.Sb_block_start + (inode.I_block[b] * sb.Sb_block_size);
            if (!folderBlock.Deserialize(diskPath, off, errMsg)) continue;

            for (int i = 0; i < 4; ++i) {
                if (folderBlock.B_content[i].B_inodo == -1) continue;

                string nombre(folderBlock.B_content[i].B_name);
                nombre = nombre.c_str();
                if (nombre.empty() || nombre == "." || nombre == "..") continue;

                int numInodo = folderBlock.B_content[i].B_inodo;
                Estructuras::INODE child;
                if (!child.Deserialize(diskPath, sb.Sb_inode_start + numInodo * sb.Sb_inode_size, errMsg)) {
                    continue;
                }

                string tipo = (child.I_type[0] == '0') ? "Carpeta" : "Archivo";

                // Fecha y hora
                time_t t = static_cast<time_t>(child.I_mtime);
                struct tm tmResult;
                localtime_r(&t, &tmResult);
                char fecha[32], hora[32];
                strftime(fecha, sizeof(fecha), "%Y-%m-%d", &tmResult);
                strftime(hora, sizeof(hora), "%H:%M:%S", &tmResult);

                dot << "\t<tr>\n"
                   << "\t\t<td> " << child.I_perm[0] << child.I_perm[1]<<child.I_perm[2]<< " </td>\n"
                  << "\t\t<td> " << child.I_uid << " </td>\n"
                   << "\t\t<td> "<< child.I_gid<< " </td>\n"
                   << "\t\t<td> " << child.I_size<< " </td>\n"
                  << "\t\t<td> "<< fecha << " </td>\n"
                  << "\t\t<td> " << hora<< " </td>\n"
                  << "\t\t<td> " << tipo << " </td>\n"
                  << "\t\t<td> " << nombre << " </td>\n"
                  << "\t</tr>\n";
            }
        }

        dot << "\t</table>>] }\n";

        ofstream dotFile(dotFileName, ios::binary | ios::trunc);
        if (!dotFile.is_open()) {
            errMsg = "No se pudo crear el archivo .dot";
            return false;
        }
        dotFile << dot.str();
        dotFile.close();

        string cmd = "dot -Tpng \"" + dotFileName + "\" -o \"" + outputImage + "\"";
        int ret = system(cmd.c_str());
        if (ret != 0) {
            errMsg = "Error al ejeutar Graphviz";
            return false;
        }
        errMsg.clear();
        return true;
    }
}
