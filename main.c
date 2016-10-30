#include "main.h"
#include "mmfutil.h"
#include "mmfcodec.h"
#include "string.h"

#include <stdio.h>
#include <conio.h>
#include <time.h>
#include <stdlib.h>
#include "generic\bitstream_test.h"

#define CHECKRES(x, y)          \
    if (failed(x)) {            \
        printf(y);              \
        return 1;               \
    }

/*
// a sample exported function
void DLL_EXPORT SomeFunction(const LPCSTR sometext)
{
    MessageBoxA(0, sometext, "DLL Message", MB_OK | MB_ICONINFORMATION);
}
*/

/*
__mmf_export BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            // attach to process
            // return FALSE to fail DLL load
            break;

        case DLL_PROCESS_DETACH:
            // detach from process
            break;

        case DLL_THREAD_ATTACH:
            // attach to thread
            break;

        case DLL_THREAD_DETACH:
            // detach from thread
            break;
    }
    return TRUE; // succesful
}
*/

#undef BUILD_DLL
#ifndef BUILD_DLL
void fill_sample(MMFSample *s)
{
    //Fills NV12 video frame with random color
    memset( s->buffer_data[0], rand() % 256, s->buffer_stride[0] * s->height );
    memset( s->buffer_data[1], rand() % 256, s->buffer_stride[1] * (s->height / 2) );
}

int main() {
    bitstream_test_file("grb_1_copy.mpg");
}
#endif
