/*
 * TODO:
 *  - Rewrite iDCT routines and implement faster ones
 */

#include <stdint.h>
#include "mpeg1dec.h"
#include "math.h"
#include "..\generic\bitstream.h"
#include "mpeg1_consts.h"
#include "dct.h"

inline int16_t get_sign(int16_t i)
{
    if(i>0) {
        return 1;
    }else if(i<0) {
        return -1;
    }else {
        return 0;
    }
}

MMFRES mpg1_dequantize_intra(int16_t *dct_table_in, int16_t *dct_table_out, int8_t *quant_matrix, int32_t scale)
{
    /* The quantization scale factor is a multiplicative constant applied to all
     * of the AC coefficients, but not to the DC term.
     */
    int x, y, coeff;

    for(x=0; x<8; x++) {
        for(y=0; y<8; y++) {
            int8_t ind = y * 8 + x;
            int8_t zigzag_ind = __zigzag_coords[ind];

            /* De-quantize coefficient */
            coeff = (2 * dct_table_in[zigzag_ind] * scale * quant_matrix[zigzag_ind]) / 16;

            /* If even, oddify */
            if((coeff & 1) == 0) {
                coeff = coeff - get_sign(dct_table_in[zigzag_ind]);
            }

            if(coeff > 2047) {
                coeff = 2047;
            }
            if(coeff < -2048) {
                coeff = -2048;
            }

            dct_table_out[zigzag_ind] = coeff;
        }
    }

    /* Overrule calculation for DC coef. */
    dct_table_out[0] = dct_table_in[0] * 8;

    return RC_OK;
}

MMFRES mpg1_dequantize_non_intra(int16_t *dct_table_in, int16_t *dct_table_out, int8_t *quant_matrix, int32_t scale)
{
    /* Since here is no DC coefficient prediction, the nonintra block dequantization function
     * does not distinguish between luminance and chrominance blocks, or between DC and AC coeffs.
     */
    int8_t sign;
    int i, j, coeff;

    for(i=0; i<8; i++) {
        for(j=0; j<8; j++) {
            int8_t ind = j * 8 + i;
            int8_t zigzag_ind = __zigzag_coords[ind];

            /* Get sign */
            //sign = dct_table_in[ind] > 0 ? dct_table_in[ind] == 0 ? 0 : 1 : -1;
            sign = get_sign(dct_table_in[zigzag_ind]);

            /* Resultant DCT coeff  */
            coeff = (((2 * dct_table_in[zigzag_ind]) + sign) * scale * quant_matrix[zigzag_ind]) / 16;

            /* If even, oddify */
            if((coeff & 1) == 0) {
                coeff -= sign;
            }

            /* Clamp to [-2048..2047] */
            if(coeff > 2047) {
                coeff = 2047;
            }
            if(coeff < -2048) {
                coeff = -2048;
            }

            /* If quantized coeff is zero, force dequantized coeff to zero also */
            if(dct_table_in[zigzag_ind] == 0) {
                coeff = 0;
            }

            dct_table_out[zigzag_ind] = coeff;
        }
    }

    return RC_OK;
}

/* Positions the bit-stream at the next sequence start code.
 * Returns error if start code not found.
 */
MMFRES mpg1_next_start_code(MMFBitstream *bs)
{
    MMFRES rc;
    uint32_t bits;

    bitstream_align_to_byte(bs);

    do {
        bits = bitstream_peek_bits(bs, 32, &rc);
        if((bits >> 8) == 1) {
            /* Found */
            return RC_OK;
        }

        /* Move to next byte */
        rc = bitstream_discard_bits(bs, 8);
    } while(succeeded(rc));

    return rc;
}

MMFRES mpg1_read_seqence_header(MMFBitstream *bs, MPEG1SeqHeader *target)
{
    MMFRES rc;
    uint32_t bits;
    int i;

    //Read start code
    bits = bitstream_read_bits(bs, 32, &rc);
    if(failed(rc)) return rc;

    if (bits != MPEG2_SEQ_STARTCODE) {
        //The stream is not positioned at the beginning of a sequence header
        return RC_INVALIDARG;
    }

    //Read parameters
    target->width = bitstream_read_bits(bs, 12, NULL);
    target->height = bitstream_read_bits(bs, 12, NULL);

    target->mb_width = (target->width + 15) / 16;
    target->mb_height = (target->height + 15) / 16;

    //Read aspect ratio
    int8_t ind;
    ind = bitstream_read_bits(bs, 4, NULL);
    target->aspect_num = __seq_hdr_aspect_ratio[ind][0];
    target->aspect_den = __seq_hdr_aspect_ratio[ind][1];

    //Read frame rate
    ind = bitstream_read_bits(bs, 4, NULL);
    target->frame_rate_num = __seq_hdr_frame_rate[ind][0];
    target->frame_rate_den = __seq_hdr_frame_rate[ind][1];

    //Read bitrate
    target->bitrate= bitstream_read_bits(bs, 18, NULL);
    target->bitrate *= 400; //bitrate is stored in units of 400 bits/s.

    //TODO: make skip function
    bitstream_discard_bits(bs, 1); //skip marker

    //Read more parameters
    target->vbv_buff_size = bitstream_read_bits(bs, 10, NULL);
    target->constrained_flag = bitstream_read_bits(bs, 1, NULL);

    //Read quantization matrix flags (and matrix itself) for intra-frames
    int8_t has_intra_qm;
    has_intra_qm = bitstream_read_bits(bs, 1, NULL);

    //TODO: following load mechanism won't work. Matrix should be load in
    //zigzag
    if(has_intra_qm) {
        for(i=0; i<64; i++) {
            //target->quant_matrix_intra[i] = bitstream_read_bits(bs, 8, NULL); //TODO
        }
    }

    //Read quantization matrix flags (and matrix itself) for non-intra-frames
    int8_t has_non_intra_qm;
    has_non_intra_qm = bitstream_read_bits(bs, 1, NULL);

    if(has_non_intra_qm) {
        for(i=0; i<64; i++) {
            //target->quant_matrix_non_intra[i] = bitstream_read_bits(bs, 8, NULL); //TODO
        }
    }

    /*
     * TODO: if we will support MPEG-2 Part-2, then check if this
     * header is followed by sequence extension header, and load it
     * using mpg1_read_sequence_header_ex()
    */
    return RC_OK;
}

