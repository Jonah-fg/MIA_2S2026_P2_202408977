#include "Sb.h"
#include "../../Utils/Utilities.h"
#include <cstdlib>
#include <ctime>
#include <fstream>
using namespace std;
#include <sstream>

namespace Reportes{
    namespace{
        string formatUnixDate(float unixTime) {
            time_t t =static_cast<time_t>(unixTime);
            struct tm tmResult;
            localtime_r(&t, &tmResult);
            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmResult);
            return string(buf);
        }
    }

    bool ReporteSB(const Estructuras::SUPERBLOCK& sb, const string& path, string& errMsg){
        if (!Utilities::CreateParentDir(path, errMsg)) {
            return false;
        }
        string dotFileName, outputImage;
        Utilities::GetFileNames(path, dotFileName, outputImage);
        ostringstream dot;
        dot<< "digraph G {\n" <<"\tnode [shape=plaintext]\n" << "\ttabla [label =<\n"
            << "\t<table border=\"0\" cellborder=\"1\" cellspacing=\"0\">\n"
            << "\t\t<tr><td colspan=\"2\" bgcolor=\"lightblue\"> REPORTE DE SUPERBLOQUE </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"lightblue1\"> sb_filesystem_type </td><td> "<< sb.Sb_filesystem_type << " </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"lightblue1\"> sb_inodes_count </td><td> " <<sb.Sb_inodes_count << " </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"lightblue1\"> sb_blocks_count </td><td> "<<sb.Sb_blocks_count << " </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"lightblue1\"> sb_free_blocks_count </td><td> "<<sb.Sb_free_blocks_count << " </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"lightblue1\"> sb_free_inodes_count </td><td> " << sb.Sb_free_inodes_count << " </td></tr>\n"
            <<"\t\t<tr><td bgcolor=\"lightblue1\"> sb_mtime </td><td> "<<formatUnixDate(sb.Sb_mtime) << " </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"lightblue1\"> sb_umtime </td><td> " << formatUnixDate(sb.Sb_umtime) << " </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"lightblue1\"> sb_mnt_count </td><td> "<< sb.Sb_mnt_count <<" </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"lightblue1\"> sb_magic </td><td> " <<sb.Sb_magic << " (0xEF53) </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"lightblue1\"> sb_inode_size </td><td> " << sb.Sb_inode_size << " </td></tr>\n"
            <<"\t\t<tr><td bgcolor=\"lightblue1\"> sb_block_size </td><td> " << sb.Sb_block_size << " </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"lightblue1\"> sb_first_ino </td><td> " << sb.Sb_first_ino << " </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"lightblue1\"> sb_first_blo </td><td> " << sb.Sb_first_blo<<" </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"lightblue1\"> sb_bm_inode_start </td><td> "<< sb.Sb_bm_inode_start << " </td></tr>\n"
            <<"\t\t<tr><td bgcolor=\"lightblue1\"> sb_bm_block_start </td><td> "<<sb.Sb_bm_block_start << " </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"lightblue1\"> sb_inode_start </td><td> "<<sb.Sb_inode_start << " </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"lightblue1\"> sb_block_start </td><td> "<< sb.Sb_block_start << " </td></tr>\n"
            <<"\t</table>>] }\n";

        ofstream dotFile(dotFileName, ios::binary | ios::trunc);
        if (!dotFile.is_open()){
            errMsg = "No se pudo crear el arhivo .dot";
            return false;
        }
        dotFile <<dot.str();
        dotFile.close();

        string cmd ="dot -Tpng \"" + dotFileName + "\" -o \"" +outputImage +"\"";
        int ret= system(cmd.c_str());
        if (ret!= 0){
            errMsg ="Error al ejecutar Graphviz";
            return false;
        }
        errMsg.clear();
        return true;
    }
}