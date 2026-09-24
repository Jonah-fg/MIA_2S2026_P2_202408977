#include "BmInode.h"
#include "../../Utils/Utilities.h"
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;
namespace Reportes{

    bool ReporteBmInode(const Estructuras::SUPERBLOCK& sb, const string& path, const string& diskPath, string& errMsg) {
        if (!Utilities::CreateParentDir(path, errMsg)) {
            return false;
        }

        //Lectura del bitmap de inodos desde el disco
        int totalInodos = sb.Sb_inodes_count +sb.Sb_free_inodes_count;
        vector<char> bitmap(totalInodos);

        ifstream file(diskPath, ios::binary);
        if (!file.is_open()){
            errMsg = "No se pudo abrir el dsco para leer bitmap de inodos";
            return false;
        }
        file.seekg(sb.Sb_bm_inode_start, ios::beg);
        file.read(bitmap.data(), totalInodos);
        if (!file) {
            errMsg ="Error al leer bitmap de inodos";
            return false;
        }
        file.close();

        ofstream out(path);
        if (!out.is_open()){
            errMsg ="No se pudo crear el archivo de reporte";
            return false;
        }

        out << "------- BITMAP DE INODOS ------------------\n";
        out << "Total de inodos: "<< totalInodos<< "\n\n";

        for (int i =0; i<totalInodos; ++i) {
            out << bitmap[i];
            if ((i + 1) % 20 == 0) {
                out << "\n";
            }
        }
        if (totalInodos % 20 != 0) {
            out << "\n";
        }

        out.close();
        errMsg.clear();
        return true;
    }

} // namespace Reports