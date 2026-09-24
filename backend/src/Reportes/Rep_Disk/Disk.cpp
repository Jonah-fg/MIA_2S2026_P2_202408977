#include "Disk.h"
#include "../../Estructuras/Str_Ebr/EBR.h"
#include <cstdlib>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <fstream>
#include "../../Utils/Utilities.h"
using namespace std;

namespace Reportes{
    namespace{
        string trimNulls(const char* data, size_t len) {
            string s(data, len);
            size_t nul =s.find('\0');
            if (nul != string::npos)
                s =s.substr(0, nul);

            return s;
        }
        string fmt2(double v){
            ostringstream oss;
            oss << fixed << setprecision(2) << v;
            return oss.str();
        }
    }

    bool ReporteDISK(const Estructuras::MBR& mbr, const string& path, const string& diskPath, string& errMsg) {
        if(!Utilities::CreateParentDir(path, errMsg)){
            return false;
        }

        string dotFileName, outputImage;
        Utilities::GetFileNames(path, dotFileName, outputImage);

        //sizeof(MBR) empaquetado
        const double mbrSize= 153.0;
        double totalSize =static_cast<double>(mbr.Mbr_size);
        double usableSize= totalSize - mbrSize;
        string diskName = std::filesystem::path(diskPath).filename().string();
        double mbrPct = (mbrSize / totalSize)  * 100.0;

        ostringstream dot;
        dot<< "digraph G {\n"
           << "\tlabelloc=\"t\";\n"
           << "\tlabel = \"Reporte de Disco: "<< diskName
           << " (Size: "<<static_cast<long long>(totalSize) <<" bytes)\";\n"
           << "\tnode [shape=plaintext];\n\n"
          << "\ttabla [label=<\n"
           << "\t<table border=\"1\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"10\" bgcolor=\"#F9F9F9\">\n"
           << "\t<tr><td rowspan=\"2\" bgcolor=\"#A95C68\" border=\"1\" color=\"black\"><b>MBR</b><br/>"
          << static_cast<long long>(mbrSize) <<" bytes<br/>"
           << fmt2(mbrPct) << "% del Disco</td>";

        string partitionRows;
        string logicalRows;
        bool extendedFound =false;
        double freeSpace = usableSize;

        //Recorrido particiones
        for (const auto& part : mbr.Mbr_partitions){
            if (part.Partition_size ==0){
                continue;
            }

            string partName =trimNulls(part.Partition_name, sizeof(part.Partition_name));
            char partType =part.Partition_type[0];
            double partSize= static_cast<double>(part.Partition_size);
            double partPct =(partSize / totalSize) * 100.0;
            freeSpace-=partSize;

            if (partType== 'E') {
                extendedFound =true;
                ostringstream row;
                row<< "\n\t\t<td colspan=\"20\" bgcolor=\"#F0E68C\" border=\"1\" color=\"black\"><b>EXTENDIDA<br/>" << fmt2(partPct) << "% del Disco</b></td>";
                partitionRows +=row.str();

                // Recorrido EBRs
                ifstream file(diskPath, ios::binary);
                if (!file.is_open()){
                    errMsg ="No se pudo abrir el disco";
                    return false;
                }
                Estructuras::EBR ebr;
                file.seekg(part.Partition_start, ios::beg);
                file.read(reinterpret_cast<char*>(&ebr), sizeof(Estructuras::EBR));

                if(file && ebr.Partition_size != 0) {
                    while(true){
                        string ebrName =trimNulls(ebr.Partition_name, sizeof(ebr.Partition_name));
                        double ebrSize= static_cast<double>(ebr.Partition_size);
                        double ebrPct=(ebrSize / totalSize) * 100.0;
                        ostringstream lrow;
                        lrow <<"\n\t\t<td bgcolor=\"#D27D2D\" border=\"1\" color=\"black\">EBR</td>\n" << "\t\t<td bgcolor=\"#C2B280\" border=\"1\" color=\"black\">Logica<br/>"
                          << ebrName <<"<br/>"<< fmt2(ebrPct) <<"% del Disco</td>";

                        logicalRows+= lrow.str();

                        if(ebr.Partition_next == -1) {
                            break;
                        }
                        file.seekg(ebr.Partition_next, ios::beg);
                        file.read(reinterpret_cast<char*>(&ebr), sizeof(Estructuras::EBR));
                        if (!file){
                           break; 
                        }
                    }
                }
                file.close();
            } 
            else{
                if (partType!= '0') {
                    ostringstream row;
                    row << "\n\t\t<td rowspan=\"2\" bgcolor=\"#DAA06D\" border=\"1\" color=\"black\"><b>" << partName << "</b><br/>" << static_cast<long long>(partSize)
                      << " bytes<br/>"<< fmt2(partPct) <<"% del Disco</td>";
                    partitionRows += row.str();
                }
            }
        }

        //Espacio libre
        if (freeSpace >0) {
            double freePct= (freeSpace / totalSize) * 100.0;
            ostringstream row;
            row << "\n\t\t<td rowspan=\"2\" bgcolor=\"#E0E0E0\" border=\"1\" color=\"black\">Espacio Libre<br/>" << fmt2(freePct) << "% del Disco</td>";
            partitionRows +=row.str();
        }

        if (!extendedFound){
            dot<< partitionRows <<"</tr></table>>]; }";
        } 
        else {
            dot<< partitionRows << "</tr><tr>"<< logicalRows<< "</tr></table>>]; }";
        }

        ofstream dotFile(dotFileName, ios::binary | ios::trunc);
        if (!dotFile.is_open()){
            errMsg ="No se pudo crear el archivo .dot";
            return false;
        }
        dotFile << dot.str();
        dotFile.close();

        string cmd="dot -Tpng \"" + dotFileName + "\" -o \""+outputImage + "\"";
        int ret =system(cmd.c_str());
        if (ret !=0) {
            errMsg = "Error al ejecutar Graphviz";
            return false;
        }
        errMsg.clear();
        return true;
    }
} 