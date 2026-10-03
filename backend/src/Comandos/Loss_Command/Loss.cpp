#include "Loss.h"
#include "../../Global/MountedPartitions.h"
#include "../../Estructuras/Str_Superblock/SUPERBLOCK.h"
#include "../../Utils/Ext2Utils.h"
#include "../../Utils/JournalUtils.h"
#include "../../Reportes/Rep_BmInode/BmInode.h"
#include "../../Reportes/Rep_BmBlock/BmBlock.h"
#include <regex>
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
using namespace std;
namespace Comandos{

    static string toLowerStr(string s){
        transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return tolower(c); });
        return s;
    }

    static string joinTokens(const vector<string>& tokens){
        string result;
        for (size_t i= 0; i<tokens.size(); ++i) {
            if (i >0){
                result+=" ";
            }
            result+=tokens[i];
        }
        return result;
    }

    //la sobreescritura con \0 una región del disco desde 'offset' durnte 'tam' bytes.
    static bool limpiarRegion(const string& diskPath, long long offset, long long tam, string& errMsg) {
        fstream archivo(diskPath, ios::binary | ios::in | ios::out);
        if (!archivo.is_open()){
            errMsg = "No se pudo abrir el disco para limpar";
            return false;
        }

        const long long bloque=1024 *1024; 
        vector<char> ceros(static_cast<size_t>(bloque), 0);
        long long restante= tam;

        archivo.seekp(offset, ios::beg);
        while (restante>0){
            long long escribir= (restante < bloque) ? restante : bloque;
            archivo.write(ceros.data(), escribir);
            if (!archivo){
                errMsg="Error escribindo ceros en el disco";
                return false;
            }
            restante -=escribir;
        }
        return true;
    }

    CommandResult Loss_Command(const vector<string>& tokens) {
        //Parseo -id
        string id;
        string atributos =joinTokens(tokens);
        static const regex lexic(R"(-id=[^\s]+)", regex::icase);

        auto begin = sregex_iterator(atributos.begin(), atributos.end(), lexic);
        auto end =sregex_iterator();
        for (auto it = begin; it != end; ++it){
            string fun = it->str();
            size_t eq=fun.find('=');
            if (eq== string::npos) 
                continue;
                
            string key= toLowerStr(fun.substr(0, eq));
            string value = fun.substr(eq+1);
            if (value.size() >= 2 && value.front() =='"' && value.back() == '"') {
                value =value.substr(1, value.size() - 2);
            }
            if(key =="-id"){
               id =value; 
            }
        }
        if (id.empty()){
            return {false, "ERROR: falta -id"};
        }

        //Obtencion particion y superbloque 
        Estructuras::PARTITION mountedPart;
        string diskPath;
        string errMsg;
        if (!Global::GetMountedPartition(id, mountedPart, diskPath, errMsg)){
            return {false, "ERROR: "+ errMsg};
        }

        Estructuras::SUPERBLOCK sb;
        if (!Ext2Utils::LeerSuperbloque(diskPath, mountedPart.Partition_start, sb, errMsg)) {
            return {false, "ERROR: No se pudo leer supebloque: " + errMsg};
        }

        //Verificacion EXT3 
        if (!JournalUtils::EsExt3(sb)){
            return {false, "ERROR: LOSS solo aplica a particiones EXT3 (id " +id + " es EXT2)"};
        }

        int32_t n =sb.Sb_inodes_count + sb.Sb_free_inodes_count;
        if (n<= 0){
            return {false, "ERROR: Tamano de inodos invalido"};
        }

        //Reportes ANTES
        ostringstream salida;
        salida << "\n=================LOSS EXT3 ============\n";
        salida << "Particion: " <<id << "  (n = " << n << ")\n\n";

        string rutaAntesInodo= "/tmp/loss_" +id + "_antes_bm_inode.txt";
        string rutaAntesBloque = "/tmp/loss_" + id + "_antes_bm_block.txt";

        string repErr;
        bool repOk1 =Reportes::ReporteBmInode(sb, rutaAntesInodo, diskPath, repErr);
        bool repOk2= Reportes::ReporteBmBlock(sb, rutaAntesBloque, diskPath, repErr);

        salida <<">> Reportes ANTES:\n";
        salida <<(repOk1 ? "  OK  " : "   ERR ") << rutaAntesInodo<< "\n";
        salida << (repOk2 ? "  OK  " : "  ERR ")<< rutaAntesBloque << "\n\n";

        salida << ">> Limpiando areas con \\0...\n";
        //Bitmap de inodos: n bytes
        if (!limpiarRegion(diskPath, sb.Sb_bm_inode_start, n, errMsg)){
            return {false, "ERROR bitmap inodos: "+errMsg};
        }
        salida << " OK  Bitmap inodos   (" <<n <<" bytes)\n";

        //Bitmap de bloques: 3n bytes
        if (!limpiarRegion(diskPath, sb.Sb_bm_block_start, 3LL *n, errMsg)) {
            return {false, "ERROR bimap bloques: " + errMsg};
        }
        salida << " OK  Bitmap bloques  (" << (3LL *n) <<" bytes)\n";

        //area de inodos: n * sizeof(INODE)
        long long tamInodos =static_cast<long long>(n)* sb.Sb_inode_size;
        if (!limpiarRegion(diskPath, sb.Sb_inode_start, tamInodos, errMsg)){
            return {false, "ERROR area inodos: "+errMsg};
        }
        salida << "  OK  Area inodos       (" << tamInodos << " bytes)\n";

        //area de blques: 3n * sizeof(FILEBLOCK)
        long long tamBloques = static_cast<long long>(3 * n) * sb.Sb_block_size;
        if(!limpiarRegion(diskPath, sb.Sb_block_start, tamBloques, errMsg)){
            return {false, "ERROR area bloques: "+errMsg};
        }
        salida << " OK  Area bloques   (" << tamBloques<< " bytes)\n\n";

        //reportes DESPUES posjdfofishjduoigfdh
        string rutaDespuesInodo  = "/tmp/loss_"+ id + "_despues_bm_inode.txt";
        string rutaDespuesBloque = "/tmp/loss_" + id+ "_despues_bm_block.txt";
        bool repOk3= Reportes::ReporteBmInode(sb, rutaDespuesInodo, diskPath, repErr);
        bool repOk4= Reportes::ReporteBmBlock(sb, rutaDespuesBloque, diskPath, repErr);

        salida << ">> Reportes DESPUES:\n";
        salida << (repOk3 ? "  OK  " : "  ERR ") << rutaDespuesInodo << "\n";
        salida << (repOk4 ? "  OK  " : "   ERR ") << rutaDespuesBloque <<"\n\n";

        salida <<"LOSS completado. La particion " << id<< " simula un sistema de archivos corrupto.\n";
        return {true, salida.str()};
    }

}