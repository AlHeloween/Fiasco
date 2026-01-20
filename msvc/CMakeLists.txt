cmake_minimum_required(VERSION 3.20)
project(fiasco VERSION 1.0 LANGUAGES C)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# MSVC-specific settings
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_CRT_NONSTDC_NO_DEPRECATE)
    add_compile_options(/W3)
    # For release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /Ob2)
    endif()
endif()

# Include directories
set(FIASCO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FIASCO_ROOT}
    ${FIASCO_ROOT}/lib
    ${FIASCO_ROOT}/codec
    ${FIASCO_ROOT}/input
    ${FIASCO_ROOT}/output
    ${FIASCO_ROOT}/bin
)

# Library sources
set(LIB_SOURCES
    ${FIASCO_ROOT}/lib/arith.c
    ${FIASCO_ROOT}/lib/bit-io.c
    ${FIASCO_ROOT}/lib/dither.c
    ${FIASCO_ROOT}/lib/error.c
    ${FIASCO_ROOT}/lib/image.c
    ${FIASCO_ROOT}/lib/list.c
    ${FIASCO_ROOT}/lib/misc.c
    ${FIASCO_ROOT}/lib/rpf.c
)

# Codec sources
set(CODEC_SOURCES
    ${FIASCO_ROOT}/codec/approx.c
    ${FIASCO_ROOT}/codec/bintree.c
    ${FIASCO_ROOT}/codec/coder.c
    ${FIASCO_ROOT}/codec/coeff.c
    ${FIASCO_ROOT}/codec/control.c
    ${FIASCO_ROOT}/codec/decoder.c
    ${FIASCO_ROOT}/codec/dfiasco.c
    ${FIASCO_ROOT}/codec/domain-pool.c
    ${FIASCO_ROOT}/codec/ip.c
    ${FIASCO_ROOT}/codec/motion.c
    ${FIASCO_ROOT}/codec/mwfa.c
    ${FIASCO_ROOT}/codec/options.c
    ${FIASCO_ROOT}/codec/prediction.c
    ${FIASCO_ROOT}/codec/subdivide.c
    ${FIASCO_ROOT}/codec/tiling.c
    ${FIASCO_ROOT}/codec/wfalib.c
)

# Input sources
set(INPUT_SOURCES
    ${FIASCO_ROOT}/input/basis.c
    ${FIASCO_ROOT}/input/matrices.c
    ${FIASCO_ROOT}/input/mc.c
    ${FIASCO_ROOT}/input/nd.c
    ${FIASCO_ROOT}/input/read.c
    ${FIASCO_ROOT}/input/tree.c
    ${FIASCO_ROOT}/input/weights.c
)

# Output sources
set(OUTPUT_SOURCES
    ${FIASCO_ROOT}/output/matrices.c
    ${FIASCO_ROOT}/output/mc.c
    ${FIASCO_ROOT}/output/nd.c
    ${FIASCO_ROOT}/output/tree.c
    ${FIASCO_ROOT}/output/weights.c
    ${FIASCO_ROOT}/output/write.c
)

# All library sources combined
set(ALL_SOURCES
    ${LIB_SOURCES}
    ${CODEC_SOURCES}
    ${INPUT_SOURCES}
    ${OUTPUT_SOURCES}
)

# Static library
add_library(fiasco_static STATIC ${ALL_SOURCES})
target_compile_definitions(fiasco_static PRIVATE FIASCO_STATIC)
set_target_properties(fiasco_static PROPERTIES OUTPUT_NAME "fiasco")

# Shared library (DLL)
add_library(fiasco_shared SHARED ${ALL_SOURCES})
target_compile_definitions(fiasco_shared PRIVATE FIASCO_BUILD_DLL)
set_target_properties(fiasco_shared PROPERTIES OUTPUT_NAME "fiasco_dll")

# Encoder executable (cwfa)
add_executable(cwfa
    ${FIASCO_ROOT}/bin/cwfa.c
    ${FIASCO_ROOT}/bin/params.c
    ${FIASCO_ROOT}/bin/getopt.c
    ${FIASCO_ROOT}/bin/getopt1.c
    ${FIASCO_ROOT}/bin/binerror.c
)
target_link_libraries(cwfa fiasco_static)

# Decoder executable (dwfa)
add_executable(dwfa
    ${FIASCO_ROOT}/bin/dwfa.c
    ${FIASCO_ROOT}/bin/params.c
    ${FIASCO_ROOT}/bin/getopt.c
    ${FIASCO_ROOT}/bin/getopt1.c
    ${FIASCO_ROOT}/bin/binerror.c
)
target_link_libraries(dwfa fiasco_static)

# Install targets
install(TARGETS fiasco_static fiasco_shared cwfa dwfa
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${FIASCO_ROOT}/fiasco.h DESTINATION include)

# Python bindings (pybind11)
find_package(Python3 COMPONENTS Interpreter Development)
if(Python3_FOUND)
    # Try to find pybind11
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import pybind11; print(pybind11.get_cmake_dir())"
        OUTPUT_VARIABLE pybind11_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    if(pybind11_DIR)
        find_package(pybind11 CONFIG)
        if(pybind11_FOUND)
            message(STATUS "Found pybind11, building Python bindings")
            
            pybind11_add_module(pyfiasco
                ${CMAKE_CURRENT_SOURCE_DIR}/pyfiasco.cpp
            )
            target_link_libraries(pyfiasco PRIVATE fiasco_static)
            target_compile_definitions(pyfiasco PRIVATE FIASCO_STATIC)
            
            # Install Python module
            install(TARGETS pyfiasco
                LIBRARY DESTINATION ${Python3_SITELIB}
            )
        endif()
    endif()
endif()

# Summary
message(STATUS "FIASCO build configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
