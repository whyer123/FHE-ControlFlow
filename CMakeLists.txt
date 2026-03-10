cmake_minimum_required(VERSION 3.10)
project(FHE_ControlFlow)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a+simd+crypto+crc")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenFHE_CXX_FLAGS}")

# Add the main executable
add_executable(demo_loop 
    examples/demo_loop.cpp
    src/fhe/fhe_context.cpp
    src/gates/fhe_gates.cpp
    src/algorithms/fhe_cmp.cpp
    src/selector/trusted_selector.cpp
    src/pipeline/loop_controller.cpp
)

target_link_libraries(demo_loop 
    ${OpenFHE_LIBRARIES}
)