/* Reads a Group-of-Pictures header
 */
MMFRES mpg1_read_group_header(MMFBitstream *bs, MPEG1GroupHeader *g)
{
    uint32_t bits;

    //Read start code
    bits = bitstream_read_bits(bs, 32, NULL);

    #ifdef DEBUG
    if(bits != MPEG2_GOP_STARTCODE) {
        return RC_INVALIDARG;
    }
    #endif

    //Read drop flag
    g->drop_flag = bitstream_read_bits(bs, 1, NULL);

    //Read time code
    g->hour = bitstream_read_bits(bs, 5, NULL);
    g->minute = bitstream_read_bits(bs, 6, NULL);
    bitstream_discard_bits(bs, 1); //discard
    g->second = bitstream_read_bits(bs, 6, NULL);
    g->frame = bitstream_read_bits(bs, 6, NULL);

    //Read closed & broken flags
    g->closed_flag = bitstream_read_bits(bs, 1, NULL);
    g->broken_flag = bitstream_read_bits(bs, 1, NULL);

    //Discard remaining 5 bits
    bitstream_discard_bits(bs, 5);

    return RC_OK;
}

/*
 * Reads MPEG-1/2 Picture header from bitstream.
 */
MMFRES mpg1_read_picture_header(MMFBitstream *bs, MPEG1PictureHeader *picture)
{
    uint32_t bits;

    //Read start code
    bits = bitstream_read_bits(bs, 32, NULL);

    #ifdef DEBUG
    if(bits != MPEG2_PICTURE_STARTCODE) {
        return RC_INVALIDARG;
    }
    #endif

    /* The temporal_reference (seq_number) is the picture number in the sequence, modulo 1024 */
    picture->seq_number = bitstream_read_bits(bs, 10, NULL);
    picture->frame_type = bitstream_read_bits(bs,  3, NULL);
    if(picture->frame_type != MPEG2_FRAME_TYPE_I && picture->frame_type != MPEG2_FRAME_TYPE_B && picture->frame_type != MPEG2_FRAME_TYPE_P && picture->frame_type != MPEG2_FRAME_TYPE_D) {
        mmf_log(NULL, 0, "Invalid picture type.\n");
        return RC_INVALIDDATA;
    }

    picture->vbv_delay = bitstream_read_bits(bs, 16, NULL);

    //Read additional fields related to P and B frames
    if(picture->frame_type == MPEG2_FRAME_TYPE_B || picture->frame_type == MPEG2_FRAME_TYPE_P) {
        picture->forward_vec_full_pel = bitstream_read_bits(bs,  1, NULL);
        picture->forward_f_code = bitstream_read_bits(bs,  3, NULL);
    }

    //Read fields related only for B frames
    if(picture->frame_type == MPEG2_FRAME_TYPE_B) {
        picture->backward_vec_full_pel = bitstream_read_bits(bs,  1, NULL);
        picture->backward_f_code = bitstream_read_bits(bs,  3, NULL);
    }

    //We will read the extra picture information in temp buffer
    //uint8_t padding_buf[4096]; //4k
    uint32_t buff_ptr = 0;

    //Read extra picture information
    bits = bitstream_peek_bits(bs, 1, NULL);
    bitstream_discard_bits(bs, 1);

    ////Discard 3 bits
    //bitstream_discard_bits(bs, 3);

    while(bits) {
        bitstream_discard_bits(bs, 1); //discard the current '1' bit

        //Read one byte
        bits = bitstream_read_bits(bs, 8, NULL);

        //Add it to temp buffer
        //padding_buf[buff_ptr++] = bits;

        //Peek next 1 bit
        bits = bitstream_peek_bits(bs, 1, NULL);
    }

    /* Discard enough bits to reach byte-alignment */
    //while(bitstream_is_byte_aligned(bs) == RC_FALSE) {
    //    bitstream_discard_bits(bs, 1);
    //}
    bitstream_align_to_byte(bs);

    /*
     * TODO: for MPEG-2 support, we should parse for picture extension data
     * and user data.
     */
    return RC_OK;
}

/*
 * Reads MPEG-1/2 Slice header from bitstream.
 */
MMFRES mpg1_read_slice_header(MMFBitstream *bs, MPEG1SliceHeader *slice)
{
    uint32_t bits;

    //Read start code
    bits = bitstream_read_bits(bs, 32, NULL);

    #ifdef DEBUG
    if(bits < MPEG2_SLICE_MIN_STARTCODE || bits > MPEG2_SLICE_MAX_STARTCODE) {
        return RC_INVALIDARG;
    }
    #endif

    slice->row = (bits & 0xFF) - 1;

    /* Read quantizer scale factor */
    slice->quant_scale = bitstream_read_bits(bs, 5, NULL);

    /* Read extra info */
    //bits = bitstream_read_bits(bs, 1, NULL);
    while((bits = bitstream_peek_bits(bs, 1, NULL)) == 1) {
    	bitstream_discard_bits(bs, 1); //discard '1'
        bitstream_discard_bits(bs, 8); //discard extra info

        //Peek next 1 bit
        bits = bitstream_peek_bits(bs, 1, NULL);
        if(bits) {
            bitstream_discard_bits(bs, 1);
        }
    }
    bitstream_discard_bits(bs, 1);

    /* Initialize last_dc_* fields, which are used to for DC prediction across
     * different blocks in slice
     */
    slice->last_dc_y = 0;
    slice->last_dc_cb = 0;
    slice->last_dc_cr = 0;

    return RC_OK;
}

