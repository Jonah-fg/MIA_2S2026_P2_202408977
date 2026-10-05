const URL_BACKEND = "http://localhost:8080";
/**
  Envía un comando (o varios, en formato script) al backend.
  @param {string} comando 
  @returns {Promise<{success: boolean, message: string, output: string}>}
 */
export async function ejecutarComando(comando){
    try{
        const respuesta =await fetch(`${URL_BACKEND}/api/execute`, {
            method: "POST",
            headers: { "Content-Type": "text/plain" },
            body: comando,
        });

        if (!respuesta.ok) {
            return {
                success: false,
                message: `Error HTTP ${respuesta.status}: ${respuesta.statusText}`,
                output: "",
            };
        }
        //Parseo del Json que envia el bakcend
        const datos = await respuesta.json();
        return datos;

    }
    catch (error){
        return {
            success: false,
            message: `No se pudo conectar al backend: ${error.message}`,
            output: "",
        };
    }
}

/**
 * backend esta vivo o nel
 * @returns {Promise<boolean>}
 */
export async function verificarBackend() {
    try{
        const respuesta = await fetch(`${URL_BACKEND}/api/health`);
        return respuesta.ok;
    } 
    catch{
        return false;
    }
}

/**
 * Obtencion de la lista de discos .mia existentes
 * @returns {Promise<string[]>}
 */
export async function obtenerDiscos() {
    try{
        const respuesta =await fetch(`${URL_BACKEND}/api/disks`);
        if(!respuesta.ok) {
            return [];
        }
        const datos =await respuesta.json();
        return datos.discos || [];
    } 
    catch{
        return [];
    }
}

