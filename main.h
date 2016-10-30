#ifndef __MAIN_H__
#define __MAIN_H__

#include <windows.h>

/*  To use this exported function of dll, include this header
 *  in your project. (IDE's bullshit note)
 */

#ifdef BUILD_DLL
    #define __mmf_export __declspec(dllexport)
#else
    #define __mmf_export
#endif

#define __cdecl __attribute__((__cdecl__))
#define __stdcall __attribute__((__stdcall__))

/*
 * We will use cdecl convention
 */
#define __mmf_api __cdecl __mmf_export

/*
 * Declare exported functions here
 */
#ifdef __cplusplus
extern "C"
{
#endif

void __mmf_export SomeFunction(const LPCSTR sometext);

#ifdef __cplusplus
}
#endif

#endif // __MAIN_H__
