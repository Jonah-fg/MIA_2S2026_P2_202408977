#include "HttpServer.h"
#include "../../httplib.h"
#include "../Analyzer/Analyzer.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <streambuf>
#include <filesystem>
#include "../Estructuras/Str_Mbr/MBR.h"
#include "../Global/MountedPartitions.h"
#include "../Utils/Ext2Utils.h"
#include "../Estructuras/Str_Folderblock/FOLDERBLOCK.h"
#include "Json.h"
using namespace std;

namespace Server{
     //ejecuta un script línea por línea, capturando toda la salia
    static string ejecutarScript(const string& script) {
        stringstream salidaTotal;
        istringstream stream(script);
        string linea;
        while (getline(stream, linea)) {
            //ignorar líneas vacías
            if (linea.find_first_not_of(" \t\r\n") == string::npos)
                continue;

            string out=Analyzer::AnalyzeCapture(vector<string>{linea});
            salidaTotal << out;
        }
        return salidaTotal.str();
    }
    static httplib::Server* svr=nullptr;

    static void setCors(httplib::Response& res){
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    } 
    

    //devuelve un string JSON con array de {name, isFolder, inode, perm}
    static string listarCarpeta(const string& idParticion, const string& ruta, string& errMsg) {

        Estructuras::PARTITION mountedPart;
        string diskPath;
        if(!Global::GetMountedPartition(idParticion, mountedPart, diskPath, errMsg)) {
            return "";
        }

        //lectur superbloque
        Estructuras::SUPERBLOCK sb;
        if (!Ext2Utils::LeerSuperbloque(diskPath, mountedPart.Partition_start, sb, errMsg)){
            return "";
        }

        //Navegacion hasta la carpea indicada, creeo yo xd
        int inodoActual=0;
        Estructuras::INODE inode;
        if(!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)){
            return "";
        }

