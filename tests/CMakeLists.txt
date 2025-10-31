cmake_minimum_required(VERSION 4.0)

include(CTest)

function(define_test_exec exec_name sources)
    add_executable(${exec_name} ${sources})
    target_link_libraries(${exec_name} ${PROJECT_NAME})
    target_include_directories(${exec_name} PUBLIC ${CMAKE_SOURCE_DIR}/src)
endfunction()

function(define_test test_name)
    add_executable(${test_name} args.c)
    target_link_libraries(${test_name} ${PROJECT_NAME})
    target_include_directories(${test_name} PUBLIC ${CMAKE_SOURCE_DIR}/src)
endfunction()

function(define_mkvm_test_fail name regex args)
    string(REPLACE " " ";" args "${args}")
    add_test(NAME mkvm.${name} COMMAND mkvm ${args})
    set_property(TEST mkvm.${name} 
        PROPERTY WILL_FAIL
        PROPERTY PASS_REGULAR_EXPRESSION ${regex}
    )
endfunction()

# mkvm tests
define_mkvm_test_fail(no_mem "cannot create VM with memory of size 0" "run")
define_mkvm_test_fail(no_kernel "kernel code is empty" "run --mem=32K")
define_mkvm_test_fail(no_args "mini_kvm <run|pause|resume>" "")

# mkvm run sub commands test
add_subdirectory(run)
add_subdirectory(core)
