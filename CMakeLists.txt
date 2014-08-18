cmake_minimum_required(VERSION 2.8)
project(rtosc-tutorial)
set(VERSION "0.1")

#Assume That ALL Deps Can be Found
find_package(FLTK                   REQUIRED)
find_package(PkgConfig              REQUIRED)
pkg_check_modules(RTOSC librtosc    REQUIRED)
pkg_check_modules(JACK  jack        REQUIRED)
pkg_check_modules(LIBLO liblo>=0.26 REQUIRED)

find_program(ASCIIDOC asciidoctor)
find_program(BLOCKDIAG blockdiag)

if(NOT ASCIIDOC)
    message(FATAL_ERROR "Could not find asciidoctor")
endif()

if(NOT BLOCKDIAG)
    message(FATAL_ERROR "Could not find blockdiag")
endif()

#Define Build Ops
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -std=c11")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++11")


#Build Executables
add_executable(01-tutorial 01-basic-jack.c)
add_executable(02-tutorial 02-basic-app.c)
add_executable(03-tutorial 03-basic-rtosc.cpp)
add_executable(04-tutorial 04-macro-rtosc.cpp)
add_executable(05-tutorial 05-lo-rtosc.cpp 05-middleware)
add_executable(06-tutorial 06-documented-rtosc.cpp 06-middleware)


#Linker Flags
target_link_libraries(01-tutorial ${JACK_LIBRARIES})
target_link_libraries(02-tutorial ${JACK_LIBRARIES} m)
target_link_libraries(03-tutorial ${JACK_LIBRARIES} ${RTOSC_LIBRARIES} rtosc-cpp)
target_link_libraries(04-tutorial ${JACK_LIBRARIES} ${RTOSC_LIBRARIES} rtosc-cpp)
target_link_libraries(05-tutorial ${JACK_LIBRARIES} ${RTOSC_LIBRARIES} rtosc-cpp
                                  ${LIBLO_LIBRARIES})
target_link_libraries(06-tutorial ${JACK_LIBRARIES} ${RTOSC_LIBRARIES} rtosc-cpp
                                  ${LIBLO_LIBRARIES})


#Build Documentation (TODO)
