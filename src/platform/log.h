// Registro de arranque/cierre en un fichero junto al EBOOT (fortnitepsp_log.txt).
//
// Sirve para diagnosticar en hardware real: si la aplicacion se cierra, el fichero
// indica hasta donde llego. Solo se escribe en arranque y cierre, nunca por frame.
#pragma once

namespace platform {
namespace log {

// argv0 = ruta del EBOOT (argv[0]); el log se crea en su misma carpeta.
void open(const char* argv0);
void write(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void close();

}  // namespace log
}  // namespace platform
