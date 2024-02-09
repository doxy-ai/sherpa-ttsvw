cmake_minimum_required(VERSION 3.20)
project(Sherpa-TTSVoiceWizard CXX C)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(thirdparty/crow-build)
add_subdirectory(thirdparty/sherpa-onnx-build)

include(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/sherpa-onnx-build/sherpa-onnx/cmake/cargs.cmake)

add_executable(tst "test.c")
target_link_libraries(tst PUBLIC Crow::Crow sherpa-onnx-c-api cargs)