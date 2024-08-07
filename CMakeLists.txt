cmake_minimum_required(VERSION 3.8)
project(mockturtle LANGUAGES CXX)
include(FetchContent)

set(MOCKTURTLE_CXX_STANDARD "17" CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD ${MOCKTURTLE_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(MOCKTURTLE_EXAMPLES "Build examples" ON)
option(MOCKTURTLE_TEST "Build tests" OFF)

option(MOCKTURTLE_EXPERIMENTS "Build experiments" OFF)
option(BILL_Z3 "Enable Z3 interface for bill library" ON)
option(ENABLE_COVERAGE "Enable coverage reporting for gcc/clang" OFF)
option(ENABLE_MATPLOTLIB "Enable matplotlib library in experiments" OFF)
option(ENABLE_NAUTY "Enable the Nauty library for percy" OFF)

if(UNIX)
  # show quite some warnings (but remove some intentionally)
  include(CheckCXXCompilerFlag)
  add_compile_options(-W -Wall -Wextra)
  foreach (WARNING unknown-pragmas gnu-anonymous-struct nested-anon-types)
    check_cxx_compiler_flag("-Wno-${WARNING}" HAS_WNO_${WARNING})
    if (HAS_WNO_${WARNING})
      add_compile_options(-Wno-${WARNING})
    endif()
  endforeach()
  if (ENABLE_COVERAGE)
    add_compile_options(-O0 -g --coverage -fprofile-arcs -ftest-coverage)
  endif()
endif()
if(MSVC)
  add_compile_options(/EHsc /bigobj)
endif()
if (WIN32)
  set(ENABLE_NAUTY OFF)
endif()

add_subdirectory(include)
add_subdirectory(lib)

FetchContent_Declare(z3
        GIT_REPOSITORY https://github.com/Z3Prover/z3
        GIT_TAG        z3-4.12.1
)
FetchContent_MakeAvailable(z3)


include_directories( ${z3_SOURCE_DIR}/src/api )

target_link_libraries(libz3)


if(ENABLE_NAUTY)
  add_definitions(-DENABLE_NAUTY)
endif()

if(MOCKTURTLE_EXAMPLES)
  add_subdirectory(examples)
endif()

if(MOCKTURTLE_TEST)
  add_subdirectory(test)
endif()

if(MOCKTURTLE_EXPERIMENTS)
  add_subdirectory(experiments)
endif()
