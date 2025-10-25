#include "core/core.h"

#include <stdio.h>
#include <string.h>

#define IS_UINT_TEST(str)                                                                          \
    if (!mini_kvm_is_uint((str), strlen(str))) {                                                   \
        printf("mini_kvm_is_uint failed with %s\n", (str));                                        \
        return 1;                                                                                  \
    }

#define IS_UINT_TEST_FAIL(str)                                                                     \
    if (mini_kvm_is_uint((str), strlen(str))) {                                                    \
        printf("mini_kvm_is_uint failed with %s\n", (str));                                        \
        return 1;                                                                                  \
    }

#define TO_UINT_TEST(str, expected)                                                                \
    {                                                                                              \
        int32_t ret = 0;                                                                           \
        uint64_t dest = 0;                                                                         \
        ret = mini_kvm_to_uint((str), strlen((str)), &dest);                                       \
        if (dest != (expected) || ret != MINI_KVM_SUCCESS) {                                       \
            printf("mini_kvm_to_uint failed with %s, %lu returned %lu\n", (str), (expected),       \
                   dest);                                                                          \
            return 1;                                                                              \
        }                                                                                          \
    }

int main(void) {
    IS_UINT_TEST("1")
    IS_UINT_TEST("2")
    IS_UINT_TEST("")
    IS_UINT_TEST("123456789")

    IS_UINT_TEST_FAIL("/")
    IS_UINT_TEST_FAIL(":")

    TO_UINT_TEST("1", 1LU)
    TO_UINT_TEST("2", 2LU)
    TO_UINT_TEST("123456789", 123456789LU)

    return 0;
}
