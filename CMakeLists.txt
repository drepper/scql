cmake_minimum_required(VERSION 3.19)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

project(mockup VERSION 0.0 HOMEPAGE_URL "https://github.com/drepper/scql" LANGUAGES C CXX)

# We cannot use -D_FORTIFY_SOURCE=2 for the debug build without enabling optimizations.
set(debug_opts "-Og -g -fvar-tracking -fvar-tracking-assignments -D_GLIBCXX_DEBUG -DYYDEBUG=1 -D_FORTIFY_SOURCE=2")
# set(debug_opts "-O0 -g -fvar-tracking -fvar-tracking-assignments -D_GLIBCXX_DEBUG -DYYDEBUG=1")
set(CMAKE_CXX_FLAGS_DEBUG "${debug_opts}")
set(CMAKE_C_FLAGS_DEBUG "${debug_opts}")

set(release_opts "-O3 -g -flto -DNDEBUG -DYYDEBUG=0 -DM2_RELEASE -D_FORTIFY_SOURCE=2")
set(CMAKE_CXX_FLAGS_RELEASE "${release_opts}")
set(CMAKE_C_FLAGS_RELEASE "${release_opts}")

set(relwithdebinfo_opts "-O2 -g -DNDEBUG -DYYDEBUG=0 -DM2_RELEASE -D_FORTIFY_SOURCE=2")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${relwithdebinfo_opts}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${relwithdebinfo_opts}")

set(minsizerel_opts "-Os -g -DNDEBUG -DYYDEBUG=0 -DM2_RELEASE -D_FORTIFY_SOURCE=2")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${minsizerel_opts}")
set(CMAKE_C_FLAGS_MINSIZEREL "${minsizerel_opts}")

set(gcc_opts "-fsave-optimization-record -frecord-gcc-switches -fdiagnostics-color=always")
set(common_warnings "-Werror -Wall -Wextra -Wnull-dereference -Wdouble-promotion -Wshadow -Wformat=2 -Wcast-qual -Wcast-align -Wstrict-aliasing -Wpointer-arith -Winit-self -Wredundant-decls -Wundef -Wempty-body -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wrestrict")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
string(APPEND CMAKE_CXX_FLAGS " ${gcc_opts} ${common_warnings} -Wuseless-cast -Wsuggest-override -Weffc++")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)
string(APPEND CMAKE_C_FLAGS " ${gcc_opts} ${common_warnings} -Wjump-misses-init -Wabsolute-value")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

bison_target(Parser scql.y ${CMAKE_CURRENT_BINARY_DIR}/scql-tab.cc COMPILE_FLAGS "-fcaret -Wcounterexamples")
flex_target(Scanner scql.l ${CMAKE_CURRENT_BINARY_DIR}/scql-scan.cc)
add_flex_bison_dependency(Scanner Parser)
cmake_policy(SET CMP0098 NEW)
set_source_files_properties(scql-tab.cc PROPERTIES COMPILE_FLAGS "-Wno-redundant-decls -Wno-free-nonheap-object")
set_source_files_properties(scql-scan.cc PROPERTIES COMPILE_FLAGS "-Wno-useless-cast -Wno-sign-compare -Wno-redundant-decls")

add_executable(mockup repl.cc scql.cc scql.hh scql.y ${BISON_Parser_OUTPUTS} scql.l ${FLEX_Scanner_OUTPUTS})
