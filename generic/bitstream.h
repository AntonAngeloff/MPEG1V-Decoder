/**
 * @file bitstream.h
 *
 * @brief      Bitstream reader
 * @details    Provides functionality to read arbitrary number of bits (up to 32) from a bit/byte stream.
 * @date       February 24, 2016
 * @author	   Anton Angelov, ant0n@mail.bg
 */

#ifndef BITSTREAM_H_INCLUDED
#define BITSTREAM_H_INCLUDED

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "..\mmfutil.h"

typedef struct {
    /**
     *  Inner buffer where we will store the bit stream (actually in form of a
     *  byte stream.
     */
    uint8_t *buffer;

    /**
     *  Capacity of the internal buffer in bytes.
     */
    int32_t buffer_capacity;

    /**
     *  Write index in byte units
     */
    int32_t write_index;

    /**
     *  Read index in byte units
     */
    int32_t read_index;

    /**
     *  Read index in bit units (actually read_bit_index/8 == read_index should always
     *  be true.
     */
    int32_t read_bit_index;

    /**
     * If a FILE handle is assigned to source file, then the stream
     *  will internally refill it's inner buffer when it exhausts.
     */
    FILE *source_file;
    int32_t file_size;
}  MMFBitstream;

//TODO: write comments
MMFBitstream* bitstream_alloc(int32_t capacity);
MMFBitstream* bitstream_alloc_load_file(char *filename, MMFRES *res);
MMFRES bitstream_free(MMFBitstream **bs);

MMFRES bitstream_write(MMFBitstream *str, uint8_t *src_data, int32_t data_size);
uint32_t bitstream_read_bits(MMFBitstream *str, int32_t n, MMFRES *rc);

/**
 * Peeks at the following <i>n</i> bits, without moving the read index.
 * @param str Pointer to MMF bitstream
 * @param n   Number of bits to peek at
 * @param rc  Pointer to MMFRES variable, which will receive the return code of the operation. NULL is allowed.
 * @return Returns 32-bit unsigned integer, where the most significant 32-<i>n</i> bits are zeroes, and the remaining <i>n</i> bits are the "peeked" bits.
 */
uint32_t bitstream_peek_bits(MMFBitstream *str, int32_t n, MMFRES *rc);

/**
 * Discards <i>n</i> bits from the stream, by moving the read index.
 * @param str Pointer to MMF bitstream
 * @param n Number of bits to dicards
 * @return RC_OK on success, error otherwise
 */
MMFRES bitstream_discard_bits(MMFBitstream *str, int32_t n);

/**
 * Indicates if the bitstream's <i>reading index</i> is byte-aligned, i.e. if it is stepping on byte border.
 * @param bs Pointer to MMF bitstream
 * @return TRUE if byte-aligned, FALSE otherwise
 */
MMFRES bitstream_is_byte_aligned(MMFBitstream *bs);

MMFRES bitstream_align_to_byte(MMFBitstream *bs);

/**
 * Returns the size of the remaining (unread) part of the bitstream in byte units.
 * @param bs Pointer to MMF bitstream struct
 * @return Size of bitstream in bytes
 */
int32_t bitstream_get_size(MMFBitstream *bs);

MMFRES bitstream_flush(MMFBitstream *bs);

/**
 * Causes the bitstream to refill it's internal buffer, by reading content from it's associated file.
 * It might call bitstream_flush() to create more space.
 * @remark This function is only usable for bitstream created with bitstream_alloc_load_file().
 * @see bitstream_alloc_load_file
 *
 * @param bs Pointer to a MMF bitstream
 * @return RC_OK on success, error otherwise
 */
MMFRES bitstream_replenish(MMFBitstream *bs);

#endif // BITSTREAM_H_INCLUDED
