# Manual de Usuario \- PSAdmin

## *Programación de Sistemas A*

*Bravo Cristhian, Luque Fernando*

# Introducción

## Propósito

Un software de administración de procesos, gestión de archivos integrado con una línea de comandos y análisis de scripts *bash*.

## Alcance

La ejecución del programa es exclusiva de *Linux*. 

Exclusión del proyecto como la no inclusión de *GUI*.

Los entregables incluyen el código fuente completo alojado en un repositorio de *GitHub*, cumpliendo con las especificaciones correspondientes como un Makefile funcional.

# Requisitos del Sistema

1. Sistema Operativo: Linux  
2. Lenguaje: C11  
3. Make 4.3  
4. CMake 3.28.3  
5. Ninja 1.11.1  
6. Check (opcional, para tests)

# Instalación

Clonar el repositorio localmente.

```
git clone https://github.com/luqueGG/PS_TrabajoFinal.git
```

Otra opción es descargarlo como zip.

Compilar el proyecto.

```
cd PS_TrabajoFinal/
make
```

Ejecutar psadmin.

```
cd build/
./psadmin
```

# Módulos

*shl:* Explorador de archivos del directorio del usuario.

*tsk:* Es el administrador de tareas.

*cli:* Consola embebida con soporte bash.

*shl:* Analizador bash scripts

# Uso 

| *Tecla* | *Acción* |
| ----- | ----- |
| *`Tab`* | *Rotar foco entre paneles (tsk → con → shl → cli → tsk)* |
| *`j` / `k`* | *Mover selección dentro del panel con foco (tsk: proceso, shl: archivo)* |
| *`Enter` (en shl)* | *Entrar al directorio seleccionado* |
| *`a` (en shl)* | *Analizar el script bash seleccionado → resultado en panel `con`* |
| *`b` (en shl)* | *Crear respaldo (`tar.gz`) de la entrada seleccionada en `~/.psadmin_backups`* |
| *`d` (en shl)* | *Eliminar la entrada seleccionada (recursivo si es directorio)* |
| *`c` / `m` (en shl)* | *Marcar la entrada seleccionada para copiar / mover* |
| *`p` (en shl)* | *Pegar (copiar/mover) lo marcado en el directorio actual* |
| *`/` (en shl)* | *Buscar archivo/directorio por nombre en el directorio actual* |
| *`i` (en shl)* | *Mostrar estadísticas (tipo, permisos, tamaño, fecha) de la selección* |
| *`x` (en tsk)* | *Terminar proceso seleccionado (SIGTERM)* |
| *`X` (en tsk)* | *Forzar terminación (SIGKILL)* |
| *`s` (en tsk)* | *Suspender proceso (SIGSTOP)* |
| *`r` (en tsk)* | *Reanudar proceso (SIGCONT)* |
| *`/` (en tsk)* | *Buscar proceso por nombre* |
| *(foco en cli)* | *Todo el teclado se reenvía directo al `bash` real corriendo dentro* |
| *`q`* | *Salir (fuera de modo búsqueda)* |

# Anexos

*Anexo 01: Repositorio GitHub*

[*https://github.com/luqueGG/PS\_TrabajoFinal*](https://github.com/luqueGG/PS_TrabajoFinal) 
