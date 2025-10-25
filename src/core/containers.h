#ifndef C_CONTAINERS_H
#define C_CONTAINERS_H

#include <stdint.h>
#include <stdlib.h>

typedef struct VCpu VCpu;

#define GEN_DEF_VEC(type)                                                                          \
    typedef struct vec_##type {                                                                    \
        type *tab;                                                                                 \
        size_t len;                                                                                \
        size_t capacity;                                                                           \
    } vec_##type;                                                                                  \
                                                                                                   \
    vec_##type *vec_new_##type();                                                                  \
    void vec_free_##type(vec_##type *vec);                                                         \
    int64_t vec_resize_##type(vec_##type *vec, size_t capacity);                                   \
    void vec_append_##type(vec_##type *vec, type value);                                           \
    void vec_pop_##type(vec_##type *vec);                                                          \
    int64_t vec_insert_##type(vec_##type *vec, size_t index, type value);

#define GEN_ENTRY(type, func) vec##_##type * : func##_##type
#define FUNC_GEN(func, X) GEN_FUNC_BODY(func, X)(X)
#define FUNC_GEN_ARG(func, X, arg) GEN_FUNC_BODY(func, X)(X, arg)
#define FUNC_GEN_ARG2(func, X, arg1, arg2) GEN_FUNC_BODY(func, X)(X, arg1, arg2)

// Generic functions
#define vec_free(X) FUNC_GEN(vec_free, X)
#define vec_resize(X, C) FUNC_GEN_ARG(vec_resize, X, C)
#define vec_append(X, C) FUNC_GEN_ARG(vec_append, X, C)
#define vec_pop(X) FUNC_GEN(vec_pop, X)
#define vec_insert(X, I, V) FUNC_GEN_ARG2(vec_insert, X, I, V)

// implementation of user functions
GEN_DEF_VEC(uint64_t)
GEN_DEF_VEC(VCpu)
#define GEN_FUNC_BODY(func, X) _Generic((X), GEN_ENTRY(uint64_t, func), GEN_ENTRY(VCpu, func))

#endif /*C_CONTAINERS_H*/
