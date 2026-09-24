#include "Analyzer.h"
#include "../Comandos/CommandResult.h"
#include "../Comandos/Mkdisk_Command/Mkdisk.h"
#include "../Comandos/Fdisk_Command/Fdisk.h"
#include "../Comandos/Mount_Command/Mount.h"
#include <iostream>
#include "../Global/MountedPartitions.h"
#include "../Comandos/Rmdisk_Command/Rmdisk.h"
#include "../Comandos/Mkfs_Command/Mkfs.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include "../Comandos/Login_Command/Login.h"
#include "../Comandos/Logout_Command/Logout.h"
#include "../Comandos/Mkgrp_Command/Mkgrp.h"
#include "../Comandos/Rmgrp_Command/Rmgrp.h"
#include "../Comandos/Mkusr_Command/Mkusr.h"
#include "../Comandos/Mkfile_Command/Mkfile.h"
#include "../Comandos/Mkdir_Command/Mkdir.h"
#include "../Comandos/Cat_Command/Cat.h"
#include "../Comandos/Rmusr_Command/Rmusr.h"
#include "../Comandos/Chgrp_Command/Chgrp.h"
#include "../Comandos/Rep_Command/Rep.h"

using namespace std;
namespace Analyzer {
    static string trim(const string& s) {
        size_t start= s.find_first_not_of(" \t\r\n");
        if (start ==string::npos){
            return "";
        }
        size_t end= s.find_last_not_of(" \t\r\n");
        return s.substr(start, end-start + 1);
    }

    static vector<string> fields(const string& s){
        vector<string> tokens;
        string actual;
        bool enComillas=false;

        for (size_t i = 0; i<s.size(); ++i) {
            char c =s[i];
            if (c =='"') {
                enComillas =!enComillas;
                actual +=c;  
            } 
            else if(c ==' ' || c == '\t') {
                if (enComillas) {
                    actual += c;  
                } 
                else{
                    if(!actual.empty()){
                        tokens.push_back(actual);
                        actual.clear();
                    }
                }
            } 
            else{
                actual+=c;
            }
        }
        if (!actual.empty()) 
            tokens.push_back(actual);

        return tokens;
    }

