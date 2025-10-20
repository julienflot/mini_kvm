cmake_minimum_required(VERSION 4.0)

include(CTest)

define_test_exec(conversion conversion.c)

add_test(NAME conversion COMMAND conversion)
