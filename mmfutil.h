#ifndef MMFUTIL_H_INCLUDED
#define MMFUTIL_H_INCLUDED

#include "mmflog.h"
#include <stdint.h>

//todo: set proper const
#define MMF_NOPTS_VALUE -0x7fff

/**
 * Assertion macro
 */
#define mmf_tostring(s) #s
#define mmf_assert(cond) do {                                           \
    if (!(cond)) {                                                      \
        mmf_log(NULL, LOG_LEVEL_ERROR, "Assertion %s failed at %s:%d\n",\
               mmf_tostring(cond), __FILE__, __LINE__);                 \
        abort();                                                        \
    }                                                                   \
} while (0)

/**
 * Return code enum
 */
typedef enum MMFRES {
    RC_OK           = 0x00,
    RC_FALSE        = 0x01,

    RC_FAIL         = 0x02,
    RC_OUTOFMEM     = 0x03,
    RC_INVALIDARG   = 0x04,
    RC_EXTERNAL     = 0x05,
    RC_NEED_MORE_INPUT = 0x06,
    RC_BUFFER_OVERFLOW = 0x07,
    RC_NOT_ALLOWED  = 0x08,
    RC_END_OF_STREAM   = 0x09,
    RC_INVALIDDATA     = 0x0A,
	RC_INVALIDPOINTER,
    RC_NOTIMPLEMENTED,
} MMFRES;

#ifdef DEBUG
//Todo: make memory leak detection mechanism
//static long mmf_alloced_mem = 0;
#endif

/**
 * We make wrapper over the memory routines
 */
inline void* mmf_alloc(int32_t size);
inline void* mmf_allocz(int32_t size);
inline void mmf_free(void *ptr);
inline void* mmf_realloc(void *ptr, int32_t newsize);

inline int succeeded(MMFRES res);
inline int failed(MMFRES res);

#endif // MMFUTIL_H_INCLUDED
