cmake_minimum_required(VERSION 3.16)
project(minilang LANGUAGES CXX)
include(CTest)

add_library(minilang_core
  src/lexer.c++
  src/parser.c++
  src/interpreter.c++
  src/compiler.c++
  src/vm.c++
)
target_compile_features(minilang_core PUBLIC cxx_std_20)
set_target_properties(minilang_core PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(minilang_core PUBLIC src)
if(MSVC)
  target_compile_options(minilang_core PRIVATE /W4)
else()
  target_compile_options(minilang_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(minilang src/main.c++)
target_link_libraries(minilang PRIVATE minilang_core)
set_target_properties(minilang PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if(BUILD_TESTING)
  add_executable(minilang_tests tests/regression.c++)
  target_link_libraries(minilang_tests PRIVATE minilang_core)
  add_test(NAME regression COMMAND minilang_tests)
  add_test(NAME cli_help COMMAND minilang --help)
  add_test(NAME cli_regression COMMAND ${CMAKE_COMMAND}
    -DMINILANG=$<TARGET_FILE:minilang> -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli.cmake)
endif()
