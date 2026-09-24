#include "Ext2Utils.h"
#include "../Global/Sesion.h"
#include "../Estructuras/Str_Fileblock/FILEBLOCK.h"
#include "../Estructuras/Str_Folderblock/FOLDERBLOCK.h"
#include <fstream>
#include <cstring>
#include <ctime>
#include <vector>

using namespace std;

namespace Ext2Utils {

    bool LeerSuperbloque(const string& diskPath, int partitionStart,
                         Estructuras::SUPERBLOCK& sb, string& errMsg) {
        return sb.Deserialize(diskPath, partitionStart, errMsg);
    }

    bool LeerInodo(const string& diskPath, const Estructuras::SUPERBLOCK& sb,
                   int inodoNum, Estructuras::INODE& inode, string& errMsg) {
        long long offset = sb.Sb_inode_start + (inodoNum * sb.Sb_inode_size);
        return inode.Deserialize(diskPath, offset, errMsg);
    }

    bool EscribirInodo(const string& diskPath, const Estructuras::SUPERBLOCK& sb,
                       int inodoNum, const Estructuras::INODE& inode, string& errMsg) {
        long long offset = sb.Sb_inode_start + (inodoNum * sb.Sb_inode_size);
        Estructuras::INODE temp = inode;
        return temp.Serialize(diskPath, offset, errMsg);
    }

    bool LeerArchivo(const string& diskPath, const Estructuras::SUPERBLOCK& sb,
                     const Estructuras::INODE& inode, string& contenido, string& errMsg) {
        contenido.clear();
        int totalBytes = inode.I_size;
        int bytesLeidos = 0;

        for (int i = 0; i < 12; ++i) {
            if (inode.I_block[i] == -1) break;

            int blockNum = inode.I_block[i];
            long long blockOffset = sb.Sb_block_start + (blockNum * sb.Sb_block_size);

            Estructuras::FILEBLOCK fileBlock;
            if (!fileBlock.Deserialize(diskPath, blockOffset, errMsg)) return false;

            int bytesPorBloque = sb.Sb_block_size;
            if (bytesLeidos + bytesPorBloque > totalBytes) {
                bytesPorBloque = totalBytes - bytesLeidos;
            }
            contenido.append(fileBlock.B_content, bytesPorBloque);
            bytesLeidos += bytesPorBloque;
            if (bytesLeidos >= totalBytes) break;
        }

        if (bytesLeidos != totalBytes) {
            errMsg = "No se pudo leer todo el archivo";
            return false;
        }
        return true;
    }

    int AsignarBloqueLibre(const string& diskPath,
                           const Estructuras::SUPERBLOCK& sb, string& errMsg) {
        int bmSize = sb.Sb_blocks_count + sb.Sb_free_blocks_count;
        vector<char> bitmap(bmSize, '0');

        ifstream file(diskPath, ios::binary);
        if (!file.is_open()) {
            errMsg = "No se pudo abrir el disco para leer bitmap de bloques";
            return -1;
        }
        file.seekg(sb.Sb_bm_block_start, ios::beg);
        file.read(bitmap.data(), bmSize);
        if (!file) {
            errMsg = "Error al leer bitmap de bloques";
            return -1;
        }
        file.close();

        for (int i = 0; i < bmSize; ++i) {
            if (bitmap[i] == '0') {
                bitmap[i] = '1';
                fstream outFile(diskPath, ios::binary | ios::in | ios::out);
                if (!outFile.is_open()) {
                    errMsg = "No se pudo abrir el disco para escribir bitmap";
                    return -1;
                }
                outFile.seekp(sb.Sb_bm_block_start + i, ios::beg);
                outFile.write(&bitmap[i], 1);
                if (!outFile) {
                    errMsg = "Error al escribir bitmap de bloques";
                    return -1;
                }
                outFile.close();
                return i;
            }
        }
        errMsg = "No hay bloques libres";
        return -1;
    }

    bool EscribirArchivo(const string& diskPath, const Estructuras::SUPERBLOCK& sb,
                         int inodoNum, Estructuras::INODE& inode,
                         const string& contenido, string& errMsg) {
        int totalBytes = contenido.size();
        int blockSize = sb.Sb_block_size;
        int numBloquesNecesarios = (totalBytes + blockSize - 1) / blockSize;

        if (numBloquesNecesarios > 12) {
            errMsg = "Archivo demasiado grande (máx 12 bloques)";
            return false;
        }

        for (int i = 0; i < numBloquesNecesarios; ++i) {
            if (inode.I_block[i] == -1 || inode.I_block[i] == 0) {
                int nuevoBloque = AsignarBloqueLibre(diskPath, sb, errMsg);
                if (nuevoBloque == -1) return false;
                inode.I_block[i] = nuevoBloque;
            }
        }

        int bytesEscritos = 0;
        for (int i = 0; i < numBloquesNecesarios; ++i) {
            int blockNum = inode.I_block[i];
            long long blockOffset = sb.Sb_block_start + (blockNum * blockSize);
            int bytesRestantes = totalBytes - bytesEscritos;
            int bytesEnEsteBloque = (bytesRestantes > blockSize) ? blockSize : bytesRestantes;

            Estructuras::FILEBLOCK fileBlock;
            memset(&fileBlock, 0, sizeof(fileBlock));
            memcpy(fileBlock.B_content, contenido.data() + bytesEscritos, bytesEnEsteBloque);
            if (!fileBlock.Serialize(diskPath, blockOffset, errMsg)) return false;
            bytesEscritos += bytesEnEsteBloque;
        }

        inode.I_size = totalBytes;
        inode.I_mtime = static_cast<float>(time(nullptr));
        if (!EscribirInodo(diskPath, sb, inodoNum, inode, errMsg)) return false;

        errMsg.clear();
        return true;
    }

