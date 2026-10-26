cmake_minimum_required(VERSION 3.20)
project(printer_2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Platform (spec §11): one configuration per platform, chosen at configure time ---
set(PRINTER_PLATFORM "PC" CACHE STRING "Target platform: PC or MC")
set_property(CACHE PRINTER_PLATFORM PROPERTY STRINGS PC MC)
if(NOT PRINTER_PLATFORM MATCHES "^(PC|MC)$")
    message(FATAL_ERROR "PRINTER_PLATFORM must be PC or MC, not '${PRINTER_PLATFORM}'")
endif()

# --- Library rules (spec §1.1, AGENTS.md §2) ---
# Common and MC code may include only these system headers.
set(PRINTER_ALLOWED_SYSTEM_HEADERS cstdint stdint.h stddef.h stdarg.h limits.h float.h)

function(printer_check_includes)
    foreach(file IN LISTS ARGN)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${file}")
        file(STRINGS "${file}" lines REGEX "^[ \t]*#[ \t]*include(_next)?[ \t]*<")
        foreach(line IN LISTS lines)
            string(REGEX REPLACE "^[ \t]*#[ \t]*include(_next)?[ \t]*<([^>]+)>.*" "\\2" header "${line}")
            if(NOT header IN_LIST PRINTER_ALLOWED_SYSTEM_HEADERS)
                message(FATAL_ERROR "${file}: <${header}> is not allowed in Common or MC code (AGENTS.md §2)")
            endif()
        endforeach()
    endforeach()
endfunction()

# Warnings are errors in every project target (Phase 1 plan Q-3).
function(printer_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    if(PRINTER_PLATFORM STREQUAL "MC")
        target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti -fno-threadsafe-statics)
    endif()
endfunction()

# --- Source collection (AGENTS.md §1): link-time platform selection by file prefix ---
file(GLOB_RECURSE library_files CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Printer/*.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/Printer/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Hardware/*.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/Hardware/*.cpp")

set(common_files ${library_files})
list(FILTER common_files EXCLUDE REGEX "/(pc|mc)_[^/]*$")
set(pc_files ${library_files})
list(FILTER pc_files INCLUDE REGEX "/pc_[^/]*$")
set(mc_files ${library_files})
list(FILTER mc_files INCLUDE REGEX "/mc_[^/]*$")

# pc_ headers that shared driver code includes follow the Common rules (AGENTS.md §1).
set(pc_headers_under_common_rules
    "${CMAKE_CURRENT_SOURCE_DIR}/Hardware/pc_chip.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Hardware/pc_STM32F103xE.hpp")
set(existing_pc_common_headers)
foreach(file IN LISTS pc_headers_under_common_rules)
    if(EXISTS "${file}")
        list(APPEND existing_pc_common_headers "${file}")
    endif()
endforeach()

file(GLOB_RECURSE mc_test_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/mc/*")

# Checked on both platforms: everything that is compiled for the MC, plus the shared entry points.
printer_check_includes(
    ${common_files} ${mc_files} ${existing_pc_common_headers} ${mc_test_files}
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Tests/Tests.hpp")

if(PRINTER_PLATFORM STREQUAL "PC")
    set(platform_files ${pc_files})
else()
    set(platform_files ${mc_files})
endif()

# --- printer: the library ---
add_library(printer STATIC ${common_files} ${platform_files})
set_target_properties(printer PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(printer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
printer_warnings(printer)

# --- printer_2: the demo; main() runs the platform's test suite first (spec §12, §13.41) ---
if(PRINTER_PLATFORM STREQUAL "PC")
    file(GLOB pc_test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/pc/*.cpp")
    list(FILTER pc_test_sources EXCLUDE REGEX "/test_config_mc_host\\.cpp$")

    add_executable(printer_2 main.cpp ${pc_test_sources})
    target_link_libraries(printer_2 PRIVATE printer)
    printer_warnings(printer_2)

    enable_testing()   # PC only: ctest runs the demo with its tests; the MC suite runs on the target
    add_test(NAME printer_2 COMMAND printer_2)
else()
    message(STATUS "PRINTER_PLATFORM=MC: only the printer library is configured; the MC demo and suite come with the MC Hardware plan")
endif()
