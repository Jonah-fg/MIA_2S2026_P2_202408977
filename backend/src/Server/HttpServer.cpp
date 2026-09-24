#include "HttpServer.h"
#include "../../httplib.h"
#include "../Analyzer/Analyzer.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <streambuf>
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

    void iniciarServidor(){
        svr =new httplib::Server();

        //Ruta POST /execute: recibe un comando o script y devuelve la salida
        svr->Post("/api/execute", [](const httplib::Request& req, httplib::Response& res) {
            setCors(res);

            string comando=req.body;
            string salida =ejecutarScript(comando);
            bool success= salida.find("[ERROR]") == string::npos && salida.find("ERROR:")== string::npos;

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
        cout << "Servidor HTTP escuchando en http://0.0.0.0:8080"<< endl;
        cout << "Endpoints: POST /api/execute | GET /api/health"<< endl;
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