MMFRES mpg1_decode_coeffs(MPEG1DecoderContext *dec, int16_t *dct, int read_dc)
{
    MMFRES rc;
    RunLevel rl_buff[64];
    int rl_index = 0;
    int i;
    int j;
    uint32_t bits;
    uint8_t rl_code;
    int32_t decoded_bytes;
    int pass=0;

    if(!read_dc) {
        /* If read_dc==false, then DC coeff is already read, and we have to read the following
         * (AC) coeffs.
         */
        pass = 1;
    }

    /* Decode all run-levels. There is an exception with END_OF_BLOCK code. Since it can't
     * appear before the DC coeff (pass 0), therefore it's code (10) is treated as 0/1 run level.
     * On second pass and so on, it is treated as EOB.
     */
    while(((bits = bitstream_peek_bits(dec->bs, 2, &rc)) != MPEG2_END_OF_BLOCK) || (pass==0)) {
        /* Handle '10' and '11' bit strings differently for first coeff */
        if(pass==0) {
            int8_t bits;

            bits = bitstream_peek_bits(dec->bs, 2, &rc);
            if(failed(rc)) return rc;

            if(bits == 2) { //'10' is run level 0/1
                rl_code = 2; //map it to '110'
                bitstream_discard_bits(dec->bs, 2);
            }else if(bits == 3) { //'11' is run level 0/-1
                rl_code = 3; //map it to '111'
                bitstream_discard_bits(dec->bs, 2);
            }else {
                /* It's neither 10 or 11, so use traditional vlc decoding */
                vlc_decode_bitstream(dec->bs, dec->vlc_run_levels, 1, &rl_code, &decoded_bytes);
            }
        }else {
            vlc_decode_bitstream(dec->bs, dec->vlc_run_levels, 1, &rl_code, &decoded_bytes);
        }

        /* Combinations of run-levels which are not found in vlc_run_level table
         * are coded by escape code, followed by a six-bit code for run length
         * and eight or 16-bit code for level.
         */
        if(rl_code == RL_ESCAPE_CODE) {
            rl_buff[rl_index].zero_cnt = bitstream_read_bits(dec->bs, 6, &rc);

            /* Read level */
            int32_t l = bitstream_read_bits(dec->bs, 8, &rc);

            if(l == 0) {
                l = bitstream_read_bits(dec->bs, 8, NULL);
            }else if(l == 128) {
                l = (int32_t)bitstream_read_bits(dec->bs, 8, NULL) - 256;
            }else {
                l = (l << 24) >> 24;
            }

            rl_buff[rl_index].coeff = (int16_t)l;
            rl_index ++;

            continue;
        }

        /* Add to run-level array */
        rl_buff[rl_index++] = __run_levels[rl_code];
        pass++;
    }

    /* Discard end_of_block bits (10) */
    bitstream_discard_bits(dec->bs, 2);

    /* Decode run-level codes and deploy them to DCT buffer
     */
    int zigzag_idx = 0;
    if(!read_dc) zigzag_idx++;

    for(i=0; i<rl_index; i++) {
        for(j=0; j<rl_buff[i].zero_cnt; j++) {
            dct[__zigzag_coords[zigzag_idx++]] = 0;
        }
        dct[__zigzag_coords[zigzag_idx++]] = rl_buff[i].coeff;
    }

    /* Remaining coeffs are filled with zeroes */
    for(i=zigzag_idx; i<64; i++) {
        dct[__zigzag_coords[i]] = 0;
    }

    return RC_OK;
}

/*
 * Reads MPEG-1/2 Block from bitstream.
 */
