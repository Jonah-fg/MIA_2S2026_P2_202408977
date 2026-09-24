#include "Mbr.h"
#include "../../Estructuras/Str_Ebr/EBR.h"
#include "../../Utils/Utilities.h"
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
using namespace std;

namespace Reportes{
    namespace{
        string trimNulls(const char* data, size_t len) {
            string s(data, len);
            size_t nul= s.find('\0');
            if (nul !=string::npos)
                s=s.substr(0, nul);
                 
            return s;
        }

        string formatUnixDate(float unixTime) {
            time_t t =static_cast<time_t>(unixTime);
            struct tm tmResult;
            localtime_r(&t, &tmResult);
            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmResult);
            return string(buf);
        }
    }

    bool ReporteMBR(const Estructuras::MBR& mbr, const string& path, const string& diskPath, string& errMsg) {
        if (!Utilities::CreateParentDir(path, errMsg)){
            return false;
        }
        string dotFileName, outputImage;
        Utilities::GetFileNames(path, dotFileName, outputImage);

        ostringstream dot;
        dot<< "digraph G {\n"
          << "\tnode [shape=plaintext]\n"
          << "\ttabla [label =<\n"
          << "\t<table border=\"0\" cellborder=\"1\" cellspacing=\"0\">\n"
          << "\t\t<tr><td colspan=\"2\" bgcolor=\"lightblue\"> REPORTE MBR </td></tr>\n"
          << "\t\t<tr><td bgcolor=\"lightblue1\"> MBR_SIZE </td><td> "<< mbr.Mbr_size << " </td></tr>\n"
          << "\t\t<tr><td bgcolor=\"lightblue1\"> MBR_DATE </td><td> " << formatUnixDate(mbr.Mbr_date) << " </td></tr>\n"
         << "\t\t<tr><td bgcolor=\"lightblue1\"> MBR_SIGNATURE </td><td> "<< mbr.Mbr_signature_disk<<" </td></tr>\n"
         << "\t\t<tr><td bgcolor=\"lightblue1\"> MBR_FIT </td><td> " <<mbr.Mbr_disk_fit[0]<< " </td></tr>\n";

        //4 particiones
        for (int i = 0; i <4; ++i){
            const Estructuras::PARTITION& part = mbr.Mbr_partitions[i];
            string partName = trimNulls(part.Partition_name, sizeof(part.Partition_name));
            char partType = part.Partition_type[0];

            dot<< "\n\t\t<tr><td colspan=\"2\" bgcolor=\"royalblue\"> PARTICION " << (i +1) <<" </td></tr>\n"
             << "\t\t<tr><td bgcolor=\"dodgerblue\"> STATUS </td><td> "<< part.Partition_status[0] << " </td></tr>\n"
             << "\t\t<tr><td bgcolor=\"dodgerblue\"> TYPE </td><td> " << partType << " </td></tr>\n"
             << "\t\t<tr><td bgcolor=\"dodgerblue\"> FIT </td><td> "<< part.Partition_fit[0]<< " </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"dodgerblue\"> START </td><td> " << part.Partition_start << " </td></tr>\n"
             << "\t\t<tr><td bgcolor=\"dodgerblue\"> SIZE </td><td> "<< part.Partition_size << " </td></tr>\n"
            << "\t\t<tr><td bgcolor=\"dodgerblue\"> NAME </td><td> " << partName << " </td></tr>\n";

            //Si es extendida se recorre la cadena de EBRs
            if (partType =='E'){
                ifstream file(diskPath, ios::binary);
                if (!file.is_open()){
                    errMsg = "No se pudo abrir el disco para leer EBRs";
                    return false;
                }
                Estructuras::EBR ebr;
                file.seekg(part.Partition_start, ios::beg);
                file.read(reinterpret_cast<char*>(&ebr), sizeof(Estructuras::EBR));

                if (file && ebr.Partition_size != 0) {
                    while(true){
                        string ebrName = trimNulls(ebr.Partition_name, sizeof(ebr.Partition_name));
                        dot << "\n\t\t<tr><td colspan=\"2\" bgcolor=\"forestgreen\"> PARTICION LOGICA </td></tr>\n"
                          << "\t\t<tr><td bgcolor=\"chartreuse2\"> MOUNT </td><td> " << ebr.Partition_mount[0] << " </td></tr>\n"
                          << "\t\t<tr><td bgcolor=\"chartreuse2\"> FIT </td><td> " << ebr.Partition_fit[0] << " </td></tr>\n"
                          << "\t\t<tr><td bgcolor=\"chartreuse2\"> START </td><td> " << ebr.Partition_start << " </td></tr>\n"
                          << "\t\t<tr><td bgcolor=\"chartreuse2\"> SIZE </td><td> " << ebr.Partition_size << " </td></tr>\n"
                          << "\t\t<tr><td bgcolor=\"chartreuse2\"> NEXT </td><td> " << ebr.Partition_next << " </td></tr>\n"
                         << "\t\t<tr><td bgcolor=\"chartreuse2\"> NAME </td><td> " << ebrName << " </td></tr>\n";

                        if (ebr.Partition_next== -1) 
                            break;

                        file.seekg(ebr.Partition_next, ios::beg);
                        file.read(reinterpret_cast<char*>(&ebr), sizeof(Estructuras::EBR));
                        if (!file){
                          break;  
                        }
                    }
                }
                file.close();
            }
        }
        dot << "\n\t</table>>] }\n";
        ofstream dotFile(dotFileName, ios::binary | ios::trunc);
        if (!dotFile.is_open()) {
            errMsg = "No se pudo crear el archivo .dot";
            return false;
        }
        dotFile << dot.str();
        dotFile.close();

        string cmd="dot -Tpng \"" + dotFileName + "\" -o \"" +outputImage +"\"";
        int ret = system(cmd.c_str());
        if (ret !=0) {
            errMsg = "Error al ejecuar Graphviz";
            return false;
        }
        errMsg.clear();
        return true;
    }

} 