#ifndef MMFLOG_H_INCLUDED
#define MMFLOG_H_INCLUDED

//#include "mmfutil.h"

typedef enum MMFLogLevel {
    LOG_LEVEL_VERBOSE,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
} MMFLogLevel;

void mmf_log(void* avcl, int level, const char *fmt, ...);

#endif // MMFLOG_H_INCLUDED
