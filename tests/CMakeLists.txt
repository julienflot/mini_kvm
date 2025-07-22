cmake_minimum_required(VERSION 4.0)

include(CTest)

function(define_test test_name)
    add_executable(${test_name} args.c)
    target_link_libraries(${test_name} ${PROJECT_NAME})
    target_include_directories(${test_name} PUBLIC ${CMAKE_SOURCE_DIR}/src)
endfunction()

add_subdirectory(run)
