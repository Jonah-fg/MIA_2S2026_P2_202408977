import { useState, useEffect } from "react";
import { obtenerJournal } from "../servicios/api";
import { useSesion } from "../contexto/SesionContext";
import "../estilos/journaling.css";

export default function Journaling({ alIrATerminal }) {
    const { idParticion }= useSesion();
    const [entradas, setEntradas] = useState([]);
    const [cargando, setCargando]=useState(true);
    const [error, setError] =useState("");

    function formatearFecha(unixTime){
        const fecha = new Date(unixTime * 1000);
        const dia=String(fecha.getDate()).padStart(2, "0");
        const mes= String(fecha.getMonth() +1).padStart(2, "0");
        const anio =fecha.getFullYear();
        const hora=String(fecha.getHours()).padStart(2, "0");
        const min= String(fecha.getMinutes()).padStart(2, "0");
        return `${dia}/${mes}/${anio} ${hora}:${min}`;
    }

    useEffect(() =>{
        if (!idParticion){
            setError("No hay sesión activa.");
            setCargando(false);
            return;
        }
        obtenerJournal(idParticion).then((lista) => {
            setEntradas(lista);
            if(lista.length === 0){
                setError("No hay operaciones registadas o la partición no es EXT3.");
            }
            setCargando(false);
        });
    },
    [idParticion]);


    return(
        <div className="journaling-contenedor">
            <div className="journaling-header">
                <h1>Journaling</h1>
                <button className="boton-volver" onClick={alIrATerminal}>
                    ← Terminal
                </button>
            </div>

            <div className="journaling-info">
                Partición: <strong>{idParticion}</strong> | Total:{" "}
                <strong>{entradas.length}</strong> operaciones
            </div>

            {cargando &&<p>Cargando journaling...</p>}
            {!cargando&&error && <p className="journaling-error">{error}</p>}

            {!cargando && !error &&(
                <div className="journaling-tabla-contenedor">
                    <table className= "journaling-tabla">
                        <thead>
                            <tr>
                                <th>No.</th>
                                <th>Operación</th>
                                <th>Path</th>
                                <th>Contenido</th>
                                <th>Fecha</th>
                            </tr>
                        </thead>
                        <tbody>
                            {entradas.map((e)=> (
                                <tr key={e.no}>
                                    <td>{e.no}</td>
                                    <td>
                                        <span className="journaling-badge">
                                            {e.operation}
                                        </span>
                                    </td>
                                    <td className="journaling-path">{e.path}</td>
                                    <td className="journaling-content">
                                        {e.content||"—"}
                                    </td>
                                    <td>{formatearFecha(e.date)}</td>
                                </tr>
                            ))}
                        </tbody>
                    </table>
                </div>
            )}
        </div>
    );
}