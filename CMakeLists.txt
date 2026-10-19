cmake_minimum_required(VERSION 3.20)
project(printer_2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(printer_2
    main.cpp
)

target_include_directories(printer_2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
