cmake_minimum_required(VERSION 4.0)

project(mini_kvm LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Werror)
add_library(${PROJECT_NAME} STATIC
    src/core/logger.c 
    src/core/errors.c 
    src/core/core.c 
    src/core/filesystem.c 
    src/core/containers.c 
    src/commands/run.c 
    src/commands/status.c 
    src/commands/pause.c 
    src/commands/resume.c 
    src/commands/shutdown.c 
    src/kvm/kvm.c 
    src/ipc/ipc.c 
)
target_include_directories(${PROJECT_NAME} PUBLIC src)
target_link_libraries(${PROJECT_NAME} pthread)

set(PROJECT_EXEC mkvm)
add_executable(${PROJECT_EXEC} src/main.c)
target_link_libraries(${PROJECT_EXEC} ${PROJECT_NAME})
target_include_directories(${PROJECT_EXEC} PUBLIC src)

# add bios compilation
add_subdirectory(bios)

# add tests
include(CTest)
add_subdirectory(tests)
