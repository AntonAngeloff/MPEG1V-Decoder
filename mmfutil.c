#include "mmfutil.h"
#include "stdlib.h"
#include "string.h"

/*
 * Wrapping of the memory function, for debug purposes later
 */
inline void* mmf_alloc(int32_t size) //todo: make inline
{
    return malloc(size);
}

inline void* mmf_allocz(int32_t size)
{
    void *ptr = mmf_alloc(size);
    memset(ptr, 0, size);

    return ptr;
}

inline void mmf_free(void *ptr)
{
    free(ptr);
}

inline void* mmf_realloc(void *ptr, int32_t newsize)
{
    return realloc(ptr, newsize);
}

inline int succeeded(MMFRES res)
{
    return res <= RC_FALSE;
}

inline int failed(MMFRES res)
{
    return res > RC_FALSE;
}