    static string toLower(string s){
        transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {return tolower(c); }); //funcion minusculas 
        return s;
    }

    void Analyze(const vector<string>& inputs){
        if (inputs.empty()) {
            cout <<"[ERROR] No se proporcionó ningn comando" << endl;
            return;
        }

        string input =trim(inputs[0]);
        if (input.empty()) {
            cout << endl;
            return;
        }

        if (input[0] == '#'){
            cout << input << endl;
            return;
        }

        vector<string> tokens= fields(input);
        if (tokens.empty()) {
            cout <<"[ERROR] No se proporcionó ningún comando válido" << endl;
            return;
        }

        tokens[0]=toLower(tokens[0]);
        vector<string> params(tokens.begin()+1, tokens.end());

        bool hasError =false;
        string errorMsg;
        string msg;

        if (tokens[0] =="mkdisk"){
            Comandos::CommandResult result = Comandos::Mkdisk_Command(params);
            if (result.success) {
                msg =result.message;
            }
            else{ 
                hasError= true; 
                errorMsg =result.message; 
            }
        } 
        else if(tokens[0] =="rmdisk"){
            Comandos::CommandResult result =Comandos::Rmdisk_Command(params);
            if (result.success) {
                msg= result.message;
            }
            else{ 
                hasError = true;
                errorMsg =result.message; 
            }
        } 
        else if (tokens[0]== "fdisk"){
            Comandos::CommandResult result= Comandos::Fdisk_Command(params);
            if (result.success) {
                msg =result.message;
            }
            else { 
                hasError= true; 
                errorMsg =result.message;
             }
        }
        else if (tokens[0]== "mount") {
            Comandos::CommandResult result=Comandos::Mount_Command(params);
            if (result.success) {
                msg =result.message;
            }
            else{ 
                hasError= true; 
                errorMsg =result.message; 
            }
        } 
        else if (tokens[0]=="mounted"){
            // Comando mounted: listar particiones montadas
            if (Global::MountedPartitions.empty()) {
                msg ="No hay particiones montadas.";
            }
            else {
                msg ="Particiones montadas:\n";
                for(auto& kv : Global::MountedPartitions) {
                    msg+=" ID: " + kv.first+ " -> Disco: " + kv.second + "\n";
                }
            }
        } 
        else if (tokens[0]== "mkfs") {
            Comandos::CommandResult result= Comandos::Mkfs_Command(params);
            if (result.success) {
                msg =result.message;
            }
            else{
                hasError= true;
                errorMsg =result.message;
            }
        }
        else if (tokens[0]== "login") {
            Comandos::CommandResult result= Comandos::Login_Command(params);
            if (result.success) {
                msg =result.message;
            }
            else{
                hasError= true;
                errorMsg =result.message;
            }
        }
        else if (tokens[0]== "logout") {
            Comandos::CommandResult result= Comandos::Logout_Command(params);
            if (result.success) {
                msg =result.message;
            }
            else{
                hasError = true;
                errorMsg =result.message;
            }
        }
        else if (tokens[0]== "mkgrp") {
            Comandos::CommandResult result= Comandos::Mkgrp_Command(params);
            if (result.success) {
                msg =result.message;
            }
            else{
                hasError= true;
                errorMsg =result.message;
            }
        }
        else if (tokens[0]== "rmgrp") {
            Comandos::CommandResult result= Comandos::Rmgrp_Command(params);
            if (result.success) {
                msg =result.message;
            }
            else{
                hasError= true;
                errorMsg =result.message;
            }
        }
        else if (tokens[0]== "mkusr") {
            Comandos::CommandResult result= Comandos::Mkusr_Command(params);
            if (result.success) {
                msg =result.message;
            }
            else{
                hasError= true;
                errorMsg =result.message;
            }
        }
        else if (tokens[0]== "rmusr") {
            Comandos::CommandResult result= Comandos::Rmusr_Command(params);
            if (result.success) {
                msg =result.message;
            }
            else{
                hasError= true;
                errorMsg =result.message;
            }
        }
        else if (tokens[0]=="chgrp"){
            Comandos::CommandResult result= Comandos::Chgrp_Command(params);
            if (result.success) {
                msg =result.message;
            }
            else{
                hasError= true;
                errorMsg =result.message;
            }
        }
        else if (tokens[0]=="mkfile"){
            Comandos::CommandResult result= Comandos::Mkfile_Command(params);
            if (result.success){
                msg = result.message;
            }
            else{
                hasError = true;
                errorMsg =result.message;
            }
        }
        else if (tokens[0] =="mkdir"){
            Comandos::CommandResult result= Comandos::Mkdir_Command(params);
            if (result.success) {
                msg =result.message;
            }
            else{
                hasError= true;
                errorMsg =result.message;
            }
        }
        else if (tokens[0] =="cat"){
            Comandos::CommandResult result= Comandos::Cat_Command(params);
            if (result.success) {
                msg =result.message;
            }
            else{
                hasError= true;
                errorMsg =result.message;
            }
        }
        else if (tokens[0] =="rep"){
            Comandos::CommandResult result= Comandos::Rep_Command(params);
            if (result.success) {
                msg =result.message;
            }
            else{
                hasError= true;
                errorMsg =result.message;
            }
        }
        else{
            hasError= true;
            errorMsg = "Comando no reconocido: "+ tokens[0];
        }
    
        if (hasError){
            cout <<"[ERROR] "<< errorMsg <<endl;
        }
        else
            cout <<msg <<endl;
    }

    std::string AnalyzeCapture(const std::vector<std::string>& inputs){
        stringstream buffer;
        streambuf* oldCout= std::cout.rdbuf(buffer.rdbuf());

        Analyze(inputs); //llamada funcion

        cout.rdbuf(oldCout);
        return buffer.str();
    }
}