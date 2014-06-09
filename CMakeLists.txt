project(nanoclone C CXX)
cmake_minimum_required(VERSION 2.6.3)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package(nanomsg REQUIRED)

include_directories(BEFORE ${NANOMSG_INCLUDE_DIR})

if ( NOT CMAKE_BUILD_TYPE )
    message(STATUS "Defaulting to 'RelWithDebInfo' build configuration.")
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

set(CMAKE_C_FLAGS         "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS       "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

add_executable(nanoclone
               main.cpp
               client.cpp
               server.cpp
               frontend.cpp
               frontend.hpp
               backend.cpp
               backend.hpp
               messages.cpp
               messages.hpp
               type_aliases.hpp
               util.cpp
               util.hpp
)
target_link_libraries(nanoclone ${NANOMSG_LIBRARY})

if ( CMAKE_BUILD_TYPE )
    string(TOUPPER ${CMAKE_BUILD_TYPE} BuildType)
endif ()

message(
    "\n=================|  nanoclone Config Summary  |=================="
    "\nBuild Type:      ${CMAKE_BUILD_TYPE}"
    "\n"
    "\nCC:              ${CMAKE_C_COMPILER}"
    "\nCFLAGS:          ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${BuildType}}"
    "\nCXX:             ${CMAKE_CXX_COMPILER}"
    "\nCXXFLAGS:        ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${BuildType}}"
    "\n================================================================"
)
