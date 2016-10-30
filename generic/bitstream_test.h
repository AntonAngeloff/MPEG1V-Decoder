#ifndef BITSTREAM_TEST_H_INCLUDED
#define BITSTREAM_TEST_H_INCLUDED

#include "bitstream.h"
#include "..\mmfsample.h"
#include "..\codec\mpeg1dec.h"
#include "..\codec\dct.h"

//Define some random data to test our bit reader
const uint8_t data[] = {
    0b11100000, 0b11111000, 0b00101010, 0b10101010,
    0b00001111, 0b00001111, 0b00001111, 0b00001111,
};
const int arr_length = 8;

void print_bits(uint32_t val, char *prefix, char *suffix)
{
    int i;

    if(prefix) {
        printf(prefix);
    }

    for(i=0; i<32; i++) {
        printf("%d", (val >> (31-i)) & 1);
    }

    if(suffix) {
        printf(suffix);
    }
}

void print_bits_b(uint32_t val, char *prefix, char *suffix)
{
    int i;

    if(prefix) {
        printf(prefix);
    }

    for(i=0; i<8; i++) {
        printf("%d", (val >> (7-i)) & 1);
    }

    if(suffix) {
        printf(suffix);
    }
}

void bitstream_test()
{
    MMFBitstream *bs = bitstream_alloc(1024*1024*1024); //1mb capacity
    bitstream_write(bs, data, arr_length*sizeof(uint8_t));

    printf("Dumping whole buffer...\n");

    int i;
    for(i=0; i<arr_length; i++) {
        print_bits_b(data[i], NULL, " ");
    }
    printf("\n\n");

    uint32_t bits;

    //Read 3 bits
    bitstream_read_bits(bs, 3, &bits);
    print_bits(bits, "Reading 3 bits: ", "\n");

    //Read 5 bits
    bitstream_read_bits(bs, 5, &bits);
    print_bits(bits, "Reading 5 bits: ", "\n");

    //Read 10 bits
    bitstream_read_bits(bs, 10, &bits);
    print_bits(bits, "Reading 10 bits: ", "\n");

    printf("Press <any> key to exit...\n");
    getch();

    bitstream_free(&bs);
}

void bitstream_test_file(char *fn)
{
    MMFBitstream *bs = bitstream_alloc_load_file(fn, NULL); //discard return code
    if(bs == NULL) {
        printf("Failed to create bitstream from file '%s'.", fn);
        getch();
        return;
    }

    printf("Dumping sequence header for '%s'...\n", fn);
    printf("------------------------------------------\n");

    //Read 1 seq header
    MPEG1SeqHeader seq_hdr;
    mpg1_read_seqence_header(bs, &seq_hdr);

    //
    printf("width: %d\n", seq_hdr.width);
    printf("height: %d\n", seq_hdr.height);
    printf("bitrate: %d bits/s\n", seq_hdr.bitrate);
    printf("frame rate: %f\n", (float)seq_hdr.frame_rate_num / seq_hdr.frame_rate_den);
    printf("aspect ratio: %d:%d\n", seq_hdr.aspect_num, seq_hdr.aspect_den);
    printf("cursor pos: %d:%d\n", bs->read_index, bs->read_bit_index % 8);

    MPEG1GroupHeader g;
    mpg1_read_group_header(bs, &g);

    printf("\nDumping first GOP header...\n");
    printf("------------------------------------------\n");
    printf("drop: %d\n", g.drop_flag);
    printf("time code: %d:%d:%d:%d\n", g.hour, g.minute, g.second, g.frame);
    printf("closed: %d\n", g.closed_flag);
    printf("broken: %d\n", g.broken_flag);
    printf("cursor pos: %d:%d\n", bs->read_index, bs->read_bit_index % 8);

    MPEG1PictureHeader p;
    mpg1_read_picture_header(bs, &p);

    char frame_type = '_';
    switch(p.frame_type) {
    case MPEG2_FRAME_TYPE_I:
        frame_type = 'I';
        break;
    case MPEG2_FRAME_TYPE_P:
        frame_type = 'P';
        break;
    case MPEG2_FRAME_TYPE_B:
        frame_type = 'B';
        break;
    case MPEG2_FRAME_TYPE_D:
        frame_type = 'D';
        break;
    }

    printf("\nDumping first picture header...\n");
    printf("------------------------------------------\n");
    printf("seq number: %d\n", p.seq_number);
    printf("frame type: %c\n", frame_type);
    printf("vbv delay: %d\n", p.vbv_delay);
    printf("forward f code: %d\n", p.forward_f_code);
    printf("cursor pos: %d:%d\n", bs->read_index, bs->read_bit_index % 8);

    bitstream_free(&bs);

    printf("Creating decoder...");
    MPEG1DecoderContext *dec;

    if(failed(mpg1_decoder_create(&dec, fn))) {
        printf("failed.\n");
        getch();
        return;
    } else {
        printf("done.\n");
    }

    //mpg1_decode_sample(dec, NULL); //force decoder to init

    /* Allocate video frame */
    MMFSample *sample = NULL;
    mmf_allocate_video_frame(SAMPLE_FORMAT_YUV420P, dec->seq_hdr->width, dec->seq_hdr->height, &sample);

    MMFRES rc;
    rc = mpg1_decode_sample(dec, sample); //decode 1st frame
    printf("Decoding single frame... retcode=%d\n", rc);

    rc = mpg1_decode_sample(dec, sample); //decode 2nd frame
    printf("Decoding single frame... retcode=%d\n", rc);

    /* Save sample to file */
    FILE *fout = fopen("d:\\out.yuv", "wb");

    int w = sample->width;
    int h = sample->height;

    mmf_sample_write_plane(fout, w, sample->buffer_data[0], w, w, h);
    mmf_sample_write_plane(fout, w/2, sample->buffer_data[1], w/2, w/2, h/2);
    mmf_sample_write_plane(fout, w/2, sample->buffer_data[2], w/2, w/2, h/2);

    //fclose(fout);

    mmf_sample_free(&sample);
    printf("\nPress <any> key to exit...\n");
    getch();
}

