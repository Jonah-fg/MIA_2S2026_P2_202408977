#ifndef ANALYZER_H
#define ANALYZER_H
#include <string>
#include <vector>

namespace Analyzer {
    std::string AnalyzeCapture(const std::vector<std::string>& inputs);
    void Analyze(const std::vector<std::string>& inputs);
}

#endif