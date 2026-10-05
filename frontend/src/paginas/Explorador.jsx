import { useState, useEffect } from "react";
import { obtenerDiscos } from"../servicios/api";
import "../estilos/explorador.css";

export default function Explorador({alIrATerminal }){
    const [discos, setDiscos] = useState([]);
    const [discoSeleccionado, setDiscoSeleccionado] = useState("");
    const [cargando, setCargando] = useState(true);

    useEffect(()=> {
        obtenerDiscos().then((lista) => {
            setDiscos(lista);
            setCargando(false);
        });
    },[]);

    function nombreDisco(ruta){
        return ruta.split("/").pop();
    }
    return (
        <div className="explorador-contenedor">
            <div className="explorador-header">
                <h1>Visualizador del Sistema de Archivos</h1>
                <button className="boton-volver" onClick={alIrATerminal}>
                    ← Volver a la Termnal
                </button>
            </div>

            <div className="explorador-seccion">
                <h2>Seleccione el disco que desea visualizar:</h2>
                {cargando && <p>Cargando discos...</p>}

                {!cargando && discos.length=== 0 &&(
                    <p className="explorador-vacio">
                        No hay discos diponibles. Cree uno con <code>mkdisk</code>.
                    </p>
                )}

                <div className="explorador-grid">
                    {discos.map((disco)=> (
                        <div
                            key={disco}
                            className={`explorador-item ${discoSeleccionado === disco ? "seleccionado" : ""}`}
                            onClick={()=>setDiscoSeleccionado(disco)}
                        >
                            <div className="explorador-icono">💾</div>
                            <div className="explorador-nombre">{nombreDisco(disco)}</div>
                        </div>
                    ))}
                </div>
            </div>

            {discoSeleccionado &&(
                <div className="explorador-seccion">
                    <p>
                        Disco seleccionado:<strong>{discoSeleccionado}</strong>
                    </p>
                    <button className="explorador-boton" disabled>
                        Ver particiones 
                    </button>
                </div>
            )}
        </div>
    );
}