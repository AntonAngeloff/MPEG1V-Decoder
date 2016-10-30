#include "bitstream.h"

/* Writes arbitrary buffer to the bit stream.
 */
MMFRES bitstream_write(MMFBitstream *str, uint8_t *src_data, int32_t data_size)
{
    if(str->buffer_capacity - str->write_index < data_size) {
        //Buffer overflow
        return RC_BUFFER_OVERFLOW;
    }

    uint8_t *dst = str->buffer;

    //Copy to end of stream and move index
    memcpy((void*)(dst + str->write_index), (void*)src_data, data_size);
    str->write_index += data_size;

    //Success
    return RC_OK;
}

/* Reads "n" bits from the stream
 */
uint32_t bitstream_read_bits(MMFBitstream *str, int32_t n, MMFRES *rc)
{
    #ifdef DEBUG
    //We don't support reading more than 32 bits at once, using this function
    if (n>32) {
        //TODO: log
        if(rc) *rc = RC_INVALIDARG;
        return 0;
    }
    #endif // DEBUG

    //Check if there are enough bits present in stream
    if((str->read_bit_index + n) > str->write_index * 8) {
        if(str->source_file == NULL) {
            if(rc) *rc = RC_NEED_MORE_INPUT;
            return 0;
        }

        MMFRES retcode = bitstream_replenish(str);
        if(failed(retcode)) {
            if(rc) *rc = retcode;
            return 0;
        }

        //We have reached end of file, and we try to read more bits than we actually have.
        if((str->read_bit_index + n) > str->write_index * 8) {
            if(rc) *rc = RC_END_OF_STREAM;
            return 0;
        }
    }

    //Reset the result value
    uint32_t result = 0;

    while(n > 0) {
        int bits_in_byte = 8 - str->read_bit_index % 8; //how many unread bits have remained in current byte
        int bits_to_read = (n > bits_in_byte) ? bits_in_byte : n; //how many bits to read from current byte
        int bits_left = bits_in_byte - bits_to_read; //bits, that will remain in current byte, after reading

        //Read bits from current byte
        result = (result << bits_to_read) | ((str->buffer[str->read_index] >> bits_left) & ((1 << bits_to_read)-1));

        //Move index
        str->read_bit_index += bits_to_read;
        str->read_index = str->read_bit_index / 8;

        //Decrement n (since we use it as a counter)
        n -= bits_to_read;
    }

    if(rc) *rc = RC_OK;
    return result;
}

/* Discards "n" bits from the stream. The idea is same as *_read_bits, it just
 * doesn't store them in result variable.
 */
MMFRES bitstream_discard_bits(MMFBitstream *str, int32_t n)
{
    /* For sake of simplicity, we will redirect to *_read_bits.
     * Anyway in future versions, this function could be implemented
     * separately, which will benefit performance.
     */
    MMFRES rc;
    bitstream_read_bits(str, n, &rc);

    return rc;
}

/* Reads the following "n" bits, but doesn't move the read index.
 */
uint32_t bitstream_peek_bits(MMFBitstream *str, int32_t n, MMFRES *rc)
{
    #ifdef DEBUG
    //We don't support reading more than 32 bits at once, using this function
    if (n>32) {
        //TODO: log
        return RC_INVALIDARG;
    }
    #endif // DEBUG

    //Check if there are enough bits present in stream
    if((str->read_bit_index + n) > str->write_index * 8) {
        if(str->source_file == NULL) {
            if(rc) *rc = RC_NEED_MORE_INPUT;
            return 0;
        }

        MMFRES retcode = bitstream_replenish(str);
        if(failed(retcode)) {
            if(rc) *rc = retcode;
            return 0;
        }

        //We have reached end of file, and we try to read more bits than we actually have.
        if((str->read_bit_index + n) > str->write_index * 8) {
            if(rc) *rc = RC_END_OF_STREAM;
            return 0;
        }
    }

    uint32_t result = 0;

    int32_t temp_index = str->read_index;
    int32_t temp_bit_index = str->read_bit_index;

    while(n > 0) {
        int bits_in_byte = 8 - temp_bit_index % 8; //how many unread bits have remained in current byte
        int bits_to_read = (n > bits_in_byte) ? bits_in_byte : n; //how many bits to read from current byte
        int bits_left = bits_in_byte - bits_to_read; //bits, that will remain in current byte, after reading

        //Read bits from current byte
        result = (result << bits_to_read) | ((str->buffer[temp_index] >> bits_left) & ((1 << bits_to_read)-1));

        //Move index
        temp_bit_index += bits_to_read;
        temp_index = temp_bit_index / 8;

        //Decrement n (since we use it as a counter)
        n -= bits_to_read;
    }

    if(rc) *rc = RC_OK;
    return result;
}


/* Discards the already read data (i.e. all bytes before the read index).
 * This cause the remaining data in buffer moved to the beginning of the buffer,
 * and read indexes to be updated.
 */
MMFRES bitstream_flush(MMFBitstream *bs)
{

}

/* Reads more data from the assigned file. If no file is assigned, it returns
 * error.
 */
