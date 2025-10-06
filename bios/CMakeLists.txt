cmake_minimum_required(VERSION 4.0)

set(PROJECT_NAME mini_bios)
project(${PROJECT_NAME} LANGUAGES C ASM)

set(MINI_BIOS_LIB ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.a)
set(LD_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/bios.ld)
set(BIOS_FILE bios.img)

add_compile_options(-m32 -fno-pic -Wall -Wextra -Werror)
add_library(mini_bios 
    boot.s
)

# ASSEMBLE BIOS
add_custom_target(
    bios
    ALL
    BYPRODUCTS ${BIOS_FILE}
    DEPENDS mini_bios
    COMMAND ld --oformat binary -T ${LD_SCRIPT} -m elf_i386 ${MINI_BIOS_LIB} -o ${BIOS_FILE}
    COMMENT "Compiling ${BIOS_FILE}"
)
