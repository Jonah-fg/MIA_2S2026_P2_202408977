#include "Block.h"
#include "../../Estructuras/Str_Inode/INODE.h"
#include "../../Estructuras/Str_Folderblock/FOLDERBLOCK.h"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include "../../Estructuras/Str_Fileblock/FILEBLOCK.h"
#include "../../Utils/Utilities.h"
#include <vector>

using namespace std;
namespace Reportes{
    bool ReporteBLOCK(const Estructuras::SUPERBLOCK& sb, const string& path, const string& diskPath, string& errMsg){
        if (!Utilities::CreateParentDir(path, errMsg)){
            return false;
        }
        string dotFileName, outputImage;
        Utilities::GetFileNames(path, dotFileName, outputImage);

        //lectura de la tabla de inodos para saber qué bloques estn en uso y de qué tipo
        int totalInodos =sb.Sb_inodes_count + sb.Sb_free_inodes_count;
        vector<Estructuras::INODE>inodos(totalInodos);

        ifstream file(diskPath, ios::binary);
        if (!file.is_open()){
            errMsg = "No se pudo abrir el disco para leer inodos";
            return false;
        }
        file.seekg(sb.Sb_inode_start, ios::beg);
        file.read(reinterpret_cast<char*>(inodos.data()), totalInodos * sizeof(Estructuras::INODE));
        file.close();

        //recolecta los bloques usados con su tipo
        struct BloqueInfo{
            int num;
            char tipo; 
        };
        vector<BloqueInfo> bloques;

        for(int i =0; i < totalInodos; ++i) {
            if (inodos[i].I_type[0] == 0) 
                continue;

            char tipo =(inodos[i].I_type[0] =='0') ? 'C' : 'A';
            for (int j=0; j <12; ++j){
                if(inodos[i].I_block[j]> 0){
                    bloques.push_back({inodos[i].I_block[j], tipo});
                }
            }
        }
        if (bloques.empty()) {
            errMsg = "No hay bloques usados para reportar";
            return false;
        }

        ostringstream dot;
        dot << "digraph G {\n" << "\tlabelloc=\"t\";\n" << "\tlabel = \"Reporte de Bloques\";\n" << "\tnode [shape=plaintext];\n\n";

        for (const auto& b : bloques) {
            if (b.tipo =='C') {
                //Bloque de carpeta
                Estructuras::FOLDERBLOCK folderBlock;
                long long offset = sb.Sb_block_start + (b.num * sb.Sb_block_size);
                if (!folderBlock.Deserialize(diskPath, offset, errMsg))
                    continue;

                dot<< "\tbloque" << b.num << " [label=<\n" << "\t<table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"5\" bgcolor=\"#FFF4E6\">\n"
                    <<"\t<tr><td colspan=\"2\" bgcolor=\"#D97706\" color=\"white\"><b>BLOQUE " << b.num << " (CARPETA)</b></td></tr>\n";

                for (int i =0; i< 4; ++i){
                    string nombre(folderBlock.B_content[i].B_name);
                    nombre =nombre.c_str();
                    dot<<"\t<tr><td bgcolor=\"#FFD9A0\"> b_name </td><td> " << nombre << " </td></tr>\n"  << "\t<tr><td bgcolor=\"#FFD9A0\"> b_inodo </td><td> " << folderBlock.B_content[i].B_inodo << " </td></tr>\n";
                }
                dot << "\t</table>>];\n\n";
            } 
            else{
                //Bloque de archivo
                Estructuras::FILEBLOCK fileBlock;
                long long offset = sb.Sb_block_start +(b.num * sb.Sb_block_size);
                if (!fileBlock.Deserialize(diskPath, offset, errMsg)) {
                    continue;
                }

                string contenido(fileBlock.B_content, 64);

                size_t nul=contenido.find('\0');
                if (nul != string::npos) {
                    contenido = contenido.substr(0, nul);
                }
                dot<< "\tbloque" <<b.num << " [label=<\n" << "\t<table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"5\" bgcolor=\"#E8F5E9\">\n"
                 << "\t<tr><td bgcolor=\"#388E3C\" color=\"white\"><b>BLOQUE " <<b.num << " (ARCHIVO)</b></td></tr>\n"
                 << "\t<tr><td> " << contenido<< " </td></tr>\n"
                << "\t</table>>];\n\n";
            }
        }
        dot<<"}\n";

        ofstream dotFile(dotFileName, ios::binary | ios::trunc);
        if (!dotFile.is_open()){
            errMsg = "No se pudo crear el archivo .dot";
            return false;
        }
        dotFile << dot.str();
        dotFile.close();

        string cmd ="dot -Tpng \"" + dotFileName +"\" -o \"" + outputImage+ "\"";
        int ret = system(cmd.c_str());
        if (ret != 0){
            errMsg ="Error al ejectar Graphviz";
            return false;
        }
        errMsg.clear();
        return true;
    }
} 