#include "Sesion.h"

namespace Global {
    SESION sesionActual;

    void CerrarSesion() {
        sesionActual = SESION();
    }
}