MMFRES mpg1_read_coded_block(MPEG1DecoderContext *dec, MPEG1MacroblockHeader *mb, MPEG1SliceHeader *s, int8_t pic_type, int8_t block_type, int8_t *dct)
{
    MMFRES rc;
    int32_t diff;
    int32_t decoded_symbols;
    int read_dc;
    int8_t dc_size;
    int16_t temp_dct[64], temp_dct2[64];

    if(mb->t_intra) {
        if(block_type <= MPEG2_BLOCK_TYPE_Y4) {
            /* Luminance block (Y1 to Y4)
             */
            vlc_decode_bitstream(dec->bs, dec->vlc_dc_size_luma, 1, &dc_size, &decoded_symbols);

            /* If size not zero -> read "dc_size" bits, which is delta-DC. Since DC coefficients in neighbour blocks
             * are likely similar, each Y block's DC coefficient is coded as a difference between the previous Y block's DC,
             * which spans across the macroblock.
             */
            if(dc_size) {
                diff = bitstream_read_bits(dec->bs, dc_size, &rc);

                /* If difference is negative, 1 is subtracted.
                 */
                if(!(diff & __bit_test[32-dc_size])) {
                  diff = __bit_mask_r[dc_size] | (diff + 1);
                }
                //if(diff >> (dc_size-1) == 0) {
                //    diff = (0xFFFFFFFF << (dc_size-1)) | (diff + 1);
                //}
                *temp_dct = diff;
            } else {
                /* If dc_size = 0, it means that DC coeff is zero
                 */
                *temp_dct = 0;
            }
        }else {
            /* Chrominance block (CR & CB)
             */
            vlc_decode_bitstream(dec->bs, dec->vlc_dc_size_chroma, 1, &dc_size, &decoded_symbols);

            if(dc_size) {
                diff = bitstream_read_bits(dec->bs, dc_size, &rc);
                if(!(diff & __bit_test[32-dc_size])) {
                  diff = __bit_mask_r[dc_size] | (diff + 1);
                }
                //diff = diff << 3;
                *temp_dct = diff;
            } else {
                /* If dc_size = 0, it means that DC coeff is zero
                 */
                *temp_dct = 0;
            }
        }

        /* Decoded run-level indexes should not overflow byte boundary. */
        if(decoded_symbols != 1) {
            return RC_INVALIDDATA;
        }

        read_dc = 0;
    } else { /* If non_intra mb */
        /* Lift "read_dc" flag, to notify mpg1_decode_ac_coeffs() that DC is not yet
         * read from bit-stream, and it should read it.
         */
        read_dc = 1;
    }

    /* For D-frames, AC coeffs and EOB code are skipped.
     */
    if(pic_type == MPEG2_FRAME_TYPE_D) {
        return RC_OK;
    }

    /* Read DCT coeffs. If read_dc==0 it will treat '10' bit-sequence as EOB.
     * Otherwise the first-appeared '10' will be treated as run level 1/1, and
     * all the following '10' will be treated as EOB.
     */
    rc = mpg1_decode_coeffs(dec, temp_dct, read_dc);

    /* Dequantize */
    if(mb->t_intra) {
        mpg1_dequantize_intra(temp_dct, temp_dct2, dec->qm_intra, mb->quant_scale);

        if(pic_type == MPEG2_FRAME_TYPE_I) {
			/* Perform DC prediction
			 */
			switch(block_type) {
			case MPEG2_BLOCK_TYPE_Y1:
			case MPEG2_BLOCK_TYPE_Y2:
			case MPEG2_BLOCK_TYPE_Y3:
			case MPEG2_BLOCK_TYPE_Y4:
				temp_dct2[0] += s->last_dc_y;
				s->last_dc_y = temp_dct2[0];
				break;
			case MPEG2_BLOCK_TYPE_CB:
				temp_dct2[0] += s->last_dc_cb;
				s->last_dc_cb = temp_dct2[0];
				break;
			case MPEG2_BLOCK_TYPE_CR:
				temp_dct2[0] += s->last_dc_cr;
				s->last_dc_cr = temp_dct2[0];
				break;
			}
        }
    }else {
        mpg1_dequantize_non_intra(temp_dct, temp_dct2, dec->qm_inter, mb->quant_scale);
    }

    /* Perform iDCT */
    //mpg1_idct_2d(temp_dct);
    mmf_idct(temp_dct2);

    int i;

    /* Copy-back temp buffer and clamp values between [0..255] */
    for(i=0; i<64; i++) {
        if(temp_dct2[i] > 255) {
            dct[i] = 255;
        }else if(temp_dct2[i] < 0) {
            dct[i] = 0;
        }else {
            dct[i] = temp_dct2[i];
        }
    }

    return rc;
}

/* Reads a motion vector inside a macroblock header */
MMFRES mpg1_read_mb_motion_vector(MPEG1DecoderContext *dec, MPEG1Picture *pic, MPEG1MacroblockHeader *mb, int32_t mb_address, int fwd)
{
    int32_t decoded_bytes;
    int8_t f, r, residual_size, motion_residual;
    int8_t sign;
    int32_t dMD;
    MMFRES rc;

    /* TODO: Calculate MOTION DISPLACEMENT
     */

    if(fwd) {
        /* Decode horizontal forward motion vector. It ranges between [-16..+16] */
        vlc_decode_bitstream(dec->bs, dec->vlc_motion_code, 1, &mb->mv_fwd.horiz, &decoded_bytes);

        //motion_residual = ???
        residual_size = pic->hdr.forward_f_code - 1;
        f = 1 << residual_size;
        mmf_assert(f>=1);

        if((f != 1) && (mb->mv_fwd.horiz != 0)) {
            /* Read "r_size" bits */
            r = bitstream_read_bits(dec->bs, residual_size, &rc);
            if(failed(rc)) return rc;

            /* The principal part, which we designate by dMB is signed int, given by product
             * of motionc_code x f */
            dMD = 1 + f * (abs(mb->mv_fwd.horiz) - 1);

            /* The residual is a positive integer */
            //r = abs(dMDp) - abs(dMD);

            //dMD = dMD + r;

            //mb->mv_fwd.horiz = dMB + PMD;
            //range = 32 * f;

            /*
            if(MD > max) {
                MD = MD - range;
            }
            if(MD < min) {
                MD = MD + range;
            }
            */
        }
        //PMD = ...
        //pic->mv_forward[mb_address] = PMD;

        /* Decode vertical forward motion vector */
        vlc_decode_bitstream(dec->bs, dec->vlc_motion_code, 1, &mb->mv_fwd.vert, &decoded_bytes);

        if((f != 1) && (mb->mv_fwd.vert != 0)) {
            /* Read "r_size" bits */
            r = bitstream_read_bits(dec->bs, residual_size, &rc);
            if(failed(rc)) return rc;

            //TODO
        }
    }else {
        /* Decode horizontal backward motion vector */
        vlc_decode_bitstream(dec->bs, dec->vlc_motion_code, 1, &mb->mv_bwd.horiz, &decoded_bytes);

        if(pic->hdr.backward_f_code != 1 && mb->mv_bwd.horiz) {
            residual_size = pic->hdr.forward_f_code - 1;
            f = (int8_t)pow(2, residual_size);

            /* Read "r_size" bits */
            r = bitstream_read_bits(dec->bs, residual_size, &rc);
            if(failed(rc)) return rc;

            //TODO: decode mv_bwd.horiz_r
            return RC_FALSE;
        }

        /* Decode vertical backward motion vector */
        vlc_decode_bitstream(dec->bs, dec->vlc_motion_code, 1, &mb->mv_bwd.vert, &decoded_bytes);

        if(pic->hdr.backward_f_code != 1 && mb->mv_bwd.vert) {
            residual_size = pic->hdr.forward_f_code - 1;
            f = (int8_t)pow(2, residual_size);

            /* Read "r_size" bits */
            r = bitstream_read_bits(dec->bs, residual_size, &rc);
            if(failed(rc)) return rc;

            //TODO: decode mv_bwd.vert_r
            return RC_FALSE;
        }
    }

    return RC_OK;
}

