cmake_minimum_required(VERSION 2.8.12)
project(pycombinae)

# Set source directory
set(SOURCE_DIR "pycombina")

# Tell CMake that headers are also in SOURCE_DIR
include_directories(${SOURCE_DIR})
set(SOURCES "${SOURCE_DIR}/cia.cpp")

# Generate Python module
add_subdirectory(lib/pybind11)
pybind11_add_module(pycombina ${SOURCES} "${SOURCE_DIR}/pycombina.cpp")