MMFRES bitstream_replenish(MMFBitstream *bs)
{
    if(bs->source_file == NULL) {
        return RC_NOT_ALLOWED;
    }

    //Decide how much bytes to read from the file
    size_t bytes_to_read = bs->buffer_capacity - bs->write_index;
    int bytes_read = 0;


    //If there is less than half space free, flush the stream
    if(bytes_to_read > bs->buffer_capacity / 2) {
        bitstream_flush(bs);
        bytes_to_read = bs->buffer_capacity - bs->write_index;
    }

    #ifdef DEBUG
    //Catch unexpected cases.
    mmf_assert(bytes_to_read>=0);
    #endif

    //See how much bytes remain to end of file
    bytes_to_read = bytes_to_read > bs->file_size ? bs->file_size : bytes_to_read;

    if(bytes_to_read == 0) {
        /* There is no space in the buffer. The user should read
         * more data to free some space.
         */
        return RC_BUFFER_OVERFLOW;
    }

    //Create static array, residing in the stack
    uint8_t *data = mmf_alloc(bytes_to_read);

    //Read from file
    bytes_read = fread(data, 1, bytes_to_read, bs->source_file);

    //Check if end of file is reached.
    if(bytes_read <= 0) {
        int err = ferror(bs->source_file);

        #ifdef DEBUG
        char *err_msg = strerror(err);
        //TODO: log
        #endif // DEBUG

        mmf_free(data);

        if(err != 0) {
            //Failed to read from file
            return RC_FAIL;
        }

        return RC_END_OF_STREAM;
    }

    //Add to inner buffer
    bitstream_write(bs, data, bytes_read);

    mmf_free(data);
    return RC_OK;
}

/*
 * Signifies that the bit index is standing on a byte boundary (i.e. it is byte-aligned).
 * This applies for the read index. If there is a need to check the write index, then
 * there should be another function, or this function should accept additional parameter,
 * signifying which index to check.
 */
MMFRES bitstream_is_byte_aligned(MMFBitstream *bs)
{
    return ((bs->read_bit_index % 8) == 0) ? RC_OK : RC_FALSE;
}

MMFRES bitstream_align_to_byte(MMFBitstream *bs)
{
    if(bs->read_bit_index % 8 == 0) {
        /* Already on byte border */
        return RC_OK;
    }

    return bitstream_discard_bits(bs, 8 - (bs->read_bit_index % 8));
}

/*
 * Allocates a MMFBitstream structure and it's respective inner buffer.
 */
MMFBitstream* bitstream_alloc(int32_t capacity)
{
    MMFBitstream *bs = mmf_allocz(sizeof(MMFBitstream));

    bs->buffer_capacity = capacity;
    bs->buffer = mmf_alloc(bs->buffer_capacity);

    return bs;
}

/* Allocates bit-stream structure, inner buffer and opens a FILE handle
 * to given filename/URL.
 */
MMFBitstream* bitstream_alloc_load_file(char *filename, MMFRES *res)
{
    //Try to open file
    FILE *srcfile = fopen(filename, "rb");
    if(srcfile == NULL) {
        //Notify caller that the given filename argument is invalid.
        if(res) *res = RC_INVALIDARG;

        //Return null pointer.
        return NULL;
    }

    //File opened successfully, so we create new bit-stream.
    MMFBitstream *bs = bitstream_alloc(1024*1024*1024);
    bs->source_file = srcfile;

    //Get file size
    fseek(bs->source_file, 0, SEEK_END);
    bs->file_size = ftell(bs->source_file);
    fseek(bs->source_file, 0, SEEK_SET);

    //Success
    if(res) *res = RC_OK;
    return bs;
}

/*
 * Frees the inner buffer and the MMFBitstream structure.
 */
MMFRES bitstream_free(MMFBitstream **bs)
{
    MMFBitstream *pbs = *bs;

    //Close file (if assigned)
    if(pbs->source_file) {
        fclose(pbs->source_file);
    }

    //Free inner buffer and structure
    mmf_free(pbs->buffer);
    mmf_free(pbs);

    //Null the pointer
    (*bs) = NULL;
    return RC_OK;
}

int32_t bitstream_get_size(MMFBitstream *bs)
{
    if(bs->source_file) {
        return bs->file_size*8;
    }

    return bs->write_index * 8 - bs->read_bit_index;
}

/*
//Reads "n" bits from the stream (first implementation)
uint32_t bitstream_read_bits(MMFBitstream *str, int32_t n)
{
    #ifdef DEBUG
    //We don't support reading more than 32 bits at once
    if (n>32) {
        return RC_INVALIDARG;
    }
    #endif // DEBUG

    //Check if EOS
    if(str->read_index >= str->write_index) {
        return RC_NEED_MORE_INPUT;
    }

    uint32_t result;
    int element_size_bits = sizeof(uint32_t) * 8;

    //Check if we need to "jump" between the element (uint32_t) boundaries
    if(str->read_bit_index + n > element_size_bits) {
        int n_block1 = element_size_bits - str->read_bit_index;
        int n_block2 = n - n_block1;

        //Since we have to read across 4-byte boundary, we will read the
        //proper bits from both sides of the boundary.
        uint32_t block1 = str->buffer[(str->read_index / 4)] & ((1 << n) - 1);
        uint32_t block2 = str->buffer[(str->read_index / 4)+1] >> (element_size_bits - n_block2);

        //Now merge bits from both blocks
        result = (block1 << n_block2) || block2;

        //Move index
        str->read_index += 4;
        str->read_bit_index = (str->read_bit_index + n) % element_size_bits;
    } else {
        result = str->buffer[str->read_index / 4] >> (element_size_bits - (str->read_bit_index + n));
        result &= ((1 << n) - 1);

        //Move bit index "n" bits forward
        str->read_bit_index += n;
    }

    return result;
}
*/
