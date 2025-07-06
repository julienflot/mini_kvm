cmake_minimum_required(VERSION 4.0)

project(mini_kvm LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set compiler options
add_compile_options(-Wall -Wextra -Werror)

set(MINI_KVM_SRC_DIR ${CMAKE_SOURCE_DIR}/src)

message(${MINI_KVM_SRC_DIR})

add_library(libmini_kvm ${MINI_KVM_SRC_DIR}/utils/logger.c)
target_include_directories(libmini_kvm PUBLIC ${MINI_KVM_SRC_DIR})

add_executable(${PROJECT_NAME} ${MINI_KVM_SRC_DIR}/main.c)
target_include_directories(${PROJECT_NAME} PUBLIC ${MINI_KVM_SRC_DIR})
target_link_libraries(${PROJECT_NAME} libmini_kvm)