/*
 * Reads MPEG-1/2 Macroblock header from bitstream.
 */
MMFRES mpg1_read_mb(MPEG1DecoderContext *dec, MPEG1Picture *pic, MPEG1SliceHeader *slice, MPEG1MacroblockHeader *mb, int32_t mb_address, int8_t *y, int8_t *u, int8_t *v)
{
    MMFRES rc;
    uint8_t type;
    int escape_cnt = 0;
    int decoded_bytes;
    int i;

    /* Discard stuffing bits */
    while(bitstream_peek_bits(dec->bs, 11, &rc) == 0x0F) { //0000 0001 111
        bitstream_discard_bits(dec->bs, 11);
    }

    /* Handle escape codes */
    while(bitstream_peek_bits(dec->bs, 11, &rc) == 0x08) { //0000 0001 000
        escape_cnt++;
        bitstream_discard_bits(dec->bs, 11);
    }

    /* Decode macroblock address increment (1 to 11 bits), using VLC decoder */
    vlc_decode_bitstream(dec->bs, dec->vlc_mb_addr_increment, 1, &mb->address_increment, &decoded_bytes);

    /* Calculate total macroblock address increment */
    mb->address_increment += 33 * escape_cnt;

    /* Finds actual YUV buffer offsets, for this particular mb address */
    int8_t *y_offs = y + (mb->address_increment + mb_address) * 64 * 4;
    int8_t *u_offs = u + (mb->address_increment + mb_address) * 64;
    int8_t *v_offs = v + (mb->address_increment + mb_address) * 64;

	/* If there are macroblocks skipped, reset DC prediction values */
	if(mb->address_increment != 1) {
		slice->last_dc_y = 1024;
		slice->last_dc_cb = 1024;
		slice->last_dc_cr = 1024;
	}

    #ifdef DEBUG
    /* addr increment should decode to a value between 1 and 33 */
    mmf_assert(decoded_bytes == 1);
    #endif //DEBUG

    /* Decode macroblock type (1 to 6 bits) */
    switch (pic->hdr.frame_type) {
    case MPEG2_FRAME_TYPE_I:
        vlc_decode_bitstream(dec->bs, dec->vlc_mb_type_i, 1, &type, &decoded_bytes);

        #ifdef DEBUG
        if(type != 0x10 && type != 0x11) {
            return RC_INVALIDDATA;
        }
        #endif // DEBUG

        break;
    case MPEG2_FRAME_TYPE_P:
        vlc_decode_bitstream(dec->bs, dec->vlc_mb_type_p, 1, &type, &decoded_bytes);
        break;
    case MPEG2_FRAME_TYPE_B:
        vlc_decode_bitstream(dec->bs, dec->vlc_mb_type_b, 1, &type, &decoded_bytes);
        break;
    case MPEG2_FRAME_TYPE_D:
        vlc_decode_bitstream(dec->bs, dec->vlc_mb_type_d, 1, &type, &decoded_bytes);
        break;
    default:
        return RC_INVALIDDATA;
    }

    #ifdef DEBUG
    //type should decode to 1 byte value, containing 5 bit flags
    mmf_assert(decoded_bytes == 1);
    #endif //DEBUG

    /* Extract each flags from bit field */
    mb->t_intra = type & 0x10; //10000
    mb->t_pattern = type & 0x08; //01000
    mb->t_motion_backward = type & 0x04; //00100
    mb->t_motion_forward = type & 0x02; //00010
    mb->t_quant = type & 0x01; //00001

    /* For intra macroblocks address incremen should be always 1 */
    if((pic->hdr.frame_type == MPEG2_FRAME_TYPE_I) && (mb->address_increment != 1)) {
        return RC_INVALIDDATA;
    }

    /* Read quantization scale factor */
    if(mb->t_quant) {
        mb->quant_scale = bitstream_read_bits(dec->bs, 5, &rc);
        slice->quant_scale = mb->quant_scale; //???
    }else {
        mb->quant_scale = slice->quant_scale;
    }

    /* Read forward motion vector */
    if(mb->t_motion_forward) {
        rc = mpg1_read_mb_motion_vector(dec, pic, mb, mb_address, 1);
        if(failed(rc)) return rc;
    }

    /* Read backward motion vector */
    if(mb->t_motion_backward) {
        rc = mpg1_read_mb_motion_vector(dec, pic, mb, mb_address, 0);
        if(failed(rc)) return rc;
    }

    /* Read coded block pattern */
    if(mb->t_pattern) {
        vlc_decode_bitstream(dec->bs, dec->vlc_mb_cb_pattern, 1, &mb->coded_block_pattern, &decoded_bytes);
    }else {
        if(mb->t_intra) {
            /* Documentation: Note that for intra-coded macroblocks pattern_code[i] is always one. */
            mb->coded_block_pattern = 0x3F; //1111 11
        }else {
            mb->coded_block_pattern = 0x00;
        }
    }

    /* Read and decode block data */
    for(i=0; i<6; i++) {
        if(!((mb->coded_block_pattern >> (5-i)) & 1)) {
            /* The cb-pattern shows that this block is missing (it's not encoded).
               In this case we resolve it to 0-filled DCT table.
             */
            continue;
        }

        int8_t *dct_ptr;

        /* Get pointer in DCT table for corresponding block. */
        switch(i) {
            case MPEG2_BLOCK_TYPE_Y1: dct_ptr = y_offs; break;
            case MPEG2_BLOCK_TYPE_Y2: dct_ptr = y_offs + 64; break;
            case MPEG2_BLOCK_TYPE_Y3: dct_ptr = y_offs + 128; break;
            case MPEG2_BLOCK_TYPE_Y4: dct_ptr = y_offs + 192; break;
            case MPEG2_BLOCK_TYPE_CB: dct_ptr = u_offs; break;
            case MPEG2_BLOCK_TYPE_CR: dct_ptr = v_offs; break;
        }

        /* Decode block */
        rc = mpg1_read_coded_block(dec, mb, slice, pic->hdr.frame_type, i, dct_ptr);
        if (failed(rc)) return rc; //...?!
    }

    /* For D pictures read 1 bit which marks the end of D-picture macroblock */
    if(pic->hdr.frame_type == MPEG2_FRAME_TYPE_D) {
        i = bitstream_read_bits(dec->bs, 1, &rc);
        if(i != 1) {
            /* Error in bitstream */
            return RC_INVALIDDATA;
        }
    }

    /* Success (or is it?) */
    return RC_OK;
}

