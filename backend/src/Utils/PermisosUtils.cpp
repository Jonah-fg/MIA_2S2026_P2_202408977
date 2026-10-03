#include "PermisosUtils.h"
namespace PermisosUtils {
    static bool digitoTieneEscritura(char d){
        return d == '2' || d == '3' || d == '6' || d == '7';
    }

    bool PuedeEscribir(const Estructuras::INODE& inodo, bool esRoot, int uidActual) {
        if (esRoot) 
            return true;

        char permiso;
        if (inodo.I_uid== uidActual) {
            permiso = inodo.I_perm[0];//dueño
        } 
        else{
            permiso = inodo.I_perm[2];  //otros
        }
        return digitoTieneEscritura(permiso);
    }

}