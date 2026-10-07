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
#include "../Estructuras/Str_Superblock/SUPERBLOCK.h"
#include "../Estructuras/Str_Journal/JOURNAL.h"
#include "../Utils/JournalUtils.h"
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
        salida<<"{\"items\":[";

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

    //Lectura del contenido de un archivo de texto en una paricion montada
    static string leerArchivoTexto(const string& idParticion, const string& ruta, string& errMsg) {
        Estructuras::PARTITION mountedPart;
        string diskPath;
        if (!Global::GetMountedPartition(idParticion, mountedPart, diskPath, errMsg)) {
            return "";
        }

        Estructuras::SUPERBLOCK sb;
        if (!Ext2Utils::LeerSuperbloque(diskPath, mountedPart.Partition_start, sb, errMsg)) {
            return "";
        }

        int inodoActual= 0;
        Estructuras::INODE inode;
        if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) {
            return "";
        }

        vector<string> partes;
        stringstream ss(ruta);
        string item;
        while (getline(ss, item, '/')) {
            if (!item.empty()) partes.push_back(item);
        }

        for (const string& comp : partes){
            int hijo=Ext2Utils::BuscarHijoEnCarpeta(diskPath, sb, inode, comp, errMsg);
            if (hijo == -1) {
                errMsg = "No existe: "+ comp;
                return "";
            }
            inodoActual = hijo;
            if (!Ext2Utils::LeerInodo(diskPath, sb, inodoActual, inode, errMsg)) {
                return "";
            }
        }
        if (inode.I_type[0]!='1') {
            errMsg = "No es un archivo";
            return "";
        }

        string contenido;
        if (!Ext2Utils::LeerArchivo(diskPath, sb, inode, contenido, errMsg)) {
            return "";
        }

        // JSON
        ostringstream salida;
        salida << "{\"content\":\""<<Json::escapar(contenido) << "\"}";
        return salida.str();
    }



    //evuelve JSON: {"entries": [{no, operation, path, content, date}, ...]}
    static string leerJournal(const string& idParticion, string& errMsg) {
        Estructuras::PARTITION mountedPart;
        string diskPath;
        if(!Global::GetMountedPartition(idParticion, mountedPart, diskPath, errMsg)) {
            return "";
        }

        Estructuras::SUPERBLOCK sb;
        if (!Ext2Utils::LeerSuperbloque(diskPath, mountedPart.Partition_start, sb, errMsg)) {
            return "";
        }

        if (!JournalUtils::EsExt3(sb)){
            errMsg ="La particion no es EXT3 (no tiene journaling)";
            return "";
        }
        if (sb.Sb_journal_count <= 0 || sb.Sb_journal_start <= 0) {
            errMsg ="La particion no tiene journaling inicializado";
            return "";
        }

        ifstream archivo(diskPath, ios::binary);
        if (!archivo.is_open()) {
            errMsg="No se pudo abir el disco";
            return "";
        }

        ostringstream salida;
        salida << "{\"entries\":[";
        bool primera= true;
        long long offset = sb.Sb_journal_start;

        for (int32_t i = 0; i<sb.Sb_journal_count; ++i) {
            Estructuras::JOURNAL entrada;
            archivo.seekg(offset, ios::beg);
            archivo.read(reinterpret_cast<char*>(&entrada), sizeof(Estructuras::JOURNAL));
            if (!archivo){
                break;
            }

            if (entrada.j_count !=0){
                // Limpiar campos char[]
                auto limpiar =[](const char* data, size_t maxLen){
                    string s(data, maxLen);
                    size_t nul=s.find('\0');
                    if (nul != string::npos) s = s.substr(0, nul);
                    while (!s.empty() && (s.back()== ' ' || s.back() == '\r' || s.back() =='\n' || s.back() == '\t')) {
                        s.pop_back();
                    }
                    return s;
                };

                string op = limpiar(entrada.j_content.i_operation, 10);
                string pth =limpiar(entrada.j_content.i_path, 32);
                string con = limpiar(entrada.j_content.i_content, 64);
                float fecha =entrada.j_content.i_date;

                if (!primera) {
                    salida << ",";
                }
                primera=false;

                salida<< "{";
                salida << "\"no\":"        << entrada.j_count              << ",";
                salida << "\"operation\":\"" << Json::escapar(op)           << "\",";
                salida << "\"path\":\""     << Json::escapar(pth)             << "\",";
                salida<< "\"content\":\""  << Json::escapar(con)             << "\",";
                salida << "\"date\":"     <<fecha;
                salida << "}";
            }
            offset += sizeof(Estructuras::JOURNAL);
        }
        archivo.close();
        salida <<"]}";
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
                res.status = 400;
                res.set_content("{\"error\":\"falta parametro path\"}", "application/json");
                return;
            }

            string diskPath = req.get_param_value("path");

            Estructuras::MBR mbr;
            string errMsg;
            if (!mbr.DeserializeMBR(diskPath, errMsg)) {
                res.status =500;
                res.set_content("{\"error\":\"" + Json::escapar(errMsg) + "\"}", "application/json");
                return;
            }

            ostringstream salida;
            salida << "{\"partitions\":[";

            bool primera = true;
            for (int i = 0; i<4; ++i) {
                const Estructuras::PARTITION& p = mbr.Mbr_partitions[i];
                if (p.Partition_status[0] == '2'){
                    continue;
                }

                string nombre(p.Partition_name, sizeof(p.Partition_name));
                size_t nul = nombre.find('\0');
                if(nul != string::npos) {
                    nombre =nombre.substr(0, nul);
                }

                string pid(p.Partition_id, sizeof(p.Partition_id));
                nul=pid.find('\0');
                if (nul != string::npos) {
                    pid = pid.substr(0, nul);
                }

                if (!primera) 
                    salida << ",";

                primera =false;

                salida << "{";
                salida<< "\"name\":\""   <<Json::escapar(nombre) << "\",";
                salida << "\"type\":\""   << p.Partition_type[0] << "\",";
                salida << "\"fit\":\""    << p.Partition_fit[0] << "\",";
                salida << "\"status\":\"" << p.Partition_status[0] << "\",";
                salida <<"\"start\":"    <<p.Partition_start << ",";
                salida << "\"size\":"     << p.Partition_size << ",";
                salida<< "\"id\":\""     <<Json::escapar(pid) << "\"";
                salida << "}";
            }

            salida <<"]}";
            res.set_content(salida.str(), "application/json");
        });

        //GET /api/fs/list?id=<ID>&path=<ruta>
        svr->Get("/api/fs/list", [](const httplib::Request& req, httplib::Response& res) {
            setCors(res);

            if (!req.has_param("id") || !req.has_param("path")) {
                res.status = 400;
                res.set_content("{\"error\":\"faltan parametros id y path\"}", "application/json");
                return;
            }

            string id=req.get_param_value("id");
            string ruta =req.get_param_value("path");

            string errMsg;
            string json=listarCarpeta(id, ruta, errMsg);
            if (json.empty()) {
                res.status = 500;
                res.set_content("{\"error\":\"" + Json::escapar(errMsg) + "\"}", "application/json");
                return;
            }
            res.set_content(json, "application/json");
        });

        //GET /api/fs/file?id=<ID>&path=<ruta>
        svr->Get("/api/fs/file", [](const httplib::Request& req, httplib::Response& res) {
            setCors(res);

            if (!req.has_param("id") || !req.has_param("path")) {
                res.status=400;
                res.set_content("{\"error\":\"faltan parametros id y path\"}", "application/json");
                return;
            }

            string id= req.get_param_value("id");
            string ruta=req.get_param_value("path");

            string errMsg;
            string json = leerArchivoTexto(id, ruta, errMsg);
            if (json.empty()) {
                res.status = 500;
                res.set_content("{\"error\":\"" + Json::escapar(errMsg) + "\"}", "application/json");
                return;
            }
            res.set_content(json, "application/json");
        });

        //GET /api/fs/journal?id=<ID
        svr->Get("/api/fs/journal", [](const httplib::Request& req, httplib::Response& res) {
            setCors(res);

            if (!req.has_param("id")){
                res.status = 400;
                res.set_content("{\"error\":\"falta parametro id\"}", "application/json");
                return;
            }

            string id=req.get_param_value("id");
            string errMsg;
            string json = leerJournal(id, errMsg);
            if (json.empty()) {
                res.status=500;
                res.set_content("{\"error\":\"" +Json::escapar(errMsg) + "\"}","application/json");
                return;
            }
            res.set_content(json, "application/json");
        });

        cout <<"Servidor HTTP escuchando en http://0.0.0.0:8080" << endl;
        cout<< "Endpoints:" << endl;
        cout << "  POST /api/execute" << endl;
        cout<< "  GET /api/health" << endl;
        cout <<"  GET /api/disks" << endl;
        cout << "  GET  /api/disks/partitions?path=<ruta>" << endl;
        cout <<"  GET /api/fs/list?id=<ID>&path=<ruta>" << endl;
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