        if(ruta != "/" && !ruta.empty()){
            //division de la ruta por el / palito ese
            vector<string> partes;
            stringstream ss(ruta);
            string item;
            while(getline(ss, item, '/')){
                if (!item.empty()) partes.push_back(item);
            }
            for(const string& comp : partes) {
                int hijo = Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, comp, errMsg);
                if (hijo == -1) {
                    errMsg="Ruta no existe: " + comp;
                    return "";
                }
                inodoActual=hijo;
                if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) {
                    return "";
                }
            }
        }

        if (inode.I_type[0]!= '0'){
            errMsg = "La ruta no es una careta";
            return "";
        }

        //armado JSON con los hijos
        ostringstream salida;
        salida <<"{\"items\":[";

        bool primera=true;
        for (int b =0; b<12; ++b){
            if (inode.I_block[b]==-1) 
                continue;

            Estructuras::FOLDERBLOCK fb;
            long long off = sb.Sb_block_start+ (inode.I_block[b] * sb.Sb_block_size);
            if (!fb.Deserialize(diskPath, off, errMsg)) 
                continue;

            for (int i =0; i<4; ++i) {
                string nombre(fb.B_content[i].B_name);
                nombre =nombre.c_str();
                if(nombre.empty() || nombre == "." || nombre == ".." || fb.B_content[i].B_inodo == -1) {
                    continue;
                }

                Estructuras::INODE hijo;
                if (!Ext2Utils::LeerInodo(diskPath, sb, fb.B_content[i].B_inodo, hijo, errMsg)){
                    continue;
                }

                string permiso;
                permiso.push_back(hijo.I_perm[0]);
                permiso.push_back(hijo.I_perm[1]);
                permiso.push_back(hijo.I_perm[2]);
                if (!primera)
                    salida << ",";

                primera=false;

                salida<<"{";
                salida << "\"name\":\""    <<Json::escapar(nombre) << "\",";
                salida << "\"isFolder\":"   <<(hijo.I_type[0] =='0' ? "true" : "false") << ",";
                salida <<"\"inode\":"      <<fb.B_content[i].B_inodo <<",";
                salida<<"\"size\":"       <<hijo.I_size << ",";
                salida<< "\"perm\":\""     << permiso <<"\"";
                salida << "}";
            }
        }
        salida << "]}";
        return salida.str();
    }

    void iniciarServidor(){
        svr =new httplib::Server();

        //Ruta POST /execute: recibe un comando o script y devuelve la salida
        svr->Post("/api/execute", [](const httplib::Request& req, httplib::Response& res) {
           setCors(res);

            string comando=req.body;
            string salida =ejecutarScript(comando);
            bool success= salida.find("[ERROR]") ==string::npos && salida.find("ERROR:")== string::npos;

            string body =Json::object({{"success", Json::boolean(success), true}, {"message", salida, false}, {"output",  salida, false}});
            res.set_content(body, "application/json");
        });

        svr->Options("/api/.*", [](const httplib::Request&, httplib::Response& res) {
            setCors(res);
            res.status =204;
        });

        //GET/api/health
        svr->Get("/api/health", [](const httplib::Request&, httplib::Response& res){
            setCors(res);
            res.set_content("{\"status\":\"ok\"}", "application/json");
        });
        cout <<"Servidor HTTP escuchando en http://0.0.0.0:8080"<< endl;
        cout << "Endpoints: POST /api/execute | GET /api/health"<< endl;

    //GET /api/disks 
        svr->Get("/api/disks", [](const httplib::Request&, httplib::Response& res){
            setCors(res);
            namespace fs= std::filesystem;

            vector<string> discos;
            vector<string> carpetas={"/tmp", "."};

            for (const auto& carpeta : carpetas){
                error_code ec;
                if (!fs::exists(carpeta, ec)){
                   continue; 
                }
                for(const auto& entry : fs::directory_iterator(carpeta, ec)){
                    if(!entry.is_regular_file())
                        continue;

                    string path= entry.path().string();
                    if (path.size()>4 && path.substr(path.size() - 4) ==".mia"){
                        discos.push_back(path);
                    }
                }
            }
            sort(discos.begin(), discos.end());
            ostringstream salida;
            salida<< "{\"discos\":[";
            for (size_t i= 0; i < discos.size(); ++i) {
                if (i > 0){
                   salida << ","; 
                }
                salida << "\"" <<Json::escapar(discos[i]) << "\"";
            }
            salida<<"]}";
            res.set_content(salida.str(), "application/json");
        });

        //GET /api/disks/partitions?path=<ruta> 
        svr->Get("/api/disks/partitions", [](const httplib::Request& req, httplib::Response& res) {
            setCors(res);

            if (!req.has_param("path")) {
                res.status=400;
                res.set_content("{\"error\":\"falta paraetro path\"}", "application/json");
                return;
            }

            string diskPath= req.get_param_value("path");
            Estructuras::MBR mbr;
            string errMsg;
            if(!mbr.DeserializeMBR(diskPath, errMsg)){
                res.status = 500;
                res.set_content("{\"error\":\""+Json::escapar(errMsg)+ "\"}", "application/json");
                return;
            }

            //armado array JSON de particiones con sus campos
            ostringstream salida;
            salida <<"{\"partitions\":[";
            bool primera=true;
            for (int i =0; i< 4; ++i) {
                const Estructuras::PARTITION& p = mbr.Mbr_partitions[i];
                if (p.Partition_status[0] =='2') 
                    continue; 

                //convesion nombre y id (char[]) a strng
                string nombre(p.Partition_name, sizeof(p.Partition_name));
                size_t nul= nombre.find('\0');
                if (nul!=string::npos){
                    nombre =nombre.substr(0, nul);
                }

                string pid(p.Partition_id, sizeof(p.Partition_id));
                nul =pid.find('\0');
                if (nul != string::npos) {
                    pid = pid.substr(0, nul);
                }

                if(!primera) 
                    salida << ",";

                primera=false;

                salida<< "{";
                salida <<"\"name\":\""        << Json::escapar(nombre)                      << "\",";
                salida << "\"type\":\""        << p.Partition_type[0]                        <<"\",";
                salida <<"\"fit\":\""         << p.Partition_fit[0]                         << "\",";
                salida << "\"status\":\""     <<p.Partition_status[0]                      <<"\",";
                salida <<"\"start\":"        << p.Partition_start                          << ",";
                salida << "\"size\":"         << p.Partition_size                           <<",";
                salida<< "\"id\":\""          << Json::escapar(pid)                         << "\"";
                salida << "}";
            }
            salida<< "]}";
            res.set_content(salida.str(), "application/json");
        }); 
        
        svr->listen("0.0.0.0", 8080);
    }

    void detenerServidor() {
        if(svr){
            svr->stop();
            delete svr;
            svr = nullptr;
        }
    }
}