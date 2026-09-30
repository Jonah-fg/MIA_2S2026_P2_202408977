import Terminal from "./paginas/Terminal";
import { useState } from "react";
import Login from "./paginas/Login";
export default function App() {
    //ágina actual: "terminal" o "login"
    const [pagina, setPagina]=useState("terminal");

    return(
        <>
            {pagina=== "terminal" && (
                <Terminal alIrALogin={() => setPagina("login")} />
            )}

            {pagina === "login" && (
                <Login alIniciarSesion={() => setPagina("terminal")} />
            )}
        </>
    );
}