void test_vld()
{
    uint32_t code = 0x267; //0100 1100 111 (ABACAD)
    VLCTreeNode *vlc_tree;
    MMFRES rc;

    char buffer[100];
    int32_t buffer_len = 0;

    VLCPrefixEntry entries[] = {
        {0x00,  1,  'A'}, //0   = A
        {0x02,  2,  'B'}, //10  = B
        {0x06,  3,  'C'}, //110 = C
        {0x07,  3,  'D'}, //111 = D
        {0x00,  0,   0 }, //END
    };
    printf("\nTesting variable length decoder...\n");
    print_bits(code, "code: ", "\n");

    if(failed(vlc_tree_create2(entries, 0, &vlc_tree))) {
        printf("\nFailed to create VLC tree!\n");
        goto getch;
    }

    MMFBitstream *bs = bitstream_alloc(4);

    uint8_t *p = (uint8_t*)&code; //we need to reverse code's endianness
    rc = bitstream_write(bs, p+3, 1);
    rc = bitstream_write(bs, p+2, 1);
    rc = bitstream_write(bs, p+1, 1);
    rc = bitstream_write(bs, p, 1);
    if (failed(rc)) goto fail;

    //Since our code is only 11 bits long, discard the first 21 bits
    rc = bitstream_discard_bits(bs, 21);
    if (failed(rc)) goto fail;

    //Decode
    if (failed(vlc_decode_bitstream(bs, vlc_tree, -1, buffer, &buffer_len))) {
        printf("\nFailed to decode!\n");
    }

    buffer[buffer_len] = '\0';
    printf("Decoded buffer: %s\n", buffer);

    bitstream_free(&bs);
    vlc_tree_free(&vlc_tree);

getch:
    getch();
    return;

fail:
    printf("Failed. RC=%d", rc);
    goto getch;
}

void print_matrix_8b(int8_t *m, int w, int h) {
    int i, j;

    for(i=0; i<w; i++) {
        for(j=0; j<h; j++) {
            int ind = i*w + j;

            if(m[ind] >= 0) printf(" ");
            printf("%.3d ", m[ind]);
        }
        printf("\n");
    }
}

void print_matrix_16b(int16_t *m, int w, int h) {
    int i, j;

    for(i=0; i<w; i++) {
        for(j=0; j<h; j++) {
            int ind = i*w + j;

            if(m[ind] >= 0) printf(" ");
            printf("%.3d ", m[ind]);
        }
        printf("\n");
    }
}

void idct_test() {
    /* Taken from https://en.wikipedia.org/wiki/JPEG to test IDCT routine.
     */
    int16_t in_matrix_1[64] = {
        -415, -30, -61,  27,  56, -20, -2,  0,
           4, -21, -61,  10,  13,  -7, -9,  5,
         -46,   7,  77, -24, -28,  10,  5, -6,
         -48,  12,  34, -14, -10,  6,   2,  2,
          12,  -6, -13,  -4,  -2,  2,  -3,  3,
          -8,   3,   2,  -6,  -2,  1,   4,  2,
          -1,   0,   0,  -2,  -1, -3,   4, -1,
           0,   0,  -1,  -4,  -1,  0,   0,  2,
    };

    int16_t in_matrix_2[64] = {
           1,   1,   1,   1,   1,   1,  1,  1,
           1,   1,   1,   1,   1,   1,  1,  1,
           1,   1,   1,   1,   1,   1,  1,  1,
           1,   1,   1,   1,   1,   1,  1,  1,
           1,   1,   1,   1,   1,   1,  1,  1,
           1,   1,   1,   1,   1,   1,  1,  1,
           1,   1,   1,   1,   1,   1,  1,  1,
           1,   1,   1,   1,   1,   1,  1,  1,
    };

    int16_t *in_matrix = in_matrix_2;

    printf("input matrix:\n");
    print_matrix_16b(in_matrix, 8, 8);
    printf("\n");

    //mpg1_idct_init();
    //mpg1_idct_2d(in_matrix);
    mmf_idct(in_matrix);

    /* Print outputted matrix */
    printf("result matrix:\n");
    print_matrix_16b(in_matrix, 8, 8);
    printf("\n");

    getch();
}

#endif // BITSTREAM_TEST_H_INCLUDED
