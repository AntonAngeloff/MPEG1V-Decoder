#include "mmflog.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "windows.h"

void mmf_log(void* avcl, int level, const char *fmt, ...)
{
    time_t now;
    struct tm *timeinfo;
    char str[200];

    time(&now);
    timeinfo = localtime(&now);

    strftime(str, 200, "%H:%M", timeinfo);

    #ifdef DEBUG
    #ifdef BUILD_DLL
    char buffer[1024];
    sprintf(buffer, "[%s]: %s", str, fmt);

    OutputDebugStringA(buffer);
    #else
    //Todo: respect log_level
    printf("[%s]: %s", str, fmt);

    if( (strlen(fmt)>0 && fmt[strlen(fmt)-1]!='\n') || strlen(fmt)==0 ) {
        printf("\n");
    }
    #endif //BUILD_DLL
    #endif //DEBUG

}

