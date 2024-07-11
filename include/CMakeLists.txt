add_library(mockturtle INTERFACE)
target_include_directories(mockturtle SYSTEM INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(mockturtle SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mockturtle INTERFACE kitty lorina parallel_hashmap percy json bill ez libabcesop abcresub fmt easy angel tweedledum eigen  sparsepp matplot rang CLI11 cudd matplot glucose tweedledum_lib tweedledee caterpillar cudd cudd_lib )

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
target_link_libraries(mockturtle INTERFACE stdc++fs)
endif()


