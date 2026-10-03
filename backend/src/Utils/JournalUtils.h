#ifndef JOURNAL_UTILS_H
#define JOURNAL_UTILS_H
#include <string>
#include "../Estructuras/Str_Superblock/SUPERBLOCK.h"
using namespace std;

namespace JournalUtils{
    bool EsExt3(const Estructuras::SUPERBLOCK& sb);

    //rregistra una operación en el journal de una partición EXT3.
    bool RegistrarOperacion(const string& diskPath, const Estructuras::SUPERBLOCK& sb, const string& operacion, const string& path, const string& contenido, string& errMsg);
}
#endif