cmake_minimum_required(VERSION 3.20)
project(Sherpa-TTSVoiceWizard CXX C)

set(CMAKE_CXX_STANDARD 20)
if(MSVC)
    set(CMAKE_BUILD_TYPE Release) # OnnxRuntime on windows only works in release mode!
endif()

add_subdirectory(thirdparty/crow-build)
add_subdirectory(thirdparty/sherpa-onnx-build)
add_subdirectory(thirdparty/argparse)

add_executable(app "app.cpp")
target_link_libraries(app PUBLIC Crow::Crow sherpa-onnx-c-api argparse)

if(MSVC)
    set_property(TARGET app PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()