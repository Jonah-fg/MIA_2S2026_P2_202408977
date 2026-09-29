import {useState, useEffect, useRef }from "react";
import { ejecutarComando, verificarBackend} from "../servicios/api";
import "../estilos/terminal.css";

export default function Terminal(){

    //Estados del componente
    const [entrada, setEntrada]= useState("");

    //Lista de bloques que se muestran en la salida.
    const [salidas, setSalidas]= useState([]);

    const [backendActivo, setBackendActivo] = useState(null);

    //eferencia al div de salida (para autoscroll)
    const refSalida= useRef(null);
    //efectos
    useEffect(()=> {verificarBackend().then(setBackendActivo);}, []);

    //Cada vez que llega una salida nueva, bajamos el scroll al fondo
    useEffect(() => {
        if(refSalida.current) {
            refSalida.current.scrollTop= refSalida.current.scrollHeight;
        }
    }, [salidas]);

    //manejadores de eventos
    async function manejarEjecutar(){
        const texto=entrada.trim();
        if (!texto) 
            return;

        //agregacion del comando al historial como bloque de entrada
        setSalidas(prev=> [...prev,{tipo: "entrada", texto }]);

        setEntrada("");

        //Llamar al backend y esperar respuestass
        const resultado =await ejecutarComando(texto);

        //Agregar la respuesta al historial
        setSalidas(prev =>[...prev,{
            tipo: resultado.success ? "exito" : "error",
            texto: resultado.message || "(sin mensaje)",
        }]);
    }

    function manejarTecla(evento) {
        if (evento.ctrlKey && evento.key=== "Enter") {
            manejarEjecutar();
        }
    }

    //Render
    return (
        <div className="terminal-contenedor">
            <div className="terminal-encabezado">
                <h1>MIA 2S2026 — Terminal</h1>

                <div className={`estado-backend ${backendActivo ? "activo" : "inactivo"}`}>
                    {backendActivo === null && "Verificando backend..."}
                    {backendActivo === true  && "Backend conectado"}
                    {backendActivo === false && "Backend NO disponible"}
                </div>
            </div>

            <div className="terminal-entrada">
                <label>Entrada:</label>
                <textarea
                    value={entrada}
                    onChange={(e) => setEntrada(e.target.value)}
                    onKeyDown={manejarTecla}
                    placeholder="Escribe uno o varios comandos. Ej: mkdisk -size=10 -unit=M -path=/tmp/d1.mia"
                    rows={6}
                />
                <button onClick={manejarEjecutar} className="boton-ejecutar">
                    Ejecutar (Ctrl+Enter)
                </button>
            </div>
            
            <div className="terminal-salida" ref={refSalida}>
                <label>Salida:</label>

                {salidas.length === 0 && (
                    <div className="salida-vacia">
                        Aquí se verán los resultados de los comandos.
                    </div>
                )}

                {salidas.map((bloque, i)=> (
                    <pre key={i} className={`bloque-${bloque.tipo}`}>
                        {bloque.tipo === "entrada" ? "> " : ""}
                        {bloque.texto}
                    </pre>
                ))}
            </div>
        </div>
    );
}