/*
 * Reads MPEG-2 sequence extension header from bitstream.
 */
 /*
MMFRES mpg2_read_sequence_header_ex(MMFBitstream *bs, MPEG1SeqHeader *target)
{
    uint32_t bits;

    //Read start code
    bits = bitstream_read_bits(bs, 32, NULL);

    if (bits != MPEG2_SEQ_EX_STARTCODE) {
        //The stream is not positioned at the beginning of a sequence header
        return RC_INVALIDARG;
    }

    //Read the type of the extension
    int8_t ext_type;
    ext_type = bitstream_read_bits(bs, 4, NULL);

    switch(ext_type) {
    //Sequence Extension (standard)
    case 0x01:
        break;

    //Sequence Display Extension
    case 0x02:
        break;

    //Picture Coding Extension
    case 0x08:
        break;

    //Unknown extension
    default:
        return RC_NOTIMPLEMENTED;
    }
}
*/

MMFRES mpg1_picture_free(MPEG1Picture **pic)
{
    //TODO
}

MMFRES mpg1_picture_alloc(int32_t width, int32_t height, MPEG1Picture **pic)
{
    MPEG1Picture *p = mmf_allocz(sizeof(MPEG1Picture));
    if(!p) {
        return RC_OUTOFMEM;
    }

    /* Size of the Y plane */
    int32_t y_size = width * height;

    /* Allocate data buffers */
    p->Y_plane = mmf_allocz(y_size);
    p->U_plane = mmf_allocz(y_size / 4);
    p->V_plane = mmf_allocz(y_size / 4);

    /* Allocate motion vector arrays. Since we don't know the picture
     * type yet, we have to allocate both buffers.
     */
    int mb_w = width / 32;
    int mb_h = height / 32;

    p->mv_backward = mmf_allocz(sizeof(MPEG1MotionVector)* mb_w * mb_h);
    p->mv_forward = mmf_allocz(sizeof(MPEG1MotionVector)* mb_w * mb_h);

    (*pic) = p;
    return RC_OK;
}

MMFRES mpg1_decoder_set_last_refpic(MPEG1DecoderContext *dec, MPEG1Picture *p)
{
	if(dec->ref_pic_penult != NULL) {
		MMFRES rc;

		/* Release penult (one before last) ref picture */
		rc = mpg1_picture_free(&dec->ref_pic_penult);
		if(failed(rc)) return rc;
	}

	dec->ref_pic_penult = dec->ref_pic_last;
	dec->ref_pic_last = p;

	return RC_OK;
}

MMFRES mpg1_decoder_release_refpics(MPEG1DecoderContext *dec)
{
	MMFRES rc;

	if(dec->ref_pic_penult) {
		/* Release penult (one before last) ref picture */
		rc = mpg1_picture_free(&dec->ref_pic_penult);
		if(failed(rc)) return rc;
	}

	if(dec->ref_pic_last) {
		/* Release last ref picture */
		rc = mpg1_picture_free(&dec->ref_pic_last);
		if(failed(rc)) return rc;
	}

	return rc;
}

