import {useState } from "react";
import {ejecutarComando} from "../servicios/api";
import { useSesion } from "../contexto/SesionContext";
import "../estilos/login.css";

export default function Login({ alIniciarSesion }) {
    //Campos del formulario
    const [idParticion, setIdParticion] = useState("");
    const [usuario, setUsuario] =useState("");
    const [contrasena, setContrasena]= useState("");
    const [recordar, setRecordar]=useState(false);

    const [mensajeError, setMensajeError]= useState("");
    const [enviando, setEnviando] =useState(false);

    //acceso al contexto de sesión
    const { iniciarSesion } = useSesion();

    // Envío del formulario
    async function manejarSubmit(evento){
        evento.preventDefault(); 
        setMensajeError("");

        if (!idParticion.trim() || !usuario.trim() || !contrasena.trim()) {
            setMensajeError("Todos los campos son obligtorios.");
            return;
        }

        setEnviando(true);

        //construccion delcomando que entiende el backend
        const comando =`login -id=${idParticion.trim()} -user=${usuario.trim()} -pass=${contrasena}`;
        const resultado= await ejecutarComando(comando);

        setEnviando(false);

        if (!resultado.success){
            //backend rechazo login
            setMensajeError(resultado.message || "Usuario o contraseña incorrectos.");
            return;
        }

        // Login exitoso
        //Guardamos la sesión en el contexto
        iniciarSesion({usuario: usuario.trim(), idParticion: idParticion.trim(),});

        //Si marcó "Recordar usuario", guardamos solo el usuario
        if (recordar){
            localStorage.setItem("mia_usuario_recordado", usuario.trim());
        }
        else{
            localStorage.removeItem("mia_usuario_recordado");
        }

        if(alIniciarSesion) 
            alIniciarSesion();
    }

    //Render
    return(
        <div className="login-contenedor">
            <h1 className="login-titulo">Login</h1>

            <form onSubmit={manejarSubmit} className="login-formulario">
                <div className="login-campo">
                    <label htmlFor="idParticion">ID Partición</label>
                    <input
                        id="idParticion"
                        type="text"
                        value={idParticion}
                        onChange={(e)=> setIdParticion(e.target.value)}
                        placeholder="341A"
                        autoComplete="off"
                    />
                </div>
                <div className="login-campo">
                    <label htmlFor="usuario">Usuario</label>
                    <input
                        id="usuario"
                        type="text"
                        value={usuario}
                        onChange={(e) =>setUsuario(e.target.value)}
                        placeholder="root"
                        autoComplete="off"
                    />
                </div>

                <div className="login-campo">
                    <label htmlFor="contrasena">Contraseña</label>
                    <input
                        id="contrasena"
                        type="password"
                        value={contrasena}
                        onChange={(e)=> setContrasena(e.target.value)}
                        placeholder="••••••••"
                    />
                </div>

                <div className="login-recordar">
                    <input
                        id="recordar"
                        type="checkbox"
                        checked={recordar}
                        onChange={(e) => setRecordar(e.target.checked)}
                    />
                    <label htmlFor="recordar">Recordar usuario</label>
                </div>

                {mensajeError && (
                    <div className="login-error">{mensajeError}</div>
                )}
                <button type="submit" className="login-boton" disabled={enviando}> {enviando ?"Iniciando..." : "Submit"}
                </button>
            </form>
        </div>
    );
}