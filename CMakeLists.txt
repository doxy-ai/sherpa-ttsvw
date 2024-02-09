cmake_minimum_required(VERSION 3.20)
project(Sherpa-TTSVoiceWizard CXX C)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(thirdparty/crow-build)

add_executable(tst "test.cpp")
target_link_libraries(tst PUBLIC Crow::Crow)