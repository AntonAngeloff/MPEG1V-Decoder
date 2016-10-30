#include "math.h"
#include "dct.h"

static double __cos[8][8];
static double __c[8];

/*
 */
__attribute__((constructor)) void mmf_dct_init()
{
    int i, j;

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++)
          __cos[i][j] = cos((2 * i + 1) * j * acos(-1) / 16.0);

        if (i)
            __c[i] = 1;
        else
            __c[i] = 1 / sqrt(2);
    }
}

/* Naive implementation of inverse discrete cosine transform, a.k.a. DCT type-III
 */
void mmf_idct(int16_t *dct)
{
    int16_t buf[64];
    int i, j, x, y;

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            double sum = 0;

            for (x = 0; x < 8; x++)
                for (y = 0; y < 8; y++) {
                    int8_t ind = (y * 8) + x;//((c * 8 * y) * 8) + (r * 8 * x);
                    sum += __c[x] * __c[y] * dct[ind] * __cos[i][x] * __cos[j][y];
                }

            sum *= 0.25;
            sum += 128;

            int8_t ind = (j * 8) + i;
            buf[ind] = (int16_t)sum;
        }
    }

    memcpy(dct, buf, 128);
}

/* Naive implementation of two-dimensional discrete cosine transform, a.k.a. DCT type-II
 */
void mmf_dct(int16_t *block)
{
    int16_t buf[64];
    int i, j, x, y;

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++) {
            double sum = 0;

            for (x = 0; x < 8; x++)
                for (y = 0; y < 8; y++)
                    sum += (block[y * 8 + x] - 128) * __cos[x][i] * __cos[y][j];

            sum *= __c[i] * __c[j] * 0.25;

            if (sum < 0) sum = 0;
            if (sum > 255) sum = 255;

            buf[j * 8 + i] = (int16_t)sum;
    }

    memcpy(block, buf, 128);
}
