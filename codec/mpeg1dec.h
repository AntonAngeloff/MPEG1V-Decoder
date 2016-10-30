#ifndef MPEG1DEC_H_INCLUDED
#define MPEG1DEC_H_INCLUDED

#include "..\mmfutil.h"
#include "..\mmfsample.h"
#include "vlc_coding.h"

//Constants
#define MPEG2_SEQ_STARTCODE     0x000001B3
#define MPEG2_SEQ_ENDCODE       0x000001B7
#define MPEG2_GOP_STARTCODE     0x000001B8
#define MPEG2_SEQ_EX_STARTCODE  0x000001B5
#define MPEG2_PICTURE_STARTCODE 0x00000100
#define MPEG2_SLICE_MIN_STARTCODE   0x00000101
#define MPEG2_SLICE_MAX_STARTCODE   0x000001AF

#define MPEG2_FRAME_TYPE_I      0x1
#define MPEG2_FRAME_TYPE_P      0x2
#define MPEG2_FRAME_TYPE_B      0x3
#define MPEG2_FRAME_TYPE_D      0x4

#define MPEG2_BLOCK_TYPE_Y1     0x00
#define MPEG2_BLOCK_TYPE_Y2     0x01
#define MPEG2_BLOCK_TYPE_Y3     0x02
#define MPEG2_BLOCK_TYPE_Y4     0x03
#define MPEG2_BLOCK_TYPE_CB     0x04
#define MPEG2_BLOCK_TYPE_CR     0x05

#define MPEG2_END_OF_BLOCK      0x02

typedef struct {
    uint8_t zero_cnt;
    int16_t coeff;
} RunLevel;

/*
 * Video Sequence Header. Starts with MPEG2_SEQ_STARTCODE and
 * ends with MPEG2_SEQ_ENDCODE.
 */
typedef struct {
    int32_t width;
    int32_t height;

    int32_t mb_width; //number of macro-blocks horizontally
    int32_t mb_height; //number of vertical macro-blocks

    int32_t bitrate;

    int32_t aspect_num;
    int32_t aspect_den;

    int32_t frame_rate_num;
    int32_t frame_rate_den;

    int32_t vbv_buff_size;
    int8_t constrained_flag;

    uint8_t quant_matrix_intra[8][8];
    uint8_t quant_matrix_non_intra[8][8];
} MPEG1SeqHeader;

/*
 * Group of pictures header
 */
typedef struct {
    int8_t drop_flag;

    /* Time code for the first frame
     */
    int8_t hour;
    int8_t minute;
    int8_t second;
    int8_t frame;

    int8_t closed_flag;
    int8_t broken_flag;
} MPEG1GroupHeader;

/*
 * Picture header
 */
typedef struct {
    int32_t seq_number;
    int8_t frame_type;
    int32_t vbv_delay;

    /*
     * Following two fields are only present in P and B frames.
     *  - forward_vec_full_pel indicates that the motion vector is representing full pixel units.
     *    If equals to 0, it means that it represents half-pixel distance.
     */
    int8_t forward_vec_full_pel;
    int8_t forward_f_code;

    /*
     * Following two fields are only present in B frames.
     */
    int8_t backward_vec_full_pel;
    int8_t backward_f_code;

    /*
     * Time stamps
     */
    int64_t pts;
    int64_t dts;
} MPEG1PictureHeader;

typedef struct {
	int8_t x;
	int8_t y;
} MPEG1MotionVector;

typedef struct {
    MPEG1PictureHeader hdr;

    int8_t *Y_plane;
    int8_t *U_plane;
    int8_t *V_plane;

    MPEG1MotionVector *mv_forward;
    MPEG1MotionVector *mv_backward;
} MPEG1Picture;

/*
 * Slice header
 */
typedef struct {
    int8_t row;
    int8_t quant_scale;

    int16_t last_dc_y;
    int16_t last_dc_cb;
    int16_t last_dc_cr;
} MPEG1SliceHeader;

/* Motion vector
 * ..todo: write more
 */
typedef struct {
    int8_t horiz;
    int8_t horiz_r;
    int8_t vert;
    int8_t vert_r;
} MPEG1MacroblockMotionVector;

/*
 */
typedef struct {
    int8_t address_increment;
    int16_t type;
    int8_t quant_scale;
    int8_t coded_block_pattern;

    /* Differentiation of the 5 control bits
     */
    int8_t t_quant;
    int8_t t_motion_forward;
    int8_t t_motion_backward;
    int8_t t_pattern;
    int8_t t_intra; //If set - inner blocks are coded without reference to blocks from other pictures.

    /* Motion compensation
    */
    MPEG1MacroblockMotionVector mv_fwd;
    MPEG1MacroblockMotionVector mv_bwd;

    /* Pointers to DCT tables to all 6 blocks
    */
    int8_t *blocks[6];
} MPEG1MacroblockHeader;

typedef struct {
    //Bit-stream (we read data from here).
    MMFBitstream *bs;

    //Current sequence header (?)
    MPEG1SeqHeader *seq_hdr;
    MPEG1GroupHeader *group;

    //VLC decoding trees
    VLCTreeNode *vlc_mb_addr_increment;
    VLCTreeNode *vlc_mb_type_i;
    VLCTreeNode *vlc_mb_type_p;
    VLCTreeNode *vlc_mb_type_b;
    VLCTreeNode *vlc_mb_type_d;
    VLCTreeNode *vlc_mb_cb_pattern;
    VLCTreeNode *vlc_motion_code;
    VLCTreeNode *vlc_dc_size_luma;
    VLCTreeNode *vlc_dc_size_chroma;
    VLCTreeNode *vlc_run_levels;

    /* Last decoded picture id */
    uint64_t last_pts;

    /**
     * Keeps track of the last reference frame (either I or P).
     */
    MPEG1Picture *ref_pic_last;

    /**
     * The one before the last reference frame.
     */
    MPEG1Picture *ref_pic_penult;

    /* Current quantization matrices */
    int8_t qm_intra[64];
    int8_t qm_inter[64];
} MPEG1DecoderContext;

MMFRES mpg1_decoder_create(MPEG1DecoderContext **dec, char *filename);
MMFRES mpg1_decoder_free(MPEG1DecoderContext **dec);
MMFRES mpg1_decode_sample(MPEG1DecoderContext *dec, MMFSample *sample);

float mpg2_seq_hdr_get_frame_rate(MPEG1SeqHeader *seq_hdr);

#endif // MPEG1DEC_H_INCLUDED
