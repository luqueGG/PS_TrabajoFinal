cmake_minimum_required(VERSION 3.16)
project(psadmin C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()


include_directories(${CMAKE_SOURCE_DIR}/include)

# --- librería core: toda la lógica de los 5 módulos, sin el main() ---
# Se separa en una librería estática para poder linkearla tanto desde el
# binario final como desde los tests (check) sin duplicar símbolos main.
add_library(psadmin_core STATIC
    src/shl.c
    src/tsk.c
    src/ccon.c
    src/cli.c
    src/viz.c
)
target_include_directories(psadmin_core PUBLIC ${CMAKE_SOURCE_DIR}/include)

# cli.c usa forkpty() de libutil
target_link_libraries(psadmin_core PUBLIC util)

# --- binario principal ---
add_executable(psadmin src/main.c)
target_link_libraries(psadmin PRIVATE psadmin_core)

# --- tests (opcional, requiere libcheck) ---
option(PSADMIN_BUILD_TESTS "Build unit tests with libcheck" ON)
if(PSADMIN_BUILD_TESTS)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(CHECK QUIET check)
  endif()
  if(CHECK_FOUND)
    enable_testing()
    add_subdirectory(tests)
  else()
    message(WARNING "libcheck no encontrado: se omiten los tests (PSADMIN_BUILD_TESTS=ON pero CHECK_FOUND=OFF)")
  endif()
endif()
