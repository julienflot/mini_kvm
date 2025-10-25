#include "containers.h"
#include <string.h>

#include "kvm/kvm.h"

#define GEN_IMPL_VEC(type)                                                                         \
    vec_##type *vec_new_##type() {                                                                 \
        vec_##type *vec = malloc(sizeof(vec_##type));                                              \
        vec->capacity = 1;                                                                         \
        vec->len = 0;                                                                              \
        vec->tab = malloc(sizeof(type) * vec->capacity);                                           \
                                                                                                   \
        return vec;                                                                                \
    }                                                                                              \
    void vec_free_##type(vec_##type *vec) {                                                        \
        free(vec->tab);                                                                            \
        free(vec);                                                                                 \
    }                                                                                              \
    int64_t vec_resize_##type(vec_##type *vec, size_t capacity) {                                  \
        if (capacity <= vec->capacity) {                                                           \
            return -1;                                                                             \
        }                                                                                          \
                                                                                                   \
        type *tab = vec->tab;                                                                      \
        vec->capacity = capacity;                                                                  \
        vec->tab = malloc(sizeof(type) * vec->capacity);                                           \
        memcpy(vec->tab, tab, vec->len * sizeof(type));                                            \
        free(tab);                                                                                 \
        return 0;                                                                                  \
    }                                                                                              \
    void vec_append_##type(vec_##type *vec, type value) {                                          \
        vec->len += 1;                                                                             \
        if (vec->len >= vec->capacity) {                                                           \
            vec_resize_##type(vec, vec->capacity << 1);                                            \
        }                                                                                          \
        vec->tab[vec->len - 1] = value;                                                            \
    }                                                                                              \
    void vec_pop_##type(vec_##type *vec) { vec->len -= 1; }

GEN_IMPL_VEC(uint64_t)
GEN_IMPL_VEC(VCpu)
