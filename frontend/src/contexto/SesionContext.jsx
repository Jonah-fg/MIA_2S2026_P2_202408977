import { createContext, useContext, useState} from "react";

//Creacion del contexto
const SesionContext =createContext(null);

//Creacion del "proveedor" el componente que envuelve a toda la app y les da aceso al estado a los hijos.
export function SesionProvider({children }) {
    //Estado de sesión
    const [activa, setActiva]= useState(false);
    const [usuario, setUsuario]= useState("");
    const [idParticion, setIdParticion]=useState("");

    //Inicia sesión
    function iniciarSesion(datos) {
        setActiva(true);
        setUsuario(datos.usuario || "");
        setIdParticion(datos.idParticion || "");
    }

    //ierra sesión
    function cerrarSesion() {
        setActiva(false);
        setUsuario("");
        setIdParticion("");
    }

    const valor={
        activa,
        usuario,
        idParticion,
        iniciarSesion,
        cerrarSesion,
    };
    return(
        <SesionContext.Provider value={valor}>
        {children}
        </SesionContext.Provider>
    );
}

export function useSesion(){
    const contexto= useContext(SesionContext);
    if (!contexto){
        throw new Error("useSesion debe usarse dentro de <SesionProvider>");
    }
    return contexto;
}