MMFRES mpg1_decode_picture(MPEG1DecoderContext *dec, MPEG1Picture **pic)
{
	MMFRES rc;
	MPEG1Picture *p;
    uint32_t next_bits;

	rc = mpg1_picture_alloc(dec->seq_hdr->width, dec->seq_hdr->height, &p);
	if(failed(rc)) goto fail;

    /* Read picture header */
    rc = mpg1_read_picture_header(dec->bs, &p->hdr);
    if(failed(rc)) goto fail;

    /* Read slices */
    do {
        MPEG1SliceHeader s;

        /* Read slice header */
        rc = mpg1_read_slice_header(dec->bs, &s);
        if(failed(rc)) goto fail;

        int mbs = s.row * dec->seq_hdr->mb_width;

        /* Get offset in the YUV buffer, corresponding to the slice row
         */
        int8_t *y_ptr = p->Y_plane + (mbs * 64 * 4);
        int8_t *u_ptr = p->U_plane + (mbs * 64);
        int8_t *v_ptr = p->V_plane + (mbs * 64);

        MPEG1MacroblockHeader mb;
        int32_t mb_address = -1;

        /* Iterate and read all macroblocks in current slice
         */
        do {
            /* Check if last peek_bits() has performed well */
            if(failed(rc)) goto fail;

            /* Decode macroblock */
            rc = mpg1_read_mb(dec, p, &s, &mb, mb_address, y_ptr, u_ptr, v_ptr);
            if(failed(rc)) goto success; //goto fail;

            /* Increment macroblock address. */
            mb_address += mb.address_increment;

        } while(bitstream_peek_bits(dec->bs, 23, &rc) != 0);

        /* Locate next start code */
        rc = mpg1_next_start_code(dec->bs);
        if(failed(rc)) return rc;

        /* Peek at next 32 bits. Search for slice start code. */
        next_bits = bitstream_peek_bits(dec->bs, 32, &rc);
        if(failed(rc)) goto fail;
    } while (next_bits >= MPEG2_SLICE_MIN_STARTCODE && next_bits <= MPEG2_SLICE_MAX_STARTCODE);

success:
    *pic = p;

    /* Success */
    return RC_OK;

fail:
    mpg1_picture_free(&p);
    return rc;
}

/*
 * Allocates and initializes a MPEG-1 decoder context.
 */
MMFRES mpg1_decoder_create(MPEG1DecoderContext **dec, char *filename)
{
    MMFRES rc;
    MPEG1DecoderContext *d = mmf_allocz(sizeof(MPEG1DecoderContext));

    /* Create bit-stream reader
     */
    d->bs = bitstream_alloc_load_file(filename, &rc);
    if(failed(rc)) goto fail;

    /* Generate VLC trees, which later will be used to decode different parts of the bitstream.
     * Passing 0 to element_size means that vlc_tree_create2() will add every prefix node until
     * it reach a node with bit_count=0.
     */
    rc = vlc_tree_create2(__vlc_mb_addr_increment, 0, &d->vlc_mb_addr_increment);
    if(failed(rc)) goto fail;

    rc = vlc_tree_create2(__vlc_mb_type_i, 0, &d->vlc_mb_type_i); //macroblock type for I-frames
    if(failed(rc)) goto fail;

    rc = vlc_tree_create2(__vlc_mb_type_p, 0, &d->vlc_mb_type_p);
    if(failed(rc)) goto fail;

    rc = vlc_tree_create2(__vlc_mb_type_b, 0, &d->vlc_mb_type_b);
    if(failed(rc)) goto fail;

    rc = vlc_tree_create2(__vlc_mb_type_d, 0, &d->vlc_mb_type_d);
    if(failed(rc)) goto fail;

    rc = vlc_tree_create2(__vlc_mb_cb_pattern, 0, &d->vlc_mb_cb_pattern);
    if(failed(rc)) goto fail;

    rc = vlc_tree_create2(__vlc_motion_code, 0, &d->vlc_motion_code);
    if(failed(rc)) goto fail;

    rc = vlc_tree_create2(__vlc_dc_size_y, 0, &d->vlc_dc_size_luma);
    if(failed(rc)) goto fail;

    rc = vlc_tree_create2(__vlc_dc_size_c, 0, &d->vlc_dc_size_chroma);
    if(failed(rc)) goto fail;

    rc = vlc_tree_create2(__vlc_run_levels, 0, &d->vlc_run_levels);
    if(failed(rc)) goto fail;

    /* Load default quantization matrices */
    memcpy(&d->qm_intra, __quant_matrix_intra, 64);
    memcpy(&d->qm_inter, __quant_matrix_non_intra, 64);

    /* Initialize decoder by passing NULL sample to mpg1_decode_sample() */
    mpg1_decode_sample(d, NULL);

    /* Copy pointer of the decoder context to the output parameter.
     */
    (*dec) = d;
    return RC_OK;

fail:
    mpg1_decoder_free(&d);
    return rc;
}

MMFRES mpg1_decoder_free(MPEG1DecoderContext **dec)
{
    MPEG1DecoderContext *d = *dec;
    //destroy VLC trees
    vlc_tree_free(&d->vlc_mb_addr_increment);
    //vlc_tree_free(d->vlc_mb_type); //TODO

    mmf_free(*dec);
    *dec = NULL;

    return RC_OK;
}

MMFRES mpg1_perform_prediction(MPEG1DecoderContext *dec, MPEG1Picture *p, MPEG1Picture *refpic)
{
	if(p->hdr.frame_type != MPEG2_FRAME_TYPE_P && p->hdr.frame_type != MPEG2_FRAME_TYPE_B) {
		return RC_INVALIDARG;
	}

	int i;
	int pixels=dec->seq_hdr->width * dec->seq_hdr->height;
	int8_t *src, *dst;

    /* Perform conditional replenishment (frame prediction) */

	dst = p->Y_plane;
	src = refpic->Y_plane;
	for(i=0; i<pixels; i++) {
		*(dst++) += *(src++);
	}

	/* U and V planes has 4 times less pixels */
	pixels /= 4;

	dst = p->U_plane;
	src = refpic->U_plane;
	for(i=0; i<pixels; i++) {
		*(dst++) += *(src++);
	}

	dst = p->V_plane;
	src = refpic->V_plane;
	for(i=0; i<pixels; i++) {
		*(dst++) += *(src++);
	}

	return RC_OK;
}

