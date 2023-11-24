cmake_minimum_required(VERSION 3.19)

project(mockup VERSION 0.0 HOMEPAGE_URL "https://github.com/drepper/scql" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

bison_target(Parser scql.y ${CMAKE_CURRENT_BINARY_DIR}/scql-tab.cc COMPILE_FLAGS "-fcaret -Wcounterexamples")
flex_target(Scanner scql.l ${CMAKE_CURRENT_BINARY_DIR}/scql-scan.cc)
add_flex_bison_dependency(Scanner Parser)
cmake_policy(SET CMP0098 NEW)
set_source_files_properties(scql-tab.cc PROPERTIES COMPILE_FLAGS "-Wno-redundant-decls -Wno-free-nonheap-object")
set_source_files_properties(scql-scan.cc PROPERTIES COMPILE_FLAGS "-Wno-useless-cast -Wno-sign-compare -Wno-redundant-decls")

add_executable(mockup repl.cc scql.cc scql.hh scql.y ${BISON_Parser_OUTPUTS} scql.l ${FLEX_Scanner_OUTPUTS})
