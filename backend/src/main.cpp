#include <iostream>
#include <string>
#include <vector>
#include "Analyzer/Analyzer.h"
#include "../httplib.h"
#include "Server/HttpServer.h"
using namespace std;

int main() {
    srand(time(nullptr));

    thread serverThread([](){Server::iniciarServidor();});
    serverThread.detach();
    this_thread::sleep_for(chrono::milliseconds(500));

    cout <<"-------Simulador de Discos MIA 2S2026---------" << endl;
    cout <<"Escribe 'salir' para terminar." << endl;

    string linea;
    while (true){
        cout <<"\n> ";
        getline(cin, linea);
        if (linea=="salir") 
            break;

        if (linea.empty()) 
            continue;
        Analyzer::Analyze(vector<string>{linea});
    }
    Server::detenerServidor();
    return 0;
}