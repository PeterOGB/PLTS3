cmake_minimum_required(VERSION 2.8.0)

PROJECT("803 Paperless Tape Station 3" C)

ADD_EXECUTABLE(PLTS3 PLTS3.c Logging.c)


SET(CMAKE_C_FLAGS "-std=gnu99  -g  -Wall -Wextra -Wunused -Wconversion -Wundef -Wcast-qual -Wmissing-prototypes "
"-Wredundant-decls -Wunreachable-code -Wwrite-strings -Warray-bounds"
"-Wstrict-aliasing=3 -Wstrict-overflow=1 -Wstrict-prototypes -Winline"
"-Wshadow -Wswitch -Wmissing-include-dirs -Woverlength-strings -Wpacked"
"-Wdisabled-optimization  -Wformat=2 -Winit-self"
"-Wunused-parameter -Wlogical-op -Wuninitialized"
"-Wnested-externs -Wpointer-arith -Wdouble-promotion -Wunused-macros"
"-Wunsafe-loop-optimizations -rdynamic -fdiagnostics-color")

STRING(REGEX REPLACE ";" " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

find_package( PkgConfig REQUIRED)


pkg_check_modules ( GTK REQUIRED gtk+-3.0 )
include_directories(${GTK_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GTK_LIBRARIES})

pkg_check_modules ( GLIB REQUIRED glib-2.0 )
include_directories(${GLIB_INCLUDE_DIRS})
set(GLIBS ${GLIBS} ${GLIB_LIBRARIES})


target_link_libraries(PLTS3 ${LIBS} )


