#include "FDISK.h"
#include "../Str_Mbr/MBR.h"
#include "../Str_Ebr/EBR.h"
#include "../../Utils/Utilities.h"
#include <algorithm>
#include <cstring>
#include <vector>
#include <fstream>
using namespace std;

namespace Estructuras
{
    // Funciones auxiliares privadas de este archivo, una por cada tipo de particion que se puede crear con FDISK
    namespace
    {

        // Crea una particion PRIMARIA dentro del MBR del disco
        bool C_primaryPartition(const FDISK &fdisk, long long sizeBytes, string &errMsg)
        {
            //Se lee el MBR actual del disco para saber que particiones ya existen y cuanto espacio queda libre
            MBR part_mbr{};
            if (!part_mbr.DeserializeMBR(fdisk.Path, errMsg))
            {
                errMsg ="ERROR: No se pudo leer el MBR del disco: "+ errMsg;
                return false;
            }

            long long space_used= 0;
            int primaryParts =0;

            /* Se recorren las 4 particiones del MBR. Se ignoran las que
            estan libres (status== '2'). De las que si estan en uso,
            se suma su tamaño (space_used) y se cuentan las primarias
            y extendidas existentes, ya que ambas ocupan un slot de los 4 disponibles en el MBR*/
            for (int i=0; i<4; ++i)
            {
                const PARTITION &partition =part_mbr.Mbr_partitions[i];
                if (partition.Partition_status[0]!= '2')
                {
                    if (partition.Partition_type[0]== 'P' || partition.Partition_type[0]=='E')
                    {
                        space_used+= partition.Partition_size;
                        primaryParts++;
                    }
                }
            }

            //Espacio libre real= tamaño total del disco - lo ya usado
            long long free_space =static_cast<long long>(part_mbr.Mbr_size)- space_used;

            //Validaciones antes de crear la particion:
            if (primaryParts >=4)
            {
                errMsg = "ERROR: Ya existen 4 particiones en el disco (primarias o extedidas)";
                return false;
            }

            //La particion pedida no puede ser mas grande que el disco entero
            if (sizeBytes> part_mbr.Mbr_size)
            {
                errMsg = "ERROR: El tamaño de la particion es mayor al tamaño diponible en el disco";
                return false;
            }

            //La particion pedida no puede ser mas grande que el espacio libre
            if (sizeBytes > free_space)
            {
                errMsg ="ERROR: El tamaño de la particion es mayor al tamaño disponible en el disco";
                return false;
            }

            //Se busca el primer slot libre dentro del arreglo de 4 particiones.
            int startParticion =-1;
            int indexParticion = -1;
            string gerr;
            PARTITION *newParticion = part_mbr.GetFirstPartitionAvailable(startParticion, indexParticion, gerr);

            if (newParticion == nullptr)
            {
                errMsg = "ERROR: No se pudo obtener una particion disponible";
                return false;
            }

            // 6) Se llenan los datos de la particion en ese slot: status,
            //    tipo, ajuste, tamaño, nombre y byte de inicio en el disco
            newParticion->CreatePartition(startParticion, static_cast<int>(sizeBytes), fdisk.Type, fdisk.Fit, fdisk.Name);

            // 7) Se guarda el MBR actualizado de vuelta al disco
            if (!part_mbr.SerializeMBR(fdisk.Path, errMsg))
            {
                errMsg = "ERROR: No se pudo escribir el MBR en el disco: " + errMsg;
                return false;
            }
            return true;
        }

        // Crea la particion EXTENDIDA del disco
        bool C_extendedPartition(const FDISK &fdisk, long long sizeBytes, string &errMsg)
        {
            MBR part_mbr{};
            if (!part_mbr.DeserializeMBR(fdisk.Path, errMsg))
            {
                errMsg ="ERROR: No se pudo leer el MBR del disco: " + errMsg;
                return false;
            }

            //Se revisa si ya existe una particion extendida activa
            int extendedExists= 0;
            for (int i = 0; i <4; ++i)
            {
                const PARTITION &partition = part_mbr.Mbr_partitions[i];
                if (partition.Partition_status[0] != '2' && partition.Partition_type[0] == 'E')
                {
                    extendedExists++;
                }
            }

            if (extendedExists>0)
            {
                errMsg = "ERROR: Ya existe una particon extendida";
                return false;
            }

            if (sizeBytes >part_mbr.Mbr_size)
            {
                errMsg = "ERROR: El tamaño de la particion es mayor al tamaño disponible en el disco";
                return false;
            }

            //Igual que en la primaria: se busca un slot libre, se llena
            //con los datos de la particion y se guarda el MBR
            int startParticion = -1;
            int indexParticion =-1;
            string gerr;
            PARTITION *newParticion = part_mbr.GetFirstPartitionAvailable(startParticion, indexParticion,gerr);

            if (newParticion== nullptr)
            {
                errMsg="ERROR: No se pudo obtener una particion disponible";
                return false;
            }
            newParticion->CreatePartition(startParticion, static_cast<int>(sizeBytes),  fdisk.Type, fdisk.Fit, fdisk.Name);

            if (!part_mbr.SerializeMBR(fdisk.Path, errMsg))
            {
                errMsg= "ERROR: No se pudo escribir el MBR en el disco: " + errMsg;
                return false;
            }
            return true;
        }

