#pragma once
#include <string>

namespace Global {
    struct SESION {
        bool activa = false;
        std::string usuario;
        std::string idParticion;
        std::string diskPath;
        int uid = -1;
        int gid = -1;
        bool esRoot = false;
    };

    extern SESION sesionActual;
    void CerrarSesion();
}
