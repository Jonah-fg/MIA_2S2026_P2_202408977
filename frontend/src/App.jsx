import Terminal from "./paginas/Terminal";
import { useState } from "react";
import Login from "./paginas/Login";
import Explorador from "./paginas/Explorador";
import Journaling from "./paginas/Journaling";
export default function App() {
    //ágina actual: "terminal" o "login"
    const [pagina, setPagina]=useState("terminal");

    return(
        <>
            {pagina=== "terminal" && (
                <Terminal
                 alIrALogin={() => setPagina("login")}
                 alIrAExplorador={() => setPagina("explorador")}
                alIrAJournaling={()=>setPagina("journaling")}
                />
            )}
            {pagina === "login" && (
                <Login alIniciarSesion={() => setPagina("terminal")} />
            )}
            {pagina === "explorador" && (
                <Explorador alIrATerminal={()=> setPagina("terminal")}/>
            )}
            {pagina === "journaling" && (
                <Journaling alIrATerminal={()=>setPagina("terminal")}/>
            )}
        </>
    );
}