        //Copia el nombre de la particion logica dentro del campo Partition_name del EBR, rellenando con ceros el resto
        static void fillEbrName(EBR &ebr, const string &name)
        {
            memset(ebr.Partition_name, 0, sizeof(ebr.Partition_name));
            size_t n= min(name.size(), sizeof(ebr.Partition_name));
            memcpy(ebr.Partition_name, name.data(), n);
        }


        // Crea una particion LOGICA dentro de la particion extendida.
        bool C_logicalPartition(const FDISK &fdisk, long long sizeBytes, string &errMsg)
        {
            MBR part_mbr{};
            if(!part_mbr.DeserializeMBR(fdisk.Path, errMsg))
            {
                errMsg= "ERROR: No se pudo leer el MBR del disco: "+errMsg;
                return false;
            }

            //Se busca la particion extendida dentro del MBR: la logica tiene que vivir dentro de ella, si no existe no hay donde crearla.
            PARTITION extendedPartition{};
            bool foundExtended = false;
            for (int i = 0; i<4; ++i)
            {
                if (part_mbr.Mbr_partitions[i].Partition_type[0]== 'E')
                {
                    extendedPartition = part_mbr.Mbr_partitions[i];
                    foundExtended =true;
                    break;
                }
            }

            if (!foundExtended)
            {
                errMsg = "ERROR: No existe una particion extendida";
                return false;
            }

            //La logica no puede ser mas grande que toda la extendida
            if (sizeBytes > extendedPartition.Partition_size)
            {
                errMsg ="ERROR: El tamaño de la particion logica es mayor al tamaño disponible en la particion extendida";
                return false;
            }

            //Se abre el archivo del disco en modo lectura/escritura binaria.
            fstream file(fdisk.Path, ios::binary | ios::in | ios::out);
            if (!file.is_open())
            {
                errMsg ="ERROR: No se pudo abrir el archivo del disco";
                return false;
            }

            //Se intenta leer el primer EBR, justo al inicio de la particion extendida.
            file.seekg(extendedPartition.Partition_start, ios::beg);

            EBR ebr{};
            file.read(reinterpret_cast<char *>(&ebr), sizeof(EBR));

            // caso1: todavia no hay ningun EBR en la extendida
            if (!file || ebr.Partition_size == 0)
            {
                file.clear();
                ebr =EBR{};
                ebr.Partition_mount[0] ='0';
                ebr.Partition_fit[0] =fdisk.Fit[0];
                ebr.Partition_start= extendedPartition.Partition_start;
                ebr.Partition_size =static_cast<int32_t>(sizeBytes);
                ebr.Partition_next=-1;
                fillEbrName(ebr, fdisk.Name);

                // Se escribe el EBR en el disco
                file.seekp(extendedPartition.Partition_start, ios::beg);
                file.write(reinterpret_cast<const char *>(&ebr), sizeof(EBR));
                if(!file)
                {
                    errMsg="ERROR: No se pudo escribir el EBR en la particion extedida";
                    return false;
                }

                // Justo despues del EBR (en el disco) va el contenido real de la particion logica
                int32_t logicalStart = extendedPartition.Partition_start +static_cast<int32_t>(sizeof(EBR));

                PARTITION logicalPartition{};
                logicalPartition.CreatePartition(static_cast<int>(logicalStart), static_cast<int>(sizeBytes), fdisk.Type, fdisk.Fit, fdisk.Name);
                memcpy(logicalPartition.Partition_id, extendedPartition.Partition_id, sizeof(logicalPartition.Partition_id));

                file.seekp(logicalStart, ios::beg);
                file.write(reinterpret_cast<const char*>(&logicalPartition), sizeof(PARTITION));
                if (!file)
                {
                    errMsg="ERROR: No se pudo escribir la particion logica";
                    return false;
                }
                file.close();

                // Se vuelve a guardar el MBR
                if (!part_mbr.SerializeMBR(fdisk.Path, errMsg)){
                    return false;
                }
                return true;
            } 

            // caso 2: ya existe al menos un EBR .
            long long size_used=ebr.Partition_size;

            while (ebr.Partition_next != -1)
            {
                file.seekg(ebr.Partition_next, ios::beg);
                file.read(reinterpret_cast<char *>(&ebr), sizeof(EBR));
                if (!file)
                {
                    errMsg = "ERROR: No se pudo leer el siguiente EBR";
                    return false;
                }
                size_used += ebr.Partition_size;
            }
            // Al salir del while, "ebr" contiene el ULTIMO EBR de la cadena

            // Se calcula cuanto espacio libre queda dentro de la extendida y se valida que la nueva logica quepa
            int32_t free_size = extendedPartition.Partition_size - static_cast<int32_t>(size_used);

            if (sizeBytes> free_size)
            {
                errMsg ="ERROR: El tamaño de la particion logica es mayor al tamaño disponible en la particion extendida";
                return false;
            }

            // El nuevo EBR se ubica justo despues del contenido de la ultima particion logica existente
            int32_t newEBRstart = ebr.Partition_start + ebr.Partition_size+ static_cast<int32_t>(sizeof(EBR));

            //Se actualiza el ULTIMO EBR existente para que apunte al nuevo 
            ebr.Partition_next= newEBRstart;
            file.clear();
            file.seekp(ebr.Partition_start, ios::beg);
            file.write(reinterpret_cast<const char *>(&ebr), sizeof(EBR));
            if (!file)
            {
                errMsg= "ERROR: No se pudo actualizar el EBR anterior con el nuevo";
                return false;
            }

            EBR ebrNew{};
            ebrNew.Partition_mount[0]= '0';
            ebrNew.Partition_fit[0] = fdisk.Fit[0];
            ebrNew.Partition_start=newEBRstart;
            ebrNew.Partition_size = static_cast<int32_t>(sizeBytes);
            ebrNew.Partition_next =-1;
            fillEbrName(ebrNew, fdisk.Name);

            file.seekp(ebrNew.Partition_start, ios::beg);
            file.write(reinterpret_cast<const char *>(&ebrNew), sizeof(EBR));
            if (!file)
            {
                errMsg= "ERROR: No se pudo escribir el nuevo EBR";
                return false;
            }

            //Justo despues del nuevo EBR va el contenido de la nueva particion logica
            int32_t logicalStart= newEBRstart+ static_cast<int32_t>(sizeof(EBR));

            PARTITION logicalPartition{};
            logicalPartition.CreatePartition(static_cast<int>(logicalStart), static_cast<int>(sizeBytes), fdisk.Type, fdisk.Fit, fdisk.Name);
            // Misma herencia de Partition_id que en el camino A.
            memcpy(logicalPartition.Partition_id, extendedPartition.Partition_id, sizeof(logicalPartition.Partition_id));

            file.seekp(logicalStart, ios::beg);
            file.write(reinterpret_cast<const char *>(&logicalPartition), sizeof(PARTITION));
            if (!file)
            {
                errMsg= "ERROR: No se pudo escribir la particion logica";
                return false;
            }
            file.close();
            return true;
        }


