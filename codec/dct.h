#ifndef DCT_H_INCLUDED
#define DCT_H_INCLUDED

#include <stdint.h>

void mmf_dct_init();
void mmf_idct(int16_t *dct);
void mmf_dct(int16_t *block);

#endif // DCT_H_INCLUDED
