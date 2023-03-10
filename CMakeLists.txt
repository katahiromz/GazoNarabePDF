# CMakeLists.txt --- CMake project settings
#    ex) cmake -G "Visual Studio 9 2008" .
#    ex) cmake -DCMAKE_BUILD_TYPE=Release -G "MSYS Makefiles" .
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.0)

# project name and languages
project(GNPDF CXX RC)

# set output directory (build/)
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# statically link
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # using Clang
    set(CMAKE_CXX_FLAGS "-static -lstdc++ -lgcc")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # using GCC
    set(CMAKE_CXX_FLAGS "-static -lstdc++ -lgcc")
elseif (MSVC)
    # replace "/MD" with "/MT" (building without runtime DLLs)
    set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    foreach(CompilerFlags ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlags} "${${CompilerFlags}}")
    endforeach()
endif()

##############################################################################

# Shareware?
option(SHAREWARE "Enable shareware" ON)
if(NOT SHAREWARE)
    add_definitions(-DNO_SHAREWARE)
endif()

# Unicode
add_definitions(-DUNICODE -D_UNICODE)

# zlib
find_package(ZLIB)

# libpng
find_package(PNG)

# libharu (hpdf)
set(LIBHPDF_STATIC ON)
add_subdirectory(libharu)
include_directories(libharu/include)

# GNPDF.exe
add_executable(GNPDF WIN32 GNPDF.cpp GNPDF_res.rc gpimage.cpp Shareware.cpp SHA-256.cpp)
target_link_libraries(GNPDF comctl32 shlwapi gdiplus hpdf ${PNG_LIBRARY} ${ZLIB_LIBRARY})

##############################################################################
