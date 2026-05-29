cmake_minimum_required(VERSION 3.10)
project(FHE_ControlFlow)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(USE_EMP_GC "Use EMP-toolkit half-gates backend for garbled circuits" OFF)

# Find OpenFHE
find_package(OpenFHE REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenFHE_INCLUDE}
    ${OpenFHE_INCLUDE}/third-party/include
    ${OpenFHE_INCLUDE}/core
    ${OpenFHE_INCLUDE}/pke
    ${OpenFHE_INCLUDE}/binfhe
)

link_directories(${OpenFHE_LIBDIR})
if(APPLE AND EXISTS "/opt/homebrew/opt/libomp/lib")
    link_directories("/opt/homebrew/opt/libomp/lib")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenFHE_CXX_FLAGS}")

set(GC_BACKEND_SOURCES)
if(USE_EMP_GC)
    find_path(EMP_TOOL_INCLUDE_DIR emp-tool/emp-tool.h)
    find_library(EMP_TOOL_LIBRARY emp-tool)
    if(NOT EMP_TOOL_INCLUDE_DIR OR NOT EMP_TOOL_LIBRARY)
        message(FATAL_ERROR "USE_EMP_GC=ON requires EMP-toolkit headers and libemp-tool.")
    endif()
    include_directories(${EMP_TOOL_INCLUDE_DIR})
    add_definitions(-DUSE_EMP_GC)
    list(APPEND GC_BACKEND_SOURCES src/gc/emp_garbled_circuit.cpp)
endif()

# Add the main executable
add_executable(demo_loop 
    examples/demo_loop.cpp
    src/fhe/fhe_context.cpp
    src/gates/fhe_gates.cpp
    src/gc/boolean_circuit.cpp
    src/gc/boolean_circuit_export.cpp
    src/gc/controlled_reveal_circuit.cpp
    src/gc/garbled_predicate_evaluator.cpp
    src/gc/minimal_garbled_circuit.cpp
    src/gc/openfhe_lwe_decryption_circuit.cpp
    ${GC_BACKEND_SOURCES}
    src/algorithms/fhe_arithmetic.cpp
    src/algorithms/fhe_cmp.cpp
)

target_link_libraries(demo_loop 
    ${OpenFHE_LIBRARIES}
)

add_executable(generate_openfhe_keypair
    tools/generate_openfhe_keypair.cpp
)

target_link_libraries(generate_openfhe_keypair
    ${OpenFHE_LIBRARIES}
)

add_executable(prepare_openfhe_constants
    tools/prepare_openfhe_constants.cpp
)

target_link_libraries(prepare_openfhe_constants
    ${OpenFHE_LIBRARIES}
)

add_executable(setup_fixed_gc_material
    tools/setup_fixed_gc_material.cpp
    src/fhe/fhe_context.cpp
    src/gates/fhe_gates.cpp
    src/gc/active_garbled_circuit_io.cpp
    src/gc/boolean_circuit.cpp
    src/gc/boolean_circuit_export.cpp
    src/gc/boolean_circuit_io.cpp
    src/gc/controlled_reveal_circuit.cpp
    src/gc/garbled_predicate_evaluator.cpp
    src/gc/minimal_garbled_circuit.cpp
    src/gc/openfhe_lwe_decryption_circuit.cpp
    ${GC_BACKEND_SOURCES}
)

target_link_libraries(setup_fixed_gc_material
    ${OpenFHE_LIBRARIES}
)

add_executable(evaluator_runtime_material_check
    tools/evaluator_runtime_material_check.cpp
)

target_link_libraries(evaluator_runtime_material_check
    ${OpenFHE_LIBRARIES}
)

add_executable(setup_openfhe_lwe_int_gc_material
    tools/setup_openfhe_lwe_int_gc_material.cpp
    src/gc/active_garbled_circuit_io.cpp
    src/gc/boolean_circuit.cpp
    src/gc/boolean_circuit_export.cpp
    src/gc/boolean_circuit_io.cpp
    src/gc/minimal_garbled_circuit.cpp
    src/gc/openfhe_lwe_int_decrypt_compare_circuit.cpp
    src/gc/openfhe_lwe_int_material.cpp
    ${GC_BACKEND_SOURCES}
)

add_executable(openfhe_lwe_int_runtime_demo
    examples/openfhe_lwe_int_runtime_demo.cpp
    src/gc/active_garbled_circuit_io.cpp
    src/gc/boolean_circuit.cpp
    src/gc/boolean_circuit_io.cpp
    src/gc/minimal_garbled_circuit.cpp
    src/gc/openfhe_lwe_int_material.cpp
    ${GC_BACKEND_SOURCES}
)

add_executable(audit_openfhe_lwe_int_runtime_boundary
    tools/audit_openfhe_lwe_int_runtime_boundary.cpp
    src/gc/active_garbled_circuit_io.cpp
    src/gc/boolean_circuit.cpp
    src/gc/boolean_circuit_io.cpp
    src/gc/openfhe_lwe_int_material.cpp
    ${GC_BACKEND_SOURCES}
)

add_executable(export_openfhe_eval_key_material
    tools/export_openfhe_eval_key_material.cpp
)

target_link_libraries(export_openfhe_eval_key_material
    ${OpenFHE_LIBRARIES}
)

add_executable(export_openfhe_lwe_int_material
    tools/export_openfhe_lwe_int_material.cpp
)

target_link_libraries(export_openfhe_lwe_int_material
    ${OpenFHE_LIBRARIES}
)

add_executable(openfhe_controlled_reveal_reference_test
    tests/openfhe_controlled_reveal_reference_test.cpp
)

add_executable(openfhe_lwe_int_decrypt_compare_circuit_test
    tests/openfhe_lwe_int_decrypt_compare_circuit_test.cpp
    src/gc/boolean_circuit.cpp
    src/gc/boolean_circuit_export.cpp
    src/gc/openfhe_lwe_int_decrypt_compare_circuit.cpp
    src/gc/openfhe_lwe_int_material.cpp
)

add_executable(openfhe_lwe_int_decrypt_compare_gc_test
    tests/openfhe_lwe_int_decrypt_compare_gc_test.cpp
    src/gc/boolean_circuit.cpp
    src/gc/minimal_garbled_circuit.cpp
    src/gc/openfhe_lwe_int_decrypt_compare_circuit.cpp
    src/gc/openfhe_lwe_int_material.cpp
    ${GC_BACKEND_SOURCES}
)

add_executable(dump_openfhe_eval_bin_gate_circuit
    tools/dump_openfhe_eval_bin_gate_circuit.cpp
    src/gc/boolean_circuit.cpp
    src/gc/boolean_circuit_export.cpp
    src/gc/openfhe_eval_bin_gate_circuit.cpp
    src/gc/openfhe_lwe_decryption_circuit.cpp
)

target_link_libraries(dump_openfhe_eval_bin_gate_circuit
    ${OpenFHE_LIBRARIES}
)

if(USE_EMP_GC)
    foreach(GC_TARGET demo_loop setup_fixed_gc_material setup_openfhe_lwe_int_gc_material openfhe_lwe_int_runtime_demo audit_openfhe_lwe_int_runtime_boundary openfhe_lwe_int_decrypt_compare_gc_test)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
            target_compile_options(${GC_TARGET} PRIVATE -march=armv8-a+simd+crypto+crc)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$")
            target_compile_options(${GC_TARGET} PRIVATE -maes -msse4.1)
        endif()
        target_link_libraries(${GC_TARGET} ${EMP_TOOL_LIBRARY} ssl crypto pthread)
    endforeach()
endif()