        //eliminacion de una particion primaria o extendida del MBR.
        bool borrarParticion(const FDISK& fdisk, bool full, string& errMsg){
            MBR mbr;
            if(!mbr.DeserializeMBR(fdisk.Path, errMsg)){
                errMsg = "ERROR: No se pudo leer MBR: " + errMsg;
                return false;
            }

            int idx =-1;
            PARTITION* part=mbr.GetPartitionByName(fdisk.Name, idx, errMsg);
            if (!part) {
                errMsg = "ERROR: No existe una partcion llamada '" + fdisk.Name + "'";
                return false;
            }

            if (part->Partition_status[0]=='2'){
                errMsg = "ERROR: La particion ya esta vacia";
                return false;
            }
            bool eraExtendida= (part->Partition_type[0] == 'E');

            int32_t inicio= part->Partition_start;
            int32_t tam = part->Partition_size;

            //Si es extendida, se borram cadena de EBRs relenando con \0 full
            if (eraExtendida && full){
                fstream archivo(fdisk.Path, ios::binary | ios::in | ios::out);
                if (!archivo.is_open()){
                    errMsg = "ERROR: No se pudo abrir disco";
                    return false;
                }
                std::vector<char> ceros(tam, 0);
                archivo.seekp(inicio, ios::beg);
                archivo.write(ceros.data(), tam);
                archivo.close();
            } 
            else if (full){
                //es Primaria: rellenado con \0 su espacio
                fstream archivo(fdisk.Path, ios::binary | ios::in | ios::out);
                if (!archivo.is_open()){
                    errMsg="ERROR: No se pudo abrir disco";
                    return false;
                }
                std::vector<char> ceros(tam, 0);
                archivo.seekp(inicio,ios::beg);
                archivo.write(ceros.data(), tam);
                archivo.close();
            }
  
            *part= EmptyPartition();
            if(!mbr.SerializeMBR(fdisk.Path, errMsg)){
                errMsg = "ERROR: No se pudo actualizar MBR";
                return false;
            }
            return true;
        }

