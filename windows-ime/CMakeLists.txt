# Akkhara Windows keyboard (TSF) + test tools. Build on Windows with Visual Studio 2022:
#   cmake -B build -A x64 -DORT_DIR=path/to/onnxruntime-win-x64-1.22.0
#   cmake --build build --config Release
cmake_minimum_required(VERSION 3.20)
project(AkkharaIME CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# static C++ runtime: the keyboard DLL is loaded into every app, it must not need anything else
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
add_compile_options(/W4 /utf-8 /EHsc)

set(ORT_DIR "" CACHE PATH "ONNX Runtime release folder (include/ and lib/)")
set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/../core)

# the keyboard: loads ONNX Runtime itself at run time (akkhara-ort.dll), so no import library
add_library(AkkharaIME SHARED
    src/dllmain.cpp src/register.cpp src/text_service.cpp src/candidate_window.cpp src/engine_host.cpp
    ${CORE}/akkhara_engine.cpp src/akkhara.rc src/akkhara.def)
target_include_directories(AkkharaIME PRIVATE ${ORT_DIR}/include ${CORE})
target_link_libraries(AkkharaIME PRIVATE uuid ole32 oleaut32 user32 gdi32 advapi32)

# engine test: same inputs as the Python test → must give the same top-5
add_executable(akkhara_cli ${CORE}/akkhara_cli.cpp ${CORE}/akkhara_engine.cpp)
target_include_directories(akkhara_cli PRIVATE ${ORT_DIR}/include ${CORE})
target_link_libraries(akkhara_cli PRIVATE ${ORT_DIR}/lib/onnxruntime.lib)

# keyboard self-test: types "nh jg tv sala rean" + Enter into a text box with Akkhara active
add_executable(akkhara_selftest src/selftest.cpp)
target_include_directories(akkhara_selftest PRIVATE src)
target_link_libraries(akkhara_selftest PRIVATE uuid ole32 user32)
