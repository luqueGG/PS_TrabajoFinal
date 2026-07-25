# psadmin

Herramienta de administración de Linux en C, con interfaz de 4 paneles en modo
raw (ANSI puro, sin ncurses), inspirada en layouts tipo vim.

```
+--------+-----------------+--------+
|  tsk   |       con        |  shl   |
|        |-------------------|        |
|        |       cli         |        |
+--------+-----------------+--------+
```

## Módulos

| Módulo | Carpeta/archivo | Responsabilidad |
|---|---|---|
| **shl** (PSShell) | `src/shl.c` | Explorador de archivos del directorio del usuario + respaldo automático (`tar.gz` con timestamp) |
| **tsk** (AdminTasks) | `src/tsk.c` | Lista procesos vía `/proc`, búsqueda, %CPU/MEM, kill/SIGKILL/stop/cont, árbol de procesos |
| **cli** (PSCLI) | `src/cli.c` | Consola embebida **real**: `forkpty()` + `bash` interactivo, con I/O no bloqueante |
| **con** (PSCon) | `src/con.c` | Analiza un script bash (seleccionado en `shl`) y detecta variables y ciclos. Diseñado como tabla de "analizadores" extensible (sumar uno nuevo = escribir 1 función + 1 línea en la tabla) |
| **viz** (Visualizer) | `src/viz.c` | Raw mode de terminal (termios) + cálculo de layout de los 4 paneles + dibujo con escape codes ANSI |
| (integrador) | `src/main.c` | Loop principal: foco rotativo entre paneles, lectura de teclado, refresco periódico de stats, reenvío de teclas al PTY cuando el foco está en `cli` |

## Build

Requiere `cmake`, `ninja`, `libncurses-dev` (solo para herramientas del sistema,
no se usa ncurses en el código), `check` (tests) y libutil (viene con glibc,
para `forkpty`).

```bash
# Ubuntu/Debian
sudo apt-get install cmake ninja-build check pkg-config

cmake -G Ninja -B build -S .
ninja -C build
```

Esto genera:
- `build/psadmin` — el binario principal
- `build/tests/psadmin_tests` — la suite de tests unitarios

## Tests

```bash
cd build
ctest --output-on-failure
# o directamente:
./tests/psadmin_tests
```

Los tests cubren la **lógica pura** de cada módulo (parsing de `/proc/[pid]/stat`,
análisis de bash, layout de paneles, buffer circular del PTY, listado/orden de
directorios, navegación), evitando testear el renderizado ANSI o la terminal
real, que se valida manualmente / con PTY simulado.

## Uso

```bash
./build/psadmin
```

| Tecla | Acción |
|---|---|
| `Tab` | Rotar foco entre paneles (tsk → con → shl → cli → tsk) |
| `j` / `k` | Mover selección dentro del panel con foco (tsk: proceso, shl: archivo) |
| `Enter` (en shl) | Entrar al directorio seleccionado |
| `a` (en shl) | Analizar el script bash seleccionado → resultado en panel `con` |
| `b` (en shl) | Crear respaldo (`tar.gz`) de la entrada seleccionada en `~/.psadmin_backups` |
| `d` (en shl) | Eliminar la entrada seleccionada (recursivo si es directorio) |
| `c` / `m` (en shl) | Marcar la entrada seleccionada para copiar / mover |
| `p` (en shl) | Pegar (copiar/mover) lo marcado en el directorio actual |
| `/` (en shl) | Buscar archivo/directorio por nombre en el directorio actual |
| `i` (en shl) | Mostrar estadísticas (tipo, permisos, tamaño, fecha) de la selección |
| `x` (en tsk) | Terminar proceso seleccionado (SIGTERM) |
| `X` (en tsk) | Forzar terminación (SIGKILL) |
| `s` (en tsk) | Suspender proceso (SIGSTOP) |
| `r` (en tsk) | Reanudar proceso (SIGCONT) |
| `/` (en tsk) | Buscar proceso por nombre |
| (foco en cli) | Todo el teclado se reenvía directo al `bash` real corriendo dentro |
| `q` | Salir (fuera de modo búsqueda) |

