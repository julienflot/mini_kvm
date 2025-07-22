cmake_minimum_required(VERSION 4.0)

project(mini_kvm LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -Werror)

set(MINI_KVM_SRC_DIR ${CMAKE_SOURCE_DIR}/src)

add_executable(${PROJECT_NAME} 
    ${MINI_KVM_SRC_DIR}/utils/logger.c 
    ${MINI_KVM_SRC_DIR}/utils/errors.c 
    ${MINI_KVM_SRC_DIR}/utils/utils.c 
    ${MINI_KVM_SRC_DIR}/commands/run.c 
    ${MINI_KVM_SRC_DIR}/kvm/kvm.c 
    ${MINI_KVM_SRC_DIR}/main.c
)
target_include_directories(${PROJECT_NAME} PUBLIC ${MINI_KVM_SRC_DIR})
