cmake_minimum_required(VERSION 4.0)

project(mini_kvm LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -Werror)

set(MINI_KVM_SRC_DIR ${CMAKE_SOURCE_DIR}/src)

add_library(${PROJECT_NAME} STATIC
    ${MINI_KVM_SRC_DIR}/utils/logger.c 
    ${MINI_KVM_SRC_DIR}/utils/errors.c 
    ${MINI_KVM_SRC_DIR}/utils/utils.c 
    ${MINI_KVM_SRC_DIR}/utils/filesystem.c 
    ${MINI_KVM_SRC_DIR}/commands/run.c 
    ${MINI_KVM_SRC_DIR}/commands/status.c 
    ${MINI_KVM_SRC_DIR}/commands/pause.c 
    ${MINI_KVM_SRC_DIR}/commands/resume.c 
    ${MINI_KVM_SRC_DIR}/kvm/kvm.c 
    ${MINI_KVM_SRC_DIR}/ipc/ipc.c 
)
target_include_directories(${PROJECT_NAME} PUBLIC ${MINI_KVM_SRC_DIR})
target_link_libraries(${PROJECT_NAME} pthread)

set(PROJECT_EXEC mkvm)
add_executable(${PROJECT_EXEC}
    ${MINI_KVM_SRC_DIR}/main.c
)
target_link_libraries(${PROJECT_EXEC} ${PROJECT_NAME})
target_include_directories(${PROJECT_EXEC} PUBLIC ${MINI_KVM_SRC_DIR})

# add tests
include(CTest)
add_subdirectory(tests)
