#include "Logout.h"
#include "../../Global/Sesion.h"
#include "../../Global/MountedPartitions.h"
#include "../../Estructuras/Str_Superblock/SUPERBLOCK.h"
#include "../../Utils/Ext2Utils.h"
#include "../../Utils/JournalUtils.h"
using namespace std;
namespace Comandos{

    CommandResult Logout_Command(const vector<string>& /*tokens*/) {
        if (!Global::sesionActual.activa){
            return {false, "ERROR: No hay sesión activa"};
        }
        //registramos el logout en el journal
        string diskPath= Global::sesionActual.diskPath;
        string id =Global::sesionActual.idParticion;
        string usuario = Global::sesionActual.usuario;

        Estructuras::PARTITION mountedPart;
        string errMsg;
        if (Global::GetMountedPartition(id, mountedPart, diskPath, errMsg)){
            Estructuras::SUPERBLOCK sb;
            if (Ext2Utils::LeerSuperbloque(diskPath, mountedPart.Partition_start, sb, errMsg)){
                string journalErr;
                JournalUtils::RegistrarOperacion(diskPath, sb, "logout", "/", usuario,journalErr);
            }
        }
        Global::CerrarSesion();
        return {true, "LOGOUT: Sesión cerrada con éxito"};
    }
}
