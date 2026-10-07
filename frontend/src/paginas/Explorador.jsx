import { useState, useEffect } from "react";

import {obtenerDiscos, obtenerParticiones,listarCarpeta, leerArchivo} from "../servicios/api";
import "../estilos/explorador.css";

export default function Explorador({alIrATerminal }){
    const [discos, setDiscos]= useState([]);
    const [nivel, setNivel] = useState("discos");
    const [particiones, setParticiones]=useState([]);
    const [items, setItems]= useState([]);

    const [discoActual, setDiscoActual] = useState(null);
     const [particionActual, setParticionActual]=useState(null);
    const [cargando, setCargando] =useState(true);
    const [pathActual, setPathActual] =useState("/");
    const [contenidoArchivo, setContenidoArchivo] = useState("");
    const [archivoAbierto, setArchivoAbierto]=useState(null);
    const [cargandoContenido, setCargandoContenido] =useState(false);


     useEffect(()=>{
        if (nivel=== "discos") {
            setCargando(true);
            obtenerDiscos().then((lista)=> {
                setDiscos(lista);
                setCargando(false);
            });
        }
    }, [nivel]);

    //Manejadores
    async function seleccionarDisco(disco) {
        setDiscoActual(disco);
        setCargando(true);
        const lista=await obtenerParticiones(disco);
        setParticiones(lista);
        setNivel("particiones");
        setCargando(false);
    }

    async function seleccionarParticion(part) {
        //particiones montadas y con sistema de arcivos 
        if (part.id=== ""){
            alert("Esta partición no está mntada. Ejecuta MOUNT primero.");
            return;
        }
        setParticionActual(part);
        setPathActual("/");
        setCargando(true);
        const lista =await listarCarpeta(part.id, "/");
        setItems(lista);
        setNivel("archivos");
        setCargando(false);
    }

    async function entrarCarpeta(item){
        const nuevoPath = pathActual=== "/" ? `/${item.name}` : `${pathActual}/${item.name}`;
        setCargando(true);
        const lista =await listarCarpeta(particionActual.id, nuevoPath);
        setPathActual(nuevoPath);
        setItems(lista);
        setCargando(false);
    }
    async function abrirArchivo(item) {
        const rutaCompleta=pathActual=== "/" ? `/${item.name}` : `${pathActual}/${item.name}`;

        setArchivoAbierto({nombre: item.name, size: item.size, perm: item.perm, path: rutaCompleta,});
        setContenidoArchivo("");
        setCargandoContenido(true);

        const contenido= await leerArchivo(particionActual.id, rutaCompleta);
        setContenidoArchivo(contenido);
        setCargandoContenido(false);
    }

    function subirNivel() {
        if (pathActual === "/") {
            return;
        }
        const partes= pathActual.split("/").filter(Boolean);
        partes.pop();
        const nuevo="/" + partes.join("/");
        setPathActual(nuevo);
        setCargando(true);
        listarCarpeta(particionActual.id, nuevo).then((lista)=> {
            setItems(lista);
            setCargando(false);
        });
    }

    function volverANivelAnterior() {
        if (nivel==="archivos"){
            setNivel("particiones");
            setParticionActual(null);
            setPathActual("/");
        } 
        else if(nivel=== "particiones"){
            setNivel("discos");
            setDiscoActual(null);
            setParticiones([]);
        }
    }

    function nombreDisco(ruta){
        return ruta.split("/").pop();
    }

    //renderr
    return (
        <div className="explorador-contenedor">
            <div className="explorador-header">
                <h1>Visualizador del Sistema de Archivos</h1>
                <div>
                    {nivel !=="discos" && (
                        <button className="boton-volver" onClick={volverANivelAnterior}>
                            ← Volver
                        </button>
                    )}
                    <button className="boton-volver" onClick={alIrATerminal}>
                        ←Terminal
                    </button>
                </div>
            </div>

            {/* Breadcrumb de contexto */}
            {discoActual&&(
                <div className="explorador-breadcrumb">
                    <span>Disco: <strong>{nombreDisco(discoActual)}</strong></span>
                    {particionActual &&(
                        <span> | Partición: <strong>{particionActual.name}</strong>({particionActual.id})</span>
                    )}
                    {nivel==="archivos"&& (
                        <span> | Ruta: <strong>{pathActual}</strong></span>
                    )}
                </div>
            )}

            {/*Nivel 1: discos*/}
            {nivel ==="discos" &&(
                <div className="explorador-seccion">
                    <h2>Seleccione el disco que desea visualizar:</h2>

                    {cargando && <p>Cargando discos...</p>}

                    {!cargando && discos.length ===0 &&(
                        <p className="explorador-vacio">
                            No hay discos disponiles. Cree uno con <code>mkdisk</code>.
                        </p>
                    )}

                    <div className="explorador-grid">
                        {discos.map((disco)=>(
                            <div
                                key={disco}
                                className="explorador-item"
                                onClick={()=>seleccionarDisco(disco)}
                            >
                                <div className="explorador-icono">💾</div>
                                <div className="explorador-nombre">{nombreDisco(disco)}</div>
                            </div>
                        ))}
                    </div>
                </div>
            )}

            {/*Nivel 2: particiones */}
            {nivel === "particiones"&&(
                <div className="explorador-seccion">
                    <h2>Seleccione la partición que desea visualizar:</h2>

                    {cargando && <p>Cargando particioes...</p>}

                    {!cargando && particiones.length ===0&&(
                        <p className="explorador-vacio">Este disco no tiene particiones.</p>
                    )}

                    <div className="explorador-grid">
                        {particiones.map((p, i) =>(
                            <div
                                key={i}
                                className="explorador-item"
                                onClick={()=>seleccionarParticion(p)}
                            >
                                <div className="explorador-icono">🗂️</div>
                                <div className="explorador-nombre">{p.name}</div>
                                <div className="explorador-meta">
                                    {p.type} | {Math.round(p.size / 1024)} KB
                                </div>
                                <div className="explorador-meta">
                                    {p.id ? `ID: ${p.id}`: "(sin montar)"}
                                </div>
                            </div>
                        ))}
                    </div>
                </div>
            )}

            {/*Nivel 3: archivos*/}
            {nivel ==="archivos" &&(
                <div className="explorador-seccion">
                    <div className="explorador-toolbar">
                        <button
                            className="boton-volver"
                            onClick={subirNivel}
                            disabled={pathActual=== "/"}
                        >
                            ⬆ Subir
                        </button>
                        <span className="explorador-path">{pathActual}</span>
                    </div>
                    {cargando && <p>Cargando contenido...</p>}

                    {!cargando && items.length ===0 &&(
                        <p className="explorador-vacio">Carpeta vacía.</p>
                    )}
                    <div className="explorador-grid">
                        {items.map((item, i)=> (
                            <div
                                key={i}
                                className="explorador-item"
                                onClick={()=>
                                    item.isFolder ? entrarCarpeta(item) : abrirArchivo(item)
                                }
                            >
                                <div className="explorador-icono">
                                    {item.isFolder ? "📁" : "📄"}
                                </div>
                                <div className="explorador-nombre">{item.name}</div>
                                <div className="explorador-meta">
                                    {item.perm}| {item.size} B
                                </div>
                            </div>
                        ))}
                    </div>
                </div>
            )}

            {/*Modal de archivo*/}
            {archivoAbierto &&(
                <div
                    className="explorador-modal-fondo"
                    onClick={() =>setArchivoAbierto(null)}
                >
                    <div
                        className="explorador-modal"
                        onClick={(e)=>e.stopPropagation()}
                    >
                        <h3>{archivoAbierto.nombre}</h3>
                        <p className="explorador-meta">
                            Ruta: {archivoAbierto.path}
                            <br />
                            Permisos:{archivoAbierto.perm}
                            <br />
                            Tamaño: {archivoAbierto.size}bytes
                        </p>

                        <h4 className="explorador-subtitulo">Contenido:</h4>
                        {cargandoContenido &&<p>Cargando...</p>}
                        {!cargandoContenido &&(
                            <pre className="explorador-contenido">
                                {contenidoArchivo || "(archivo vacío)"}
                            </pre>
                        )}
                        <button
                            className="explorador-boton"
                            onClick={()=> setArchivoAbierto(null)}
                        >
                            Cerrar
                        </button>
                    </div>
                </div>
            )}
        </div>
    );
}