        //ajuste del tamaño de una particion primaria (-add positivo o negativo).
        bool agregarEspacio(const FDISK& fdisk, long long deltaBytes, string& errMsg) {
            MBR mbr;
            if(!mbr.DeserializeMBR(fdisk.Path, errMsg)){
                errMsg = "ERROR: No se pudo leer MBR: " + errMsg;
                return false;
            }

            int idx =-1;
            PARTITION* part = mbr.GetPartitionByName(fdisk.Name, idx, errMsg);
            if (!part) {
                errMsg="ERROR: No existe una particion llamada '" + fdisk.Name + "'";
                return false;
            }

            if (part->Partition_type[0] != 'P') {
                errMsg = "ERROR: Solo se puede agregar espacio a particiones primarias";
                return false;
            }

            int32_t nuevoTam = part->Partition_size + (int32_t)deltaBytes;
            if (nuevoTam <= 0) {
                errMsg ="ERROR: El tamano resuante seria negativo o cero";
                return false;
            }

            //verificacion que quepa en el disco
            long long nuevoFin = (long long)part->Partition_start+ (long long)nuevoTam;
            if (nuevoFin > mbr.Mbr_size) {
                errMsg ="ERROR: No hay suficiente espacio despues de la particion";
                return false;
            }

            //verificacion que no choque con la siguiente particion ocupada
            for (int i = 0; i < 4; ++i){
                if (&mbr.Mbr_partitions[i]==part) 
                    continue;

                const PARTITION& otra= mbr.Mbr_partitions[i];
                if(otra.Partition_status[0] =='2') 
                    continue;

                long long otroInicio=otra.Partition_start;
                long long otroFin= otroInicio + otra.Partition_size;
                if (nuevoFin> otroInicio && (long long)part->Partition_start< otroFin) {
                    errMsg ="ERROR: La particion chocaria con otra existente";
                    return false;
                }
            }
            part->Partition_size = nuevoTam;
            if (!mbr.SerializeMBR(fdisk.Path, errMsg)) {
                errMsg="ERROR: No se pudo actualizar MBR";
                return false;
            }
            return true;
        }
    }

    //recbe los datos ya validdos del comando FDISK (tamaño, unidad, path disco, etc) despacha a la funcion correspndiente segun el tipo de particion
    bool Struct_FDISK(const FDISK &fdisk, string &errMsg){
        //Modo delete
        if (!fdisk.Delete.empty()){
            bool full= (fdisk.Delete== "full");
            return borrarParticion(fdisk, full, errMsg);
        }

        // modo add
        if (fdisk.TieneAdd){
            long long deltaBytes=0;
            if(!Utilities::ConvertBytes(std::abs(fdisk.Add), fdisk.Unit.empty() ? "K" : fdisk.Unit, deltaBytes, errMsg)) {
                return false;
            }
            if(fdisk.Add< 0) 
                deltaBytes= -deltaBytes;

            return agregarEspacio(fdisk, deltaBytes, errMsg);
        }
        
        long long sizeBytes= 0;
        if (!Utilities::ConvertBytes(fdisk.Size, fdisk.Unit, sizeBytes, errMsg))
        {
            errMsg = "ERROR: No se pudo convertir el size de la particion (" + errMsg + ")";
            return false;
        }

        if (fdisk.Type== "P")
        {
            return C_primaryPartition(fdisk, sizeBytes, errMsg);
        }
        else if (fdisk.Type == "E")
        {
            return C_extendedPartition(fdisk, sizeBytes, errMsg);
        }
        else if (fdisk.Type== "L")
        {
            return C_logicalPartition(fdisk, sizeBytes, errMsg);
        }
        errMsg ="ERROR: Tipo de particion no reconocido";
        return false;
    }
}
