cmake_minimum_required(VERSION 3.20)
project(Sherpa-TTSVoiceWizard CXX C)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(thirdparty/crow-build)
add_subdirectory(thirdparty/sherpa-onnx-build)
add_subdirectory(thirdparty/argparse)

add_executable(app "app.cpp")
target_link_libraries(app PUBLIC Crow::Crow sherpa-onnx-c-api argparse)