MMFRES mpg1_decode_sample(MPEG1DecoderContext *dec, MMFSample *sample)
{
    MMFRES rc;

    if(dec->seq_hdr == NULL) {
        /* Allocate sequence header struct */
        dec->seq_hdr = mmf_alloc(sizeof(MPEG1SeqHeader));
        if(!dec->seq_hdr) return RC_OUTOFMEM;

        /* Parse video sequence header */
        rc = mpg1_read_seqence_header(dec->bs, dec->seq_hdr);
        if(failed(rc)) return rc;
    }

    /* Check if we are currently in a GOP */
    if(dec->group == NULL) {
        /* Apparently not. Find and read GOP */
        dec->group = mmf_alloc(sizeof(MPEG1GroupHeader));
        if(!dec->group) return RC_OUTOFMEM;

        rc = mpg1_read_group_header(dec->bs, dec->group);
        if(failed(rc)) return rc;
    }

    if(sample == NULL) {
        /* Passing NULL sample to decoder causes it to initialize only */
        return RC_OK;
    }

    /* Validate sample */
    if(sample->buffer_count == 0 || sample->width != dec->seq_hdr->width || sample->height != dec->seq_hdr->height) {
        /* Sample not initialized correctly */
        return RC_INVALIDARG;
    }

    /* Validate pixel format */
    if(sample->format != SAMPLE_FORMAT_YUV420P) {
        /* Unsupported pix fmt */
        return RC_INVALIDARG;
    }

    /* Read a picture */
    MPEG1Picture *pic;
    rc = mpg1_decode_picture(dec, &pic);
    if(failed(rc)) return rc;

    /* Perform conditional replenishment */
    if(pic->hdr.frame_type == MPEG2_FRAME_TYPE_P || pic->hdr.frame_type == MPEG2_FRAME_TYPE_B) {
		/* Get the reference picture, which we will use as a base
		 * to reconstruct current picture.
		 */
    	rc = mpg1_perform_prediction(dec, pic, dec->ref_pic_last);
    	if(failed(rc)) return rc;
    }

    //fill with zeroes (test)
    /*
    memset(pic->Y_plane, 0, dec->seq_hdr->width * dec->seq_hdr->height);
    memset(pic->U_plane, 64, dec->seq_hdr->width * dec->seq_hdr->height / 4);
    memset(pic->V_plane, 64, dec->seq_hdr->width * dec->seq_hdr->height / 4);

    memset(sample->buffer_data[0], 251, dec->seq_hdr->width * dec->seq_hdr->height);
    memset(sample->buffer_data[1], 100, dec->seq_hdr->width * dec->seq_hdr->height / 4);
    memset(sample->buffer_data[2], 50, dec->seq_hdr->width * dec->seq_hdr->height / 4);
*/

    int mb_x = dec->seq_hdr->mb_width;
    int mb_y = dec->seq_hdr->mb_height;
    int i, j;

    /*
     * Copy decoded picture data to sample (macroblock by macroblock)
     */
    for(i=0; i<mb_x*mb_y; i++) {
        int x = (i * 16) % dec->seq_hdr->width;
        int y = ((i * 16) / dec->seq_hdr->width) * 16;

        int8_t *y1_block = pic->Y_plane + (i * 4 * 64);
        int8_t *y2_block = pic->Y_plane + (i * 4 * 64) + 64;
        int8_t *y3_block = pic->Y_plane + (i * 4 * 64) + 128;
        int8_t *y4_block = pic->Y_plane + (i * 4 * 64) + 192;

        int8_t *y_dst = sample->buffer_data[0] + (y * sample->buffer_stride[0]) + (x);

        /* Copy Y blocks */
        for(j=0; j<8; j++) {
            /* Copy block line by line */
            memcpy(y_dst + (j * sample->buffer_stride[0]), y1_block + (j * 8), 8);
            memcpy(y_dst + (j * sample->buffer_stride[0]) + 8, y2_block + (j * 8), 8);
            memcpy(y_dst + ((j+8) * sample->buffer_stride[0]), y3_block + (j * 8), 8);
            memcpy(y_dst + ((j+8) * sample->buffer_stride[0]) + 8, y4_block + (j * 8), 8);
        }

        int8_t *u_block = pic->U_plane + i * 64;
        int8_t *v_block = pic->V_plane + i * 64;

        int8_t *u_dst = sample->buffer_data[1] + (y * sample->buffer_stride[1] / 2) + (x/2);
        int8_t *v_dst = sample->buffer_data[2] + (y * sample->buffer_stride[2] / 2) + (x/2);

        /* Copy U, V blocks */
        for(j=0; j<8; j++) {
            /* Copy block line by line */
            memcpy(u_dst + (j * sample->buffer_stride[1]), u_block + (j * 8), 8);
            memcpy(v_dst + (j * sample->buffer_stride[2]), v_block + (j * 8), 8);
   //         memset(u_dst + (j * sample->buffer_stride[1]), -128, 8);
   //         memset(v_dst + (j * sample->buffer_stride[2]), -128, 8);
        }
    }

    switch(pic->hdr.frame_type) {
    case MPEG2_FRAME_TYPE_I:
    case MPEG2_FRAME_TYPE_P:
    	/*
    	 * Keep this frame, since it will be used as a reference picture
    	 * when decoding next pictures.
    	 */
    	mpg1_decoder_set_last_refpic(dec, pic);
    	break;
    default:
    	/* Release MMFPicture */
    	mpg1_picture_free(&pic);
    	break;
    }

    return rc;
}
