#include "Tree.h"
#include "../../Estructuras/Str_Inode/INODE.h"
#include "../../Estructuras/Str_Folderblock/FOLDERBLOCK.h"
#include "../../Estructuras/Str_Fileblock/FILEBLOCK.h"
#include "../../Utils/Utilities.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;
namespace Reportes{

    bool ReporteTREE(const Estructuras::SUPERBLOCK& sb, const string& path,const string& diskPath, string& errMsg) {
        if (!Utilities::CreateParentDir(path, errMsg)) return false;

        string dotFileName, outputImage;
        Utilities::GetFileNames(path, dotFileName, outputImage);

        int totalInodos = sb.Sb_inodes_count + sb.Sb_free_inodes_count;
        vector<Estructuras::INODE> inodos(totalInodos);

        ifstream file(diskPath, ios::binary);
        if (!file.is_open()) {
            errMsg = "No se pudo abrir el disco para leer inodos";
            return false;
        }
        file.seekg(sb.Sb_inode_start, ios::beg);
        file.read(reinterpret_cast<char*>(inodos.data()), totalInodos * sizeof(Estructuras::INODE));
        file.close();

        ostringstream dot;
        dot<< "digraph G {\n"<< "\tlabelloc=\"t\";\n" << "\tlabel = \"Reporte Tree\";\n"<< "\tnode [shape=plaintext];\n"
            << "\trankdir=LR;\n\n";

        //Creacion nodos de inodos 
        for (int i =0; i< totalInodos; ++i) {
            if (inodos[i].I_type[0]==0)
                 continue;

            string tipo = (inodos[i].I_type[0] == '0') ? "Carpeta" : "Archivo";
            dot << "\tinodo" << i << " [label=<\n"
                << "\t<table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"5\" bgcolor=\"#E3F2FD\">\n"
                << "\t<tr><td colspan=\"2\" bgcolor=\"#1976D2\" color=\"white\"><b>INODO " <<i<< " (" <<tipo << ")</b></td></tr>\n"
                << "\t<tr><td bgcolor=\"#BBDEFB\"> i_uid </td><td> "<< inodos[i].I_uid << " </td></tr>\n"
                << "\t<tr><td bgcolor=\"#BBDEFB\"> i_gid </td><td> "<< inodos[i].I_gid<< " </td></tr>\n"
                << "\t<tr><td bgcolor=\"#BBDEFB\"> i_size </td><td> " << inodos[i].I_size << " </td></tr>\n";

            dot<< "\t<tr><td bgcolor=\"#BBDEFB\"> i_block </td><td>";
            for (int j=0; j < 15; ++j){
                dot << inodos[i].I_block[j];
                if (j < 14) {
                    dot <<", ";
                }
            }
            dot << "</td></tr>\n";
            dot << "\t<tr><td bgcolor=\"#BBDEFB\"> i_type </td><td> " << inodos[i].I_type[0] << " </td></tr>\n"
                << "\t<tr><td bgcolor=\"#BBDEFB\"> i_perm </td><td> "
                << inodos[i].I_perm[0] << inodos[i].I_perm[1] << inodos[i].I_perm[2]<< "</td></tr>\n"
                << "\t</table>>];\n\n";
        }

        //Creacion nodos de bloques usados
        for (int i = 0; i < totalInodos; ++i) {
            if (inodos[i].I_type[0] == 0) {
                continue;
            }
 
            for (int j = 0; j<12; ++j) {
                int bnum= inodos[i].I_block[j];
                if (bnum<= 0) 
                    continue;

                if (inodos[i].I_type[0]== '0') {
                    Estructuras::FOLDERBLOCK fb;
                    long long off = sb.Sb_block_start +(bnum * sb.Sb_block_size);
                    if (!fb.Deserialize(diskPath, off, errMsg)) {
                        continue;
                    }

                    dot<< "\tbloque" << bnum << " [label=<\n" << "\t<table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"5\" bgcolor=\"#FFF8E1\">\n"
                        <<"\t<tr><td colspan=\"2\" bgcolor=\"#F57C00\" color=\"white\"><b>BLOQUE " <<bnum << " (CARPETA)</b></td></tr>\n";

                    for (int k =0; k<4; ++k) {
                        string nombre(fb.B_content[k].B_name);
                        nombre = nombre.c_str();
                        dot<< "\t<tr><td bgcolor=\"#FFE0B2\"> b_name </td><td> " << nombre << " </td></tr>\n" << "\t<tr><td bgcolor=\"#FFE0B2\"> b_inodo </td><td> " << fb.B_content[k].B_inodo << " </td></tr>\n";
                    }
                    dot << "\t</table>>];\n\n";
                }
                 else {
                    //bloque de archivo
                    Estructuras::FILEBLOCK fb;
                    long long off = sb.Sb_block_start + (bnum * sb.Sb_block_size);
                    if (!fb.Deserialize(diskPath, off, errMsg)) {
                       continue; 
                    }

                    string contenido(fb.B_content, 64);
                    size_t nul=contenido.find('\0');
                    if (nul != string::npos){
                       contenido = contenido.substr(0, nul); 
                    }
                    dot<< "\tbloque" <<bnum<< " [label=<\n"
                        << "\t<table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"5\" bgcolor=\"#E8F5E9\">\n"
                      << "\t<tr><td bgcolor=\"#388E3C\" color=\"white\"><b>BLOQUE " << bnum << " (ARCHIVO)</b></td></tr>\n"
                        << "\t<tr><td> " << contenido << "</td></tr>\n"
                        << "\t</table>>];\n\n";
                }
            }
        }
        //Creacion flechas inodo ->bloque
        for (int i = 0; i<totalInodos; ++i){
            if (inodos[i].I_type[0] ==0) 
                continue;

            for (int j= 0; j <12; ++j){
                int bnum = inodos[i].I_block[j];
                if (bnum <= 0){
                    continue;
                }
                dot <<"\tinodo" << i<< " -> bloque" <<bnum<< " [label=\"b[" <<j <<"]\"];\n";
            }
        }
        dot << "}\n";
        ofstream dotFile(dotFileName, ios::binary | ios::trunc);
        if (!dotFile.is_open()){
            errMsg = "No se pudo crer el archivo .dot";
            return false;
        }
        dotFile <<dot.str();
        dotFile.close();

        string cmd= "dot -Tpng \"" + dotFileName + "\" -o \""+ outputImage + "\"";
        int ret = system(cmd.c_str());
        if (ret !=0) {
            errMsg="Error al ejecutar Graphviz";
            return false;
        }
        errMsg.clear();
        return true;
    }
} 