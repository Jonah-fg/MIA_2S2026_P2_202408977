#include "Rep.h"
#include "../../Global/MountedPartitions.h"
#include "../../Estructuras/Str_Mbr/MBR.h"
#include "../../Estructuras/Str_Superblock/SUPERBLOCK.h"
#include "../../Reportes/Rep_BmInode/BmInode.h"
#include "../../Reportes/Rep_BmBlock/BmBlock.h"
#include "../../Reportes/Rep_Mbr/Mbr.h"
#include "../../Reportes/Rep_Disk/Disk.h"
#include "../../Reportes/Rep_Inode/Inode.h"
#include "../../Reportes/Rep_Block/Block.h"
#include "../../Reportes/Rep_Ls/Ls.h"
#include "../../Reportes/Rep_Tree/Tree.h"
#include "../../Reportes/Rep_Sb/Sb.h"
#include "../../Reportes/Rep_File/File.h"
#include <regex>
#include <sstream>
using namespace std;

namespace Comandos {
    static string toLowerStr(string s){
        transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return tolower(c); });
        return s;
    }

    static string joinTokens(const vector<string>& tokens) {
        string result;
        for (size_t i= 0; i< tokens.size(); ++i) {
            if (i>0){
                result +=" ";
            }
            result+= tokens[i];
        }
        return result;
    }

    static bool contains(const vector<string>& list, const string& value){
        return find(list.begin(), list.end(), value) != list.end();
    }

    //Ejecucion del reporte correspondiente
    static bool Get_type_report(const REP& reporte, string& errMsg) {
        Estructuras::MBR mbr;
        string diskPath;
        if (!Global::GetEssentialRep(reporte.Id, mbr, diskPath, errMsg)) {
            return false;
        }

        //obtencio la partición montada
        Estructuras::PARTITION mountedPart;
        string partPath;
        if (!Global::GetMountedPartition(reporte.Id, mountedPart, partPath, errMsg)) {
            return false;
        }

        //lectura superbloque
        Estructuras::SUPERBLOCK sb;
        if (!sb.Deserialize(diskPath, mountedPart.Partition_start, errMsg)){
            return false;
        }
        string repErr;
        bool bien=true;

        if(reporte.Name =="bm_inode") {
            bien= Reportes::ReporteBmInode(sb, reporte.Path, diskPath, repErr);
        } 
        else if (reporte.Name == "bm_block" || reporte.Name =="bm_bloc") {
            bien= Reportes::ReporteBmBlock(sb, reporte.Path, diskPath, repErr);
        } 
        else if (reporte.Name== "sb") {
            bien =Reportes::ReporteSB(sb, reporte.Path, repErr);
        } 
        else if(reporte.Name== "file") {
            bien=Reportes::ReporteFile(sb, reporte.Path, diskPath, reporte.Path_file, repErr);
        }
        else if (reporte.Name== "mbr") {
            bien=Reportes::ReporteMBR(mbr, reporte.Path, diskPath, repErr);
        } 
        else if (reporte.Name== "disk") {
            bien=Reportes::ReporteDISK(mbr, reporte.Path, diskPath, repErr);
        } 
        else if(reporte.Name== "inode") {
            bien=Reportes::ReporteINODE(sb, reporte.Path, diskPath, repErr);
        } 
        else if (reporte.Name== "block") {
            bien=Reportes::ReporteBLOCK(sb, reporte.Path, diskPath, repErr);
        } 
        else if (reporte.Name== "ls") {
            bien=Reportes::ReporteLS(sb, reporte.Path, diskPath, reporte.Path_file, repErr);
        } 
        else if (reporte.Name== "tree"){
            bien=Reportes::ReporteTREE(sb, reporte.Path, diskPath, repErr);
        } 
        else{
            errMsg = "Reporte no implementado aún: " + reporte.Name;
            return false;
        }

        if (!bien) {
            errMsg= repErr;
            return false;
        }
        return true;
    }

    CommandResult Rep_Command(const vector<string>& tokens) {
        REP reporte;
        string atributos =joinTokens(tokens);
        static const regex lexic(R"(-id=[^\s]+|-path="[^"]+"|-path=[^\s]+|-name=[^\s]+|-path_file_ls="[^"]+"|-path_file_ls=[^\s]+)", regex::icase);

        vector<string> found;
        auto begin= sregex_iterator(atributos.begin(), atributos.end(), lexic);
        auto end =sregex_iterator();
        for (auto it= begin; it != end; ++it)
            found.push_back(it->str());

        if (found.size()!=tokens.size()){
            for (const auto& token : tokens) {
                if (!regex_search(token, lexic)){
                    return {false, "ERROR: Parámetro no reconocido: " +token+ " en REP"};
                }
            }
        }

        static const vector<string> validNames ={"mbr", "disk", "inode", "block","bm_inode", "bm_block",
        "bm_bloc","sb", "file", "ls","tree"};

        for (const auto& fun : found){
            size_t eqPos =fun.find('=');
            if (eqPos== string::npos) {
                return{false, "ERROR: formato inválido: " + fun};
            }
            string key= toLowerStr(fun.substr(0, eqPos));
            string value= fun.substr(eqPos + 1);
            if (value.size() >=2 && value.front() == '"' && value.back() == '"'){
                value = value.substr(1, value.size() - 2);
            }

            if (key =="-name") {
                if(!contains(validNames, value)) {
                    return {false, "ERROR: nombre de reporte inválido: " +value};
                }
                reporte.Name = value;
            } 
            else if (key == "-id") {
                if (value.empty()){
                    return {false, "ERROR: id vacío"};
                }
                reporte.Id= value;
            } 
            else if (key =="-path") {
                if (value.empty()){
                    return {false, "ERROR: path inválido, es de caracter oblgatorio"};
                }
                // Quitar comillas sobrantes (bug del script del auxiliar)
                while (!value.empty() && value.back() == '"'){
                value.pop_back();
                }
                reporte.Path = value;
            }

            else if(key == "-path_file_ls") {
                reporte.Path_file= value;
            } 
            else{
                return {false, "ERROR: parámetro desconocido: " + key};
            }
        }

        if (reporte.Name.empty() || reporte.Id.empty() || reporte.Path.empty()) {
            return {false, "ERROR: Faltan paámetros obligatorios (-name, -id, -path)"};
        }
        string errMsg;
        if (!Get_type_report(reporte, errMsg)) {
            return {false, errMsg};
        }
        return {true, "REP: Reporte '" +reporte.Name + "' generado en "+ reporte.Path};
    }

} 