    int BuscarInodoLibre(const string& diskPath,
                         const Estructuras::SUPERBLOCK& sb, string& errMsg) {
        int totalInodos = sb.Sb_inodes_count + sb.Sb_free_inodes_count;
        vector<Estructuras::INODE> inodos(totalInodos);

        ifstream file(diskPath, ios::binary);
        if (!file.is_open()) {
            errMsg = "No se pudo abrir el disco para leer tabla de inodos";
            return -1;
        }
        file.seekg(sb.Sb_inode_start, ios::beg);
        file.read(reinterpret_cast<char*>(inodos.data()), totalInodos * sizeof(Estructuras::INODE));
        if (!file) {
            errMsg = "Error al leer tabla de inodos";
            return -1;
        }
        file.close();

        for (int i = 0; i < totalInodos; ++i) {
            const Estructuras::INODE& inode = inodos[i];
            if (inode.I_type[0] == '\0' && inode.I_size == 0) {
                bool tieneBloqueReal = false;
                for (int j = 0; j < 15; ++j) {
                    if (inode.I_block[j] != 0 && inode.I_block[j] != -1) {
                        tieneBloqueReal = true;
                        break;
                    }
                }
                if (!tieneBloqueReal) return i;
            }
        }
        errMsg = "No hay inodos libres";
        return -1;
    }

    bool MarcarInodoUsado(const string& diskPath, const Estructuras::SUPERBLOCK& sb,
                          int inodoNum, string& errMsg) {
        fstream file(diskPath, ios::binary | ios::in | ios::out);
        if (!file.is_open()) {
            errMsg = "No se pudo abrir el disco para marcar inodo usado";
            return false;
        }
        long long pos = sb.Sb_bm_inode_start + inodoNum;
        file.seekp(pos, ios::beg);
        char bit = '1';
        file.write(&bit, 1);
        if (!file) {
            errMsg = "Error al escribir en el bitmap de inodos";
            file.close();
            return false;
        }
        file.close();
        errMsg.clear();
        return true;
    }

    bool MarcarBloqueUsado(const string& diskPath, const Estructuras::SUPERBLOCK& sb,
                           int bloqueNum, string& errMsg) {
        fstream file(diskPath, ios::binary | ios::in | ios::out);
        if (!file.is_open()) {
            errMsg = "No se pudo abrir el disco para marcar bloque usado";
            return false;
        }
        long long pos = sb.Sb_bm_block_start + bloqueNum;
        file.seekp(pos, ios::beg);
        char bit = '1';
        file.write(&bit, 1);
        if (!file) {
            errMsg = "Error al escribir en el bitmap de bloques";
            file.close();
            return false;
        }
        file.close();
        errMsg.clear();
        return true;
    }

    bool EncontrarSlotEnCarpeta(const string& diskPath,
                                const Estructuras::SUPERBLOCK& sb,
                                Estructuras::INODE& inodoPadre,
                                int& slotNum,
                                long long& blockOffset,
                                string& errMsg) {
        // 1) Buscar slot libre en los bloques directos existentes
        for (int b = 0; b < 12; ++b) {
            if (inodoPadre.I_block[b] == -1) continue;

            Estructuras::FOLDERBLOCK fb;
            long long off = sb.Sb_block_start + (inodoPadre.I_block[b] * sb.Sb_block_size);
            if (!fb.Deserialize(diskPath, off, errMsg)) return false;

            for (int i = 0; i < 4; ++i) {
                string nombre(fb.B_content[i].B_name);
                nombre = nombre.c_str();
                if (nombre.empty() || nombre == "-" || fb.B_content[i].B_inodo == -1) {
                    slotNum = i;
                    blockOffset = off;
                    return true;
                }
            }
        }

        // 2) Todos llenos → asignar bloque nuevo
        int siguiente = -1;
        for (int b = 0; b < 12; ++b) {
            if (inodoPadre.I_block[b] == -1) { siguiente = b; break; }
        }
        if (siguiente == -1) {
            errMsg = "Carpeta padre llena (máx 48 entradas)";
            return false;
        }

        int nuevoBloque = AsignarBloqueLibre(diskPath, sb, errMsg);
        if (nuevoBloque == -1) return false;

        inodoPadre.I_block[siguiente] = nuevoBloque;

        Estructuras::FOLDERBLOCK nuevoFB;
        memset(&nuevoFB, 0, sizeof(nuevoFB));
        for (int i = 0; i < 4; ++i) {
            nuevoFB.B_content[i].B_name[0] = '-';
            nuevoFB.B_content[i].B_inodo = -1;
        }
        long long off = sb.Sb_block_start + (nuevoBloque * sb.Sb_block_size);
        if (!nuevoFB.Serialize(diskPath, off, errMsg)) return false;

        MarcarBloqueUsado(diskPath, sb, nuevoBloque, errMsg);

        slotNum = 0;
        blockOffset = off;
        return true;
    }

