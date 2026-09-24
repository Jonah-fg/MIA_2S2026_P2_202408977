#include "BmBlock.h"
#include "../../Utils/Utilities.h"
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;

namespace Reportes {
    bool ReporteBmBlock(const Estructuras::SUPERBLOCK& sb, const string& path, const string& diskPath, string& errMsg) {
        if (!Utilities::CreateParentDir(path, errMsg)) {
            return false;
        }

        //lectura del bitmap de bloques desede el disco
        int totalBloques = sb.Sb_blocks_count + sb.Sb_free_blocks_count;
        vector<char> bitmap(totalBloques);

        ifstream file(diskPath, ios::binary);
        if (!file.is_open()) {
            errMsg = "No se pudo abrir el disco para leer bitmap de bloques";
            return false;
        }
        file.seekg(sb.Sb_bm_block_start, ios::beg);
        file.read(bitmap.data(), totalBloques);
        if (!file){
            errMsg = "Error al leer bitmap de bloques";
            return false;
        }
        file.close();

        ofstream out(path);
        if (!out.is_open()) {
            errMsg = "No se pudo crear el archivo de reporte";
            return false;
        }

        out << "------------------ BITMAP DE BLOQUES =--------\n";
        out << "Total de bloques: "<< totalBloques << "\n\n";

        for (int i = 0; i<totalBloques; ++i) {
            out << bitmap[i];
            if ((i + 1) % 20 ==0) {
                out<< "\n";
            }
        }
        if (totalBloques % 20 != 0) {
            out<< "\n";
        }
        out.close();
        errMsg.clear();
        return true;
    }
} 