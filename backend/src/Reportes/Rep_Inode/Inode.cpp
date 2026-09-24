#include "../../Estructuras/Str_Inode/INODE.h"
#include "../../Utils/Utilities.h"
#include <cstdlib>
#include "Inode.h"
#include <ctime>
#include <sstream>
#include <vector>
#include <fstream>

using namespace std;
namespace Reportes {
    namespace{
        string formatUnixDate(float unixTime) {
            time_t t= static_cast<time_t>(unixTime);
            struct tm tmResult;
            localtime_r(&t, &tmResult);
            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmResult);
            return string(buf);
        }
    }

    bool ReporteINODE(const Estructuras::SUPERBLOCK& sb, const string& path, const string& diskPath, string& errMsg) {
        if (!Utilities::CreateParentDir(path, errMsg)){
            return false;
        }

        string dotFileName, outputImage;
        Utilities::GetFileNames(path, dotFileName, outputImage);

        //Total de inodos
        int totalInodos =sb.Sb_inodes_count + sb.Sb_free_inodes_count;
        vector<Estructuras::INODE> inodos(totalInodos);

        ifstream file(diskPath, ios::binary);
        if (!file.is_open()) {
            errMsg ="No se pudo abrir el disco para leer inodos";
            return false;
        }
        file.seekg(sb.Sb_inode_start, ios::beg);
        file.read(reinterpret_cast<char*>(inodos.data()), totalInodos * sizeof(Estructuras::INODE));
        if (!file) {
            errMsg="Error al leer tabla de inodos";
            return false;
        }
        file.close();

        ostringstream dot;
        dot <<"digraph G{\n"
            << "\tlabelloc=\"t\";\n"
          << "\tlabel = \"Reporte de Inodos\";\n"
           <<"\tnode [shape=plaintext];\n\n";

        int usados =0;
        for (int i=0; i<totalInodos; ++i){
            const auto& inode = inodos[i];
            if (inode.I_type[0] == 0){
                continue;
            } 
            usados++;
            string tipo= (inode.I_type[0] == '0')?"Carpeta" : "Archivo";

            dot<< "\tinodo" << i << " [label=<\n"
              << "\t<table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"5\" bgcolor=\"#E8F4F8\">\n"
              << "\t<tr><td colspan=\"2\" bgcolor=\"#3A7CA5\" color=\"white\"><b>INODO "<<i<< "</b></td></tr>\n"
              << "\t<tr><td bgcolor=\"#A8D5E2\"> i_uid </td><td> " << inode.I_uid << " </td></tr>\n"
              << "\t<tr><td bgcolor=\"#A8D5E2\"> i_gid </td><td> "<< inode.I_gid << " </td></tr>\n"
              << "\t<tr><td bgcolor=\"#A8D5E2\"> i_size </td><td> " << inode.I_size << " </td></tr>\n"
               << "\t<tr><td bgcolor=\"#A8D5E2\"> i_atime </td><td> " << formatUnixDate(inode.I_atime) << " </td></tr>\n"
              << "\t<tr><td bgcolor=\"#A8D5E2\"> i_ctime </td><td> "<< formatUnixDate(inode.I_ctime) << " </td></tr>\n"
              << "\t<tr><td bgcolor=\"#A8D5E2\"> i_mtime </td><td> " << formatUnixDate(inode.I_mtime) << " </td></tr>\n";

            //en una fila los 15 apuntadores
            dot << "\t<tr><td bgcolor=\"#A8D5E2\"> i_block </td><td>";
            for (int j=0; j < 15; ++j){
                dot <<inode.I_block[j];
                if (j <14)
                    dot << ", ";
            }
            dot<< "</td></tr>\n";

            dot<< "\t<tr><td bgcolor=\"#A8D5E2\"> i_type </td><td> " << tipo << " </td></tr>\n" << "\t<tr><td bgcolor=\"#A8D5E2\"> i_perm </td><td> " << inode.I_perm[0] << inode.I_perm[1] << inode.I_perm[2] << " </td></tr>\n"
             << "\t</table>>];\n\n";
        }
        dot<<"}\n";
        if (usados ==0){
            errMsg ="No hay inodos usados para reportar";
            return false;
        }

        ofstream dotFile(dotFileName, ios::binary | ios::trunc);
        if (!dotFile.is_open()){
            errMsg ="No se pudo crear el archivo .dot";
            return false;
        }
        dotFile<< dot.str();
        dotFile.close();

        string cmd= "dot -Tpng \"" + dotFileName +"\" -o \"" + outputImage+ "\"";
        int ret= system(cmd.c_str());
        if (ret !=0){
            errMsg ="Error al ejecutar Graphviz";
            return false;
        }
        errMsg.clear();
        return true;
    }
} 