#ifndef ANALYZER_H
#define ANALYZER_H
#include <string>
#include <vector>

namespace Analyzer {
    // Ejecuta un comando y captura la salida (stdout).
    // Devuelve el texto que el comando imprimió.
    std::string AnalyzeCapture(const std::vector<std::string>& inputs);

    // Se mantiene la firma original por compatibilidad con main.cpp
    void Analyze(const std::vector<std::string>& inputs);
}

#endif