    int CrearCarpetaEnPadre(const string& diskPath,
                            const Estructuras::SUPERBLOCK& sb,
                            int inodoPadre,
                            const string& nombreCarpeta,
                            string& errMsg) {
        // Leer inodo padre
        Estructuras::INODE inodePadre;
        if (!LeerInodo(diskPath, sb, inodoPadre, inodePadre, errMsg)) return -1;

        // Buscar slot libre (extendiendo si es necesario)
        int slotLibre;
        long long blockOffset;
        if (!EncontrarSlotEnCarpeta(diskPath, sb, inodePadre, slotLibre, blockOffset, errMsg)) {
            return -1;
        }

        // Asignar inodo para la nueva carpeta
        int nuevoInodo = BuscarInodoLibre(diskPath, sb, errMsg);
        if (nuevoInodo == -1) return -1;

        // Asignar bloque para la nueva carpeta
        int bloque = AsignarBloqueLibre(diskPath, sb, errMsg);
        if (bloque == -1) return -1;

        // Crear inodo de la nueva carpeta
        Estructuras::INODE newInode;
        memset(&newInode, 0, sizeof(newInode));
        newInode.I_uid = Global::sesionActual.uid;
        newInode.I_gid = Global::sesionActual.gid;
        newInode.I_size = 0;
        newInode.I_atime = static_cast<float>(time(nullptr));
        newInode.I_ctime = static_cast<float>(time(nullptr));
        newInode.I_mtime = static_cast<float>(time(nullptr));
        newInode.I_type[0] = '0';
        newInode.I_perm[0] = '6';
        newInode.I_perm[1] = '6';
        newInode.I_perm[2] = '4';
        newInode.I_block[0] = bloque;
        for (int i = 1; i < 15; ++i) newInode.I_block[i] = -1;

        // Crear bloque de carpeta con "." y ".."
        Estructuras::FOLDERBLOCK newFB;
        memset(&newFB, 0, sizeof(newFB));
        newFB.B_content[0].B_name[0] = '.';
        newFB.B_content[0].B_inodo = nuevoInodo;
        newFB.B_content[1].B_name[0] = '.';
        newFB.B_content[1].B_name[1] = '.';
        newFB.B_content[1].B_inodo = inodoPadre;
        newFB.B_content[2].B_name[0] = '-';
        newFB.B_content[2].B_inodo = -1;
        newFB.B_content[3].B_name[0] = '-';
        newFB.B_content[3].B_inodo = -1;

        long long newBlockOffset = sb.Sb_block_start + (bloque * sb.Sb_block_size);
        if (!newFB.Serialize(diskPath, newBlockOffset, errMsg)) return -1;
        if (!EscribirInodo(diskPath, sb, nuevoInodo, newInode, errMsg)) return -1;

        MarcarInodoUsado(diskPath, sb, nuevoInodo, errMsg);
        MarcarBloqueUsado(diskPath, sb, bloque, errMsg);

        // Agregar entrada en el padre
        Estructuras::FOLDERBLOCK fbPadre;
        if (!fbPadre.Deserialize(diskPath, blockOffset, errMsg)) return -1;
        fbPadre.B_content[slotLibre].B_inodo = nuevoInodo;
        strncpy(fbPadre.B_content[slotLibre].B_name, nombreCarpeta.c_str(), 12);
        if (!fbPadre.Serialize(diskPath, blockOffset, errMsg)) return -1;

        // Escribir inodo padre actualizado
        inodePadre.I_mtime = static_cast<float>(time(nullptr));
        if (!EscribirInodo(diskPath, sb, inodoPadre, inodePadre, errMsg)) return -1;

        return nuevoInodo;
    }

    int BuscarHijoEnCarpeta(const string& diskPath, const Estructuras::SUPERBLOCK& sb, const Estructuras::INODE& inodoPadre, const string& nombreHijo, string& errMsg) {
        for (int b = 0; b < 12; ++b) {
            if (inodoPadre.I_block[b] == -1) continue;

            Estructuras::FOLDERBLOCK fb;
            long long off = sb.Sb_block_start + (inodoPadre.I_block[b] * sb.Sb_block_size);
            if (!fb.Deserialize(diskPath, off, errMsg)) return -1;

            for (int i = 0; i < 4; ++i) {
                string nombre(fb.B_content[i].B_name);
                nombre = nombre.c_str();
                if (nombre == nombreHijo) {
                    return fb.B_content[i].B_inodo;
                }
            }
        }
        return -1;
    }
} 