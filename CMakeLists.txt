# The following lines of boilerplate have to be in your project's
# CMakeLists.txt in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wno-error=unused-variable -Wno-error=format -Wno-error=unused-but-set-variable -Wno-error=uninitialized)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(max_matter_bridge)
