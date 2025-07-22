cmake_minimum_required(VERSION 4.0)

include(CTest)

function(define_test test_name)
    add_executable(${test_name} args.c)
    target_link_libraries(${test_name} mini_kvm)
    target_include_directories(${test_name} PUBLIC ../../src) # TODO: find a better wau to handle this path
endfunction()

add_subdirectory(run)
