#ifndef PERMISOS_UTILS_H
#define PERMISOS_UTILS_H

#include "../Estructuras/Str_Inode/INODE.h"
namespace PermisosUtils {
    //Verifica si el usuario actual (root/uidActual) puede escribir sobre el inodo.
    bool PuedeEscribir(const Estructuras::INODE& inodo, bool esRoot, int uidActual);
    bool PuedeLeer(const Estructuras::INODE& inodo, bool esRoot, int uidActual);
}
#endif