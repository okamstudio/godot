/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_config.h"
#include "vp8_rtcd.h"
#include "./vpx_scale_rtcd.h"
#include "onyxd_int.h"
#include "vp8/common/header.h"
#include "vp8/common/reconintra4x4.h"
#include "vp8/common/reconinter.h"
#include "detokenize.h"
#include "vp8/common/common.h"
#include "vp8/common/invtrans.h"
#include "vp8/common/alloccommon.h"
#include "vp8/common/entropymode.h"
#include "vp8/common/quant_common.h"
#include "vpx_scale/vpx_scale.h"
#include "vp8/common/reconintra.h"
#include "vp8/common/setupintrarecon.h"

#include "decodemv.h"
#include "vp8/common/extend.h"
#if CONFIG_ERROR_CONCEALMENT
#include "error_concealment.h"
#endif
#include "vpx_mem/vpx_mem.h"
#include "vp8/common/threading.h"
#include "decoderthreading.h"
#include "dboolhuff.h"
#include "vpx_dsp/vpx_dsp_common.h"

#include <assert.h>
#include <stdio.h>

void vp8cx_init_de_quantizer(VP8D_COMP *pbi)
{
    int Q;
    VP8_COMMON *const pc = & pbi->common;

    for (Q = 0; Q < QINDEX_RANGE; Q++)
    {
        pc->Y1dequant[Q][0] = (short)vp8_dc_quant(Q, pc->y1dc_delta_q);
        pc->Y2dequant[Q][0] = (short)vp8_dc2quant(Q, pc->y2dc_delta_q);
        pc->UVdequant[Q][0] = (short)vp8_dc_uv_quant(Q, pc->uvdc_delta_q);

        pc->Y1dequant[Q][1] = (short)vp8_ac_yquant(Q);
        pc->Y2dequant[Q][1] = (short)vp8_ac2quant(Q, pc->y2ac_delta_q);
        pc->UVdequant[Q][1] = (short)vp8_ac_uv_quant(Q, pc->uvac_delta_q);
    }
}

void vp8_mb_init_dequantizer(VP8D_COMP *pbi, MACROBLOCKD *xd)
{
    int i;
    int QIndex;
    MB_MODE_INFO *mbmi = &xd->mode_info_context->mbmi;
    VP8_COMMON *const pc = & pbi->common;

    /* Decide whether to use the default or alternate baseline Q value. */
    if (xd->segmentation_enabled)
    {
        /* Abs Value */
        if (xd->mb_segement_abs_delta == SEGMENT_ABSDATA)
            QIndex = xd->segment_feature_data[MB_LVL_ALT_Q][mbmi->segment_id];

        /* Delta Value */
        else
            QIndex = pc->base_qindex + xd->segment_feature_data[MB_LVL_ALT_Q][mbmi->segment_id];

        QIndex = (QIndex >= 0) ? ((QIndex <= MAXQ) ? QIndex : MAXQ) : 0;    /* Clamp to valid range */
    }
    else
        QIndex = pc->base_qindex;

    /* Set up the macroblock dequant constants */
    xd->dequant_y1_dc[0] = 1;
    xd->dequant_y1[0] = pc->Y1dequant[QIndex][0];
    xd->dequant_y2[0] = pc->Y2dequant[QIndex][0];
    xd->dequant_uv[0] = pc->UVdequant[QIndex][0];

    for (i = 1; i < 16; i++)
    {
        xd->dequant_y1_dc[i] =
        xd->dequant_y1[i] = pc->Y1dequant[QIndex][1];
        xd->dequant_y2[i] = pc->Y2dequant[QIndex][1];
        xd->dequant_uv[i] = pc->UVdequant[QIndex][1];
    }
}

static void decode_macroblock(VP8D_COMP *pbi, MACROBLOCKD *xd,
                              unsigned int mb_idx)
{
    MB_PREDICTION_MODE mode;
    int i;
#if CONFIG_ERROR_CONCEALMENT
    int corruption_detected = 0;
#else
    (void)mb_idx;
#endif

    if (xd->mode_info_context->mbmi.mb_skip_coeff)
    {
        vp8_reset_mb_tokens_context(xd);
    }
    else if (!vp8dx_bool_error(xd->current_bc))
    {
        int eobtotal;
        eobtotal = vp8_decode_mb_tokens(pbi, xd);

        /* Special case:  Force the loopfilter to skip when eobtotal is zero */
        xd->mode_info_context->mbmi.mb_skip_coeff = (eobtotal==0);
    }

    mode = xd->mode_info_context->mbmi.mode;

    if (xd->segmentation_enabled)
        vp8_mb_init_dequantizer(pbi, xd);


#if CONFIG_ERROR_CONCEALMENT

    if(pbi->ec_active)
    {
        int throw_residual;
        /* When we have independent partitions we can apply residual even
         * though other partitions within the frame are corrupt.
         */
        throw_residual = (!pbi->independent_partitions &&
                          pbi->frame_corrupt_residual);
        throw_residual = (throw_residual || vp8dx_bool_error(xd->current_bc));

        if ((mb_idx >= pbi->mvs_corrupt_from_mb || throw_residual))
        {
            /* MB with corrupt residuals or corrupt mode/motion vectors.
             * Better to use the predictor as reconstruction.
             */
            pbi->frame_corrupt_residual = 1;
            memset(xd->qcoeff, 0, sizeof(xd->qcoeff));

            corruption_detected = 1;

            /* force idct to be skipped for B_PRED and use the
             * prediction only for reconstruction
             * */
            memset(xd->eobs, 0, 25);
        }
    }
#endif

    /* do prediction */
    if (xd->mode_info_context->mbmi.ref_frame == INTRA_FRAME)
    {
        vp8_build_intra_predictors_mbuv_s(xd,
                                          xd->recon_above[1],
                                          xd->recon_above[2],
                                          xd->recon_left[1],
                                          xd->recon_left[2],
                                          xd->recon_left_stride[1],
                                          xd->dst.u_buffer, xd->dst.v_buffer,
                                          xd->dst.uv_stride);

        if (mode != B_PRED)
        {
            vp8_build_intra_predictors_mby_s(xd,
                                                 xd->recon_above[0],
                                                 xd->recon_left[0],
                                                 xd->recon_left_stride[0],
                                                 xd->dst.y_buffer,
                                                 xd->dst.y_stride);
        }
        else
        {
            short *DQC = xd->dequant_y1;
            int dst_stride = xd->dst.y_stride;

            /* clear out residual eob info */
            if(xd->mode_info_context->mbmi.mb_skip_coeff)
                memset(xd->eobs, 0, 25);

            intra_prediction_down_copy(xd, xd->recon_above[0] + 16);

            for (i = 0; i < 16; i++)
            {
                BLOCKD *b = &xd->block[i];
                unsigned char *dst = xd->dst.y_buffer + b->offset;
                B_PREDICTION_MODE b_mode =
                    xd->mode_info_context->bmi[i].as_mode;
                unsigned char *Above = dst - dst_stride;
                unsigned char *yleft = dst - 1;
                int left_stride = dst_stride;
                unsigned char top_left = Above[-1];

                vp8_intra4x4_predict(Above, yleft, left_stride, b_mode,
                                     dst, dst_stride, top_left);

                if (xd->eobs[i])
                {
                    if (xd->eobs[i] > 1)
                    {
                    vp8_dequant_idct_add(b->qcoeff, DQC, dst, dst_stride);
                    }
                    else
                    {
                        vp8_dc_only_idct_add
                            (b->qcoeff[0] * DQC[0],
                                dst, dst_stride,
                                dst, dst_stride);
                        memset(b->qcoeff, 0, 2 * sizeof(b->qcoeff[0]));
                    }
                }
            }
        }
    }
    else
    {
        vp8_build_inter_predictors_mb(xd);
    }


#if CONFIG_ERROR_CONCEALMENT
    if (corruption_detected)
    {
        return;
    }
#endif

    if(!xd->mode_info_context->mbmi.mb_skip_coeff)
    {
        /* dequantization and idct */
        if (mode != B_PRED)
        {
            short *DQC = xd->dequant_y1;

            if (mode != SPLITMV)
            {
                BLOCKD *b = &xd->block[24];

                /* do 2nd order transform on the dc block */
                if (xd->eobs[24] > 1)
                {
                    vp8_dequantize_b(b, xd->dequant_y2);

                    vp8_short_inv_walsh4x4(&b->dqcoeff[0],
                        xd->qcoeff);
                    memset(b->qcoeff, 0, 16 * sizeof(b->qcoeff[0]));
                }
                else
                {
                    b->dqcoeff[0] = b->qcoeff[0] * xd->dequant_y2[0];
                    vp8_short_inv_walsh4x4_1(&b->dqcoeff[0],
                        xd->qcoeff);
                    memset(b->qcoeff, 0, 2 * sizeof(b->qcoeff[0]));
                }

                /* override the dc dequant constant in order to preserve the
                 * dc components
                 */
                DQC = xd->dequant_y1_dc;
            }

            vp8_dequant_idct_add_y_block
                            (xd->qcoeff, DQC,
                             xd->dst.y_buffer,
                             xd->dst.y_stride, xd->eobs);
        }

        vp8_dequant_idct_add_uv_block
                        (xd->qcoeff+16*16, xd->dequant_uv,
                         xd->dst.u_buffer, xd->dst.v_buffer,
                         xd->dst.uv_stride, xd->eobs+16);
    }
}

static int get_delta_q(vp8_reader *bc, int prev, int *q_update)
{
    int ret_val = 0;

    if (vp8_read_bit(bc))
    {
        ret_val = vp8_read_literal(bc, 4);

        if (vp8_read_bit(bc))
            ret_val = -ret_val;
    }

    /* Trigger a quantizer update if the delta-q value has changed */
    if (ret_val != prev)
        *q_update = 1;

    return ret_val;
}

#ifdef PACKET_TESTING
#include <stdio.h>
FILE *vpxlog = 0;
#endif

static void yv12_extend_frame_top_c(YV12_BUFFER_CONFIG *ybf)
{
    int i;
    unsigned char *src_ptr1;
    unsigned char *dest_ptr1;

    unsigned int Border;
    int plane_stride;

    /***********/
    /* Y Plane */
    /***********/
    Border = ybf->border;
    plane_stride = ybf->y_stride;
    src_ptr1 = ybf->y_buffer - Border;
    dest_ptr1 = src_ptr1 - (Border * plane_stride);

    for (i = 0; i < (int)Border; i++)
    {
        memcpy(dest_ptr1, src_ptr1, plane_stride);
        dest_ptr1 += plane_stride;
    }


    /***********/
    /* U Plane */
    /***********/
    plane_stride = ybf->uv_stride;
    Border /= 2;
    src_ptr1 = ybf->u_buffer - Border;
    dest_ptr1 = src_ptr1 - (Border * plane_stride);

    for (i = 0; i < (int)(Border); i++)
    {
        memcpy(dest_ptr1, src_ptr1, plane_stride);
        dest_ptr1 += plane_stride;
    }

    /***********/
    /* V Plane */
    /***********/

    src_ptr1 = ybf->v_buffer - Border;
    dest_ptr1 = src_ptr1 - (Border * plane_stride);

    for (i = 0; i < (int)(Border); i++)
    {
        memcpy(dest_ptr1, src_ptr1, plane_stride);
        dest_ptr1 += plane_stride;
    }
}

static void yv12_extend_frame_bottom_c(YV12_BUFFER_CONFIG *ybf)
{
    int i;
    unsigned char *src_ptr1, *src_ptr2;
    unsigned char *dest_ptr2;

    unsigned int Border;
    int plane_stride;
    int plane_height;

    /***********/
    /* Y Plane */
    /***********/
    Border = ybf->border;
    plane_stride = ybf->y_stride;
    plane_height = ybf->y_height;

    src_ptr1 = ybf->y_buffer - Border;
    src_ptr2 = src_ptr1 + (plane_height * plane_stride) - plane_stride;
    dest_ptr2 = src_ptr2 + plane_stride;

    for (i = 0; i < (int)Border; i++)
    {
        memcpy(dest_ptr2, src_ptr2, plane_stride);
        dest_ptr2 += plane_stride;
    }


    /***********/
    /* U Plane */
    /***********/
    plane_stride = ybf->uv_stride;
    plane_height = ybf->uv_height;
    Border /= 2;

    src_ptr1 = ybf->u_buffer - Border;
    src_ptr2 = src_ptr1 + (plane_height * plane_stride) - plane_stride;
    dest_ptr2 = src_ptr2 + plane_stride;

    for (i = 0; i < (int)(Border); i++)
    {
        memcpy(dest_ptr2, src_ptr2, plane_stride);
        dest_ptr2 += plane_stride;
    }

    /***********/
    /* V Plane */
    /***********/

    src_ptr1 = ybf->v_buffer - Border;
    src_ptr2 = src_ptr1 + (plane_height * plane_stride) - plane_stride;
    dest_ptr2 = src_ptr2 + plane_stride;

    for (i = 0; i < (int)(Border); i++)
    {
        memcpy(dest_ptr2, src_ptr2, plane_stride);
        dest_ptr2 += plane_stride;
    }
}

static void yv12_extend_frame_left_right_c(YV12_BUFFER_CONFIG *ybf,
                                           unsigned char *y_src,
                                           unsigned char *u_src,
                                           unsigned char *v_src)
{
    int i;
    unsigned char *src_ptr1, *src_ptr2;
    unsigned char *dest_ptr1, *dest_ptr2;

    unsigned int Border;
    int plane_stride;
    int plane_height;
    int plane_width;

    /***********/
    /* Y Plane */
    /***********/
    Border = ybf->border;
    plane_stride = ybf->y_stride;
    plane_height = 16;
    plane_width = ybf->y_width;

    /* copy the left and right most columns out */
    src_ptr1 = y_src;
    src_ptr2 = src_ptr1 + plane_width - 1;
    dest_ptr1 = src_ptr1 - Border;
    dest_ptr2 = src_ptr2 + 1;

    for (i = 0; i < plane_height; i++)
    {
        memset(dest_ptr1, src_ptr1[0], Border);
        memset(dest_ptr2, src_ptr2[0], Border);
        src_ptr1  += plane_stride;
        src_ptr2  += plane_stride;
        dest_ptr1 += plane_stride;
        dest_ptr2 += plane_stride;
    }

    /***********/
    /* U Plane */
    /***********/
    plane_stride = ybf->uv_stride;
    plane_height = 8;
    plane_width = ybf->uv_width;
    Border /= 2;

    /* copy the left and right most columns out */
    src_ptr1 = u_src;
    src_ptr2 = src_ptr1 + plane_width - 1;
    dest_ptr1 = src_ptr1 - Border;
    dest_ptr2 = src_ptr2 + 1;

    for (i = 0; i < plane_height; i++)
    {
        memset(dest_ptr1, src_ptr1[0], Border);
        memset(dest_ptr2, src_ptr2[0], Border);
        src_ptr1  += plane_stride;
        src_ptr2  += plane_stride;
        dest_ptr1 += plane_stride;
        dest_ptr2 += plane_stride;
    }

    /***********/
    /* V Plane */
    /***********/

    /* copy the left and right most columns out */
    src_ptr1 = v_src;
    src_ptr2 = src_ptr1 + plane_width - 1;
    dest_ptr1 = src_ptr1 - Border;
    dest_ptr2 = src_ptr2 + 1;

    for (i = 0; i < plane_height; i++)
    {
        memset(dest_ptr1, src_ptr1[0], Border);
        memset(dest_ptr2, src_ptr2[0], Border);
        src_ptr1  += plane_stride;
        src_ptr2  += plane_stride;
        dest_ptr1 += plane_stride;
        dest_ptr2 += plane_stride;
    }
}

static void decode_mb_rows(VP8D_COMP *pbi)
{
    VP8_COMMON *const pc = & pbi->common;
    MACROBLOCKD *const xd  = & pbi->mb;

    MODE_INFO *lf_mic = xd->mode_info_context;

    int ibc = 0;
    int num_part = 1 << pc->multi_token_partition;

    int recon_yoffset, recon_uvoffset;
    int mb_row, mb_col;
    int mb_idx = 0;

    YV12_BUFFER_CONFIG *yv12_fb_new = pbi->dec_fb_ref[INTRA_FRAME];

    int recon_y_stride = yv12_fb_new->y_stride;
    int recon_uv_stride = yv12_fb_new->uv_stride;

    unsigned char *ref_buffer[MAX_REF_FRAMES][3];
    unsigned char *dst_buffer[3];
    unsigned char *lf_dst[3];
    unsigned char *eb_dst[3];
    int i;
    int ref_fb_corrupted[MAX_REF_FRAMES];

    ref_fb_corrupted[INTRA_FRAME] = 0;

    for(i = 1; i < MAX_REF_FRAMES; i++)
    {
        YV12_BUFFER_CONFIG *this_fb = pbi->dec_fb_ref[i];

        ref_buffer[i][0] = this_fb->y_buffer;
        ref_buffer[i][1] = this_fb->u_buffer;
        ref_buffer[i][2] = this_fb->v_buffer;

        ref_fb_corrupted[i] = this_fb->corrupted;
    }

    /* Set up the buffer pointers */
    eb_dst[0] = lf_dst[0] = dst_buffer[0] = yv12_fb_new->y_buffer;
    eb_dst[1] = lf_dst[1] = dst_buffer[1] = yv12_fb_new->u_buffer;
    eb_dst[2] = lf_dst[2] = dst_buffer[2] = yv12_fb_new->v_buffer;

    xd->up_available = 0;

    /* Initialize the loop filter for this frame. */
    if(pc->filter_level)
        vp8_loop_filter_frame_init(pc, xd, pc->filter_level);

    pc->filter_level = 0;
    vp8_setup_intra_recon_top_line(yv12_fb_new);

    /* Decode the individual macro block */
    for (mb_row = 0; mb_row < pc->mb_rows; mb_row++)
    {
        if (num_part > 1)
        {
            xd->current_bc = & pbi->mbc[ibc];
            ibc++;

            if (ibc == num_part)
                ibc = 0;
        }

        recon_yoffset = mb_row * recon_y_stride * 16;
        recon_uvoffset = mb_row * recon_uv_stride * 8;

        /* reset contexts */
        xd->above_context = pc->above_context;
        memset(xd->left_context, 0, sizeof(ENTROPY_CONTEXT_PLANES));

        xd->left_available = 0;

        xd->mb_to_top_edge = -((mb_row * 16) << 3);
        xd->mb_to_bottom_edge = ((pc->mb_rows - 1 - mb_row) * 16) << 3;

        xd->recon_above[0] = dst_buffer[0] + recon_yoffset;
        xd->recon_above[1] = dst_buffer[1] + recon_uvoffset;
        xd->recon_above[2] = dst_buffer[2] + recon_uvoffset;

        xd->recon_left[0] = xd->recon_above[0] - 1;
        xd->recon_left[1] = xd->recon_above[1] - 1;
        xd->recon_left[2] = xd->recon_above[2] - 1;

        xd->recon_above[0] -= xd->dst.y_stride;
        xd->recon_above[1] -= xd->dst.uv_stride;
        xd->recon_above[2] -= xd->dst.uv_stride;

        /* TODO: move to outside row loop */
        xd->recon_left_stride[0] = xd->dst.y_stride;
        xd->recon_left_stride[1] = xd->dst.uv_stride;

        setup_intra_recon_left(xd->recon_left[0], xd->recon_left[1],
                               xd->recon_left[2], xd->dst.y_stride,
                               xd->dst.uv_stride);

        for (mb_col = 0; mb_col < pc->mb_cols; mb_col++)
        {
            /* Distance of Mb to the various image edges.
             * These are specified to 8th pel as they are always compared to values
             * that are in 1/8th pel units
             */
            xd->mb_to_left_edge = -((mb_col * 16) << 3);
            xd->mb_to_right_edge = ((pc->mb_cols - 1 - mb_col) * 16) << 3;

#if CONFIG_ERROR_CONCEALMENT
            {
                int corrupt_residual = (!pbi->independent_partitions &&
                                       pbi->frame_corrupt_residual) ||
                                       vp8dx_bool_error(xd->current_bc);
                if (pbi->ec_active &&
                    xd->mode_info_context->mbmi.ref_frame == INTRA_FRAME &&
                    corrupt_residual)
                {
                    /* We have an intra block with corrupt coefficients, better to
                     * conceal with an inter block. Interpolate MVs from neighboring
                     * MBs.
                     *
                     * Note that for the first mb with corrupt residual in a frame,
                     * we might not discover that before decoding the residual. That
                     * happens after this check, and therefore no inter concealment
                     * will be done.
                     */
                    vp8_interpolate_motion(xd,
                                           mb_row, mb_col,
                                           pc->mb_rows, pc->mb_cols);
                }
            }
#endif

            xd->dst.y_buffer = dst_buffer[0] + recon_yoffset;
            xd->dst.u_buffer = dst_buffer[1] + recon_uvoffset;
            xd->dst.v_buffer = dst_buffer[2] + recon_uvoffset;

            if (xd->mode_info_context->mbmi.ref_frame >= LAST_FRAME) {
              const MV_REFERENCE_FRAME ref = xd->mode_info_context->mbmi.ref_frame;
              xd->pre.y_buffer = ref_buffer[ref][0] + recon_yoffset;
              xd->pre.u_buffer = ref_buffer[ref][1] + recon_uvoffset;
              xd->pre.v_buffer = ref_buffer[ref][2] + recon_uvoffset;
            } else {
              // ref_frame is INTRA_FRAME, pre buffer should not be used.
              xd->pre.y_buffer = 0;
              xd->pre.u_buffer = 0;
              xd->pre.v_buffer = 0;
            }

            /* propagate errors from reference frames */
            xd->corrupted |= ref_fb_corrupted[xd->mode_info_context->mbmi.ref_frame];

            decode_macroblock(pbi, xd, mb_idx);

            mb_idx++;
            xd->left_available = 1;

            /* check if the boolean decoder has suffered an error */
            xd->corrupted |= vp8dx_bool_error(xd->current_bc);

            xd->recon_above[0] += 16;
            xd->recon_above[1] += 8;
            xd->recon_above[2] += 8;
            xd->recon_left[0] += 16;
            xd->recon_left[1] += 8;
            xd->recon_left[2] += 8;

            recon_yoffset += 16;
            recon_uvoffset += 8;

            ++xd->mode_info_context;  /* next mb */

            xd->above_context++;
        }

        /* adjust to the next row of mbs */
        vp8_extend_mb_row(yv12_fb_new, xd->dst.y_buffer + 16,
                          xd->dst.u_buffer + 8, xd->dst.v_buffer + 8);

        ++xd->mode_info_context;      /* skip prediction column */
        xd->up_available = 1;

        if(pc->filter_level)
        {
            if(mb_row > 0)
            {
                if (pc->filter_type == NORMAL_LOOPFILTER)
                    vp8_loop_filter_row_normal(pc, lf_mic, mb_row-1,
                                               recon_y_stride, recon_uv_stride,
                                               lf_dst[0], lf_dst[1], lf_dst[2]);
                else
                    vp8_loop_filter_row_simple(pc, lf_mic, mb_row-1,
                                               recon_y_stride, recon_uv_stride,
                                               lf_dst[0], lf_dst[1], lf_dst[2]);
                if(mb_row > 1)
                {
                    yv12_extend_frame_left_right_c(yv12_fb_new,
                                                   eb_dst[0],
                                                   eb_dst[1],
                                                   eb_dst[2]);

                    eb_dst[0] += recon_y_stride  * 16;
                    eb_dst[1] += recon_uv_stride *  8;
                    eb_dst[2] += recon_uv_stride *  8;
                }

                lf_dst[0] += recon_y_stride  * 16;
                lf_dst[1] += recon_uv_stride *  8;
                lf_dst[2] += recon_uv_stride *  8;
                lf_mic += pc->mb_cols;
                lf_mic++;         /* Skip border mb */
            }
        }
        else
        {
            if(mb_row > 0)
            {
                /**/
                yv12_extend_frame_left_right_c(yv12_fb_new,
                                               eb_dst[0],
                                               eb_dst[1],
                                               eb_dst[2]);
                eb_dst[0] += recon_y_stride  * 16;
                eb_dst[1] += recon_uv_stride *  8;
                eb_dst[2] += recon_uv_stride *  8;
            }
        }
    }

    if(pc->filter_level)
    {
        if (pc->filter_type == NORMAL_LOOPFILTER)
            vp8_loop_filter_row_normal(pc, lf_mic, mb_row-1, recon_y_stride,
                                       recon_uv_stride, lf_dst[0], lf_dst[1],
                                       lf_dst[2]);
        else
            vp8_loop_filter_row_simple(pc, lf_mic, mb_row-1, recon_y_stride,
                                       recon_uv_stride, lf_dst[0], lf_dst[1],
                                       lf_dst[2]);

        yv12_extend_frame_left_right_c(yv12_fb_new,
                                       eb_dst[0],
                                       eb_dst[1],
                                       eb_dst[2]);
        eb_dst[0] += recon_y_stride  * 16;
        eb_dst[1] += recon_uv_stride *  8;
        eb_dst[2] += recon_uv_stride *  8;
    }
    yv12_extend_frame_left_right_c(yv12_fb_new,
                                   eb_dst[0],
                                   eb_dst[1],
                                   eb_dst[2]);
    yv12_extend_frame_top_c(yv12_fb_new);
    yv12_extend_frame_bottom_c(yv12_fb_new);

}

static unsigned int read_partition_size(VP8D_COMP *pbi,
                                        const unsigned char *cx_size)
{
    unsigned char temp[3];
    if (pbi->decrypt_cb)
    {
        pbi->decrypt_cb(pbi->decrypt_state, cx_size, temp, 3);
        cx_size = temp;
    }
    return cx_size[0] + (cx_size[1] << 8) + (cx_size[2] << 16);
}

static int read_is_valid(const unsigned char *start,
                         size_t               len,
                         const unsigned char *end)
{
    return (start + len > start && start + len <= end);
}

static unsigned int read_available_partition_size(
                                       VP8D_COMP *pbi,
                                       const unsigned char *token_part_sizes,
                                       const unsigned char *fragment_start,
                                       const unsigned char *first_fragment_end,
                                       const unsigned char *fragment_end,
                                       int i,
                                       int num_part)
{
    VP8_COMMON* pc = &pbi->common;
    const unsigned char *partition_size_ptr = token_part_sizes + i * 3;
    unsigned int partition_size = 0;
    ptrdiff_t bytes_left = fragment_end - fragment_start;
    /* Calculate the length of this partition. The last partition
     * size is implicit. If the partition size can't be read, then
     * either use the remaining data in the buffer (for EC mode)
     * or throw an error.
     */
    if (i < num_part - 1)
    {
        if (read_is_valid(partition_size_ptr, 3, first_fragment_end))
            partition_size = read_partition_size(pbi, partition_size_ptr);
        else if (pbi->ec_active)
            partition_size = (unsigned int)bytes_left;
        else
            vpx_internal_error(&pc->error, VPX_CODEC_CORRUPT_FRAME,
                               "Truncated partition size data");
    }
    else
        partition_size = (unsigned int)bytes_left;

    /* Validate the calculated partition length. If the buffer
     * described by the partition can't be fully read, then restrict
     * it to the portion that can be (for EC mode) or throw an error.
     */
    if (!read_is_valid(fragment_start, partition_size, fragment_end))
    {
        if (pbi->ec_active)
            partition_size = (unsigned int)bytes_left;
        else
            vpx_internal_error(&pc->error, VPX_CODEC_CORRUPT_FRAME,
                               "Truncated packet or corrupt partition "
                               "%d length", i + 1);
    }
    return partition_size;
}


static void setup_token_decoder(VP8D_COMP *pbi,
                                const unsigned char* token_part_sizes)
{
    vp8_reader *bool_decoder = &pbi->mbc[0];
    unsigned int partition_idx;
    unsigned int fragment_idx;
    unsigned int num_token_partitions;
    const unsigned char *first_fragment_end = pbi->fragments.ptrs[0] +
                                          pbi->fragments.sizes[0];

    TOKEN_PARTITION multi_token_partition =
            (TOKEN_PARTITION)vp8_read_literal(&pbi->mbc[8], 2);
    if (!vp8dx_bool_error(&pbi->mbc[8]))
        pbi->common.multi_token_partition = multi_token_partition;
    num_token_partitions = 1 << pbi->common.multi_token_partition;

    /* Check for partitions within the fragments and unpack the fragments
     * so that each fragment pointer points to its corresponding partition. */
    for (fragment_idx = 0; fragment_idx < pbi->fragments.count; ++fragment_idx)
    {
        unsigned int fragment_size = pbi->fragments.sizes[fragment_idx];
        const unsigned char *fragment_end = pbi->fragments.ptrs[fragment_idx] +
                                            fragment_size;
        /* Special case for handling the first partition since we have already
         * read its size. */
        if (fragment_idx == 0)
        {
            /* Size of first partition + token partition sizes element */
            ptrdiff_t ext_first_part_size = token_part_sizes -
                pbi->fragments.ptrs[0] + 3 * (num_token_partitions - 1);
            fragment_size -= (unsigned int)ext_first_part_size;
            if (fragment_size > 0)
            {
                pbi->fragments.sizes[0] = (unsigned int)ext_first_part_size;
                /* The fragment contains an additional partition. Move to
                 * next. */
                fragment_idx++;
                pbi->fragments.ptrs[fragment_idx] = pbi->fragments.ptrs[0] +
                  pbi->fragments.sizes[0];
            }
        }
        /* Split the chunk into partitions read from the bitstream */
        while (fragment_size > 0)
        {
            ptrdiff_t partition_size = read_available_partition_size(
                                                 pbi,
                                                 token_part_sizes,
                                                 pbi->fragments.ptrs[fragment_idx],
                                                 first_fragment_end,
                                                 fragment_end,
                                                 fragment_idx - 1,
                                                 num_token_partitions);
            pbi->fragments.sizes[fragment_idx] = (unsigned int)partition_size;
            fragment_size -= (unsigned int)partition_size;
            assert(fragment_idx <= num_token_partitions);
            if (fragment_size > 0)
            {
                /* The fragment contains an additional partition.
                 * Move to next. */
                fragment_idx++;
                pbi->fragments.ptrs[fragment_idx] =
                    pbi->fragments.ptrs[fragment_idx - 1] + partition_size;
            }
        }
    }

    pbi->fragments.count = num_token_partitions + 1;

    for (partition_idx = 1; partition_idx < pbi->fragments.count; ++partition_idx)
    {
        if (vp8dx_start_decode(bool_decoder,
                               pbi->fragments.ptrs[partition_idx],
                               pbi->fragments.sizes[partition_idx],
                               pbi->decrypt_cb, pbi->decrypt_state))
            vpx_internal_error(&pbi->common.error, VPX_CODEC_MEM_ERROR,
                               "Failed to allocate bool decoder %d",
                               partition_idx);

        bool_decoder++;
    }

#if CONFIG_MULTITHREAD
    /* Clamp number of decoder threads */
    if (pbi->decoding_thread_count > num_token_partitions - 1)
        pbi->decoding_thread_count = num_token_partitions - 1;
#endif
}


static void init_frame(VP8D_COMP *pbi)
{
    VP8_COMMON *const pc = & pbi->common;
    MACROBLOCKD *const xd  = & pbi->mb;

    if (pc->frame_type == KEY_FRAME)
    {
        /* Various keyframe initializations */
        memcpy(pc->fc.mvc, vp8_default_mv_context, sizeof(vp8_default_mv_context));

        vp8_init_mbmode_probs(pc);

        vp8_default_coef_probs(pc);

        /* reset the segment feature data to 0 with delta coding (Default state). */
        memset(xd->segment_feature_data, 0, sizeof(xd->segment_feature_data));
        xd->mb_segement_abs_delta = SEGMENT_DELTADATA;

        /* reset the mode ref deltasa for loop filter */
        memset(xd->ref_lf_deltas, 0, sizeof(xd->ref_lf_deltas));
        memset(xd->mode_lf_deltas, 0, sizeof(xd->mode_lf_deltas));

        /* All buffers are implicitly updated on key frames. */
        pc->refresh_golden_frame = 1;
        pc->refresh_alt_ref_frame = 1;
        pc->copy_buffer_to_gf = 0;
        pc->copy_buffer_to_arf = 0;

        /* Note that Golden and Altref modes cannot be used on a key frame so
         * ref_frame_sign_bias[] is undefined and meaningless
         */
        pc->ref_frame_sign_bias[GOLDEN_FRAME] = 0;
        pc->ref_frame_sign_bias[ALTREF_FRAME] = 0;
    }
    else
    {
        /* To enable choice of different interploation filters */
        if (!pc->use_bilinear_mc_filter)
        {
            xd->subpixel_predict        = vp8_sixtap_predict4x4;
            xd->subpixel_predict8x4     = vp8_sixtap_predict8x4;
            xd->subpixel_predict8x8     = vp8_sixtap_predict8x8;
            xd->subpixel_predict16x16   = vp8_sixtap_predict16x16;
        }
        else
        {
            xd->subpixel_predict        = vp8_bilinear_predict4x4;
            xd->subpixel_predict8x4     = vp8_bilinear_predict8x4;
            xd->subpixel_predict8x8     = vp8_bilinear_predict8x8;
            xd->subpixel_predict16x16   = vp8_bilinear_predict16x16;
        }

        if (pbi->decoded_key_frame && pbi->ec_enabled && !pbi->ec_active)
            pbi->ec_active = 1;
    }

    xd->left_context = &pc->left_context;
    xd->mode_info_context = pc->mi;
    xd->frame_type = pc->frame_type;
    xd->mode_info_context->mbmi.mode = DC_PRED;
    xd->mode_info_stride = pc->mode_info_stride;
    xd->corrupted = 0; /* init without corruption */

    xd->fullpixel_mask = 0xffffffff;
    if(pc->full_pixel)
        xd->fullpixel_mask = 0xfffffff8;

}

int vp8_decode_frame(VP8D_COMP *pbi)
{
    vp8_reader *const bc = &pbi->mbc[8];
    VP8_COMMON *const pc = &pbi->common;
    MACROBLOCKD *const xd  = &pbi->mb;
    const unsigned char *data = pbi->fragments.ptrs[0];
    const unsigned int data_sz = pbi->fragments.sizes[0];
    const unsigned char *data_end = data + data_sz;
    ptrdiff_t first_partition_length_in_bytes;

    int i, j, k, l;
    const int *const mb_feature_data_bits = vp8_mb_feature_data_bits;
    int corrupt_tokens = 0;
    int prev_independent_partitions = pbi->independent_partitions;

    YV12_BUFFER_CONFIG *yv12_fb_new = pbi->dec_fb_ref[INTRA_FRAME];

    /* start with no corruption of current frame */
    xd->corrupted = 0;
    yv12_fb_new->corrupted = 0;

    if (data_end - data < 3)
    {
        if (!pbi->ec_active)
        {
            vpx_internal_error(&pc->error, VPX_CODEC_CORRUPT_FRAME,
                               "Truncated packet");
        }

        /* Declare the missing frame as an inter frame since it will
           be handled as an inter frame when we have estimated its
           motion vectors. */
        pc->frame_type = INTER_FRAME;
        pc->version = 0;
        pc->show_frame = 1;
        first_partition_length_in_bytes = 0;
    }
    else
    {
        unsigned char clear_buffer[10];
        const unsigned char *clear = data;
        if (pbi->decrypt_cb)
        {
            int n = (int)VPXMIN(sizeof(clear_buffer), data_sz);
            pbi->decrypt_cb(pbi->decrypt_state, data, clear_buffer, n);
            clear = clear_buffer;
        }

        pc->frame_type = (FRAME_TYPE)(clear[0] & 1);
        pc->version = (clear[0] >> 1) & 7;
        pc->show_frame = (clear[0] >> 4) & 1;
        first_partition_length_in_bytes =
            (clear[0] | (clear[1] << 8) | (clear[2] << 16)) >> 5;

        if (!pbi->ec_active &&
            (data + first_partition_length_in_bytes > data_end
            || data + first_partition_length_in_bytes < data))
            vpx_internal_error(&pc->error, VPX_CODEC_CORRUPT_FRAME,
                               "Truncated packet or corrupt partition 0 length");

        data += 3;
        clear += 3;

        vp8_setup_version(pc);


        if (pc->frame_type == KEY_FRAME)
        {
            /* vet via sync code */
            /* When error concealment is enabled we should only check the sync
             * code if we have enough bits available
             */
            if (!pbi->ec_active || data + 3 < data_end)
            {
                if (clear[0] != 0x9d || clear[1] != 0x01 || clear[2] != 0x2a)
                    vpx_internal_error(&pc->error, VPX_CODEC_UNSUP_BITSTREAM,
                                   "Invalid frame sync code");
            }

            /* If error concealment is enabled we should only parse the new size
             * if we have enough data. Otherwise we will end up with the wrong
             * size.
             */
            if (!pbi->ec_active || data + 6 < data_end)
            {
                pc->Width = (clear[3] | (clear[4] << 8)) & 0x3fff;
                pc->horiz_scale = clear[4] >> 6;
                pc->Height = (clear[5] | (clear[6] << 8)) & 0x3fff;
                pc->vert_scale = clear[6] >> 6;
            }
            data += 7;
        }
        else
        {
          memcpy(&xd->pre, yv12_fb_new, sizeof(YV12_BUFFER_CONFIG));
          memcpy(&xd->dst, yv12_fb_new, sizeof(YV12_BUFFER_CONFIG));
        }
    }
    if ((!pbi->decoded_key_frame && pc->frame_type != KEY_FRAME))
    {
        return -1;
    }

    init_frame(pbi);

    if (vp8dx_start_decode(bc, data, (unsigned int)(data_end - data),
                           pbi->decrypt_cb, pbi->decrypt_state))
        vpx_internal_error(&pc->error, VPX_CODEC_MEM_ERROR,
                           "Failed to allocate bool decoder 0");
    if (pc->frame_type == KEY_FRAME) {
        (void)vp8_read_bit(bc);  // colorspace
        pc->clamp_type  = (CLAMP_TYPE)vp8_read_bit(bc);
    }

    /* Is segmentation enabled */
    xd->segmentation_enabled = (unsigned char)vp8_read_bit(bc);

    if (xd->segmentation_enabled)
    {
        /* Signal whether or not the segmentation map is being explicitly updated this frame. */
        xd->update_mb_segmentation_map = (unsigned char)vp8_read_bit(bc);
        xd->update_mb_segmentation_data = (unsigned char)vp8_read_bit(bc);

        if (xd->update_mb_segmentation_data)
        {
            xd->mb_segement_abs_delta = (unsigned char)vp8_read_bit(bc);

            memset(xd->segment_feature_data, 0, sizeof(xd->segment_feature_data));

            /* For each segmentation feature (Quant and loop filter level) */
            for (i = 0; i < MB_LVL_MAX; i++)
            {
                for (j = 0; j < MAX_MB_SEGMENTS; j++)
                {
                    /* Frame level data */
                    if (vp8_read_bit(bc))
                    {
                        xd->segment_feature_data[i][j] = (signed char)vp8_read_literal(bc, mb_feature_data_bits[i]);

                        if (vp8_read_bit(bc))
                            xd->segment_feature_data[i][j] = -xd->segment_feature_data[i][j];
                    }
                    else
                        xd->segment_feature_data[i][j] = 0;
                }
            }
        }

        if (xd->update_mb_segmentation_map)
        {
            /* Which macro block level features are enabled */
            memset(xd->mb_segment_tree_probs, 255, sizeof(xd->mb_segment_tree_probs));

            /* Read the probs used to decode the segment id for each macro block. */
            for (i = 0; i < MB_FEATURE_TREE_PROBS; i++)
            {
                /* If not explicitly set value is defaulted to 255 by memset above */
                if (vp8_read_bit(bc))
                    xd->mb_segment_tree_probs[i] = (vp8_prob)vp8_read_literal(bc, 8);
            }
        }
    }
    else
    {
        /* No segmentation updates on this frame */
        xd->update_mb_segmentation_map = 0;
        xd->update_mb_segmentation_data = 0;
    }

    /* Read the loop filter level and type */
    pc->filter_type = (LOOPFILTERTYPE) vp8_read_bit(bc);
    pc->filter_level = vp8_read_literal(bc, 6);
    pc->sharpness_level = vp8_read_literal(bc, 3);

    /* Read in loop filter deltas applied at the MB level based on mode or ref frame. */
    xd->mode_ref_lf_delta_update = 0;
    xd->mode_ref_lf_delta_enabled = (unsigned char)vp8_read_bit(bc);

    if (xd->mode_ref_lf_delta_enabled)
    {
        /* Do the deltas need to be updated */
        xd->mode_ref_lf_delta_update = (unsigned char)vp8_read_bit(bc);

        if (xd->mode_ref_lf_delta_update)
        {
            /* Send update */
            for (i = 0; i < MAX_REF_LF_DELTAS; i++)
            {
                if (vp8_read_bit(bc))
                {
                    /*sign = vp8_read_bit( bc );*/
                    xd->ref_lf_deltas[i] = (signed char)vp8_read_literal(bc, 6);

                    if (vp8_read_bit(bc))        /* Apply sign */
                        xd->ref_lf_deltas[i] = xd->ref_lf_deltas[i] * -1;
                }
            }

            /* Send update */
            for (i = 0; i < MAX_MODE_LF_DELTAS; i++)
            {
                if (vp8_read_bit(bc))
                {
                    /*sign = vp8_read_bit( bc );*/
                    xd->mode_lf_deltas[i] = (signed char)vp8_read_literal(bc, 6);

                    if (vp8_read_bit(bc))        /* Apply sign */
                        xd->mode_lf_deltas[i] = xd->mode_lf_deltas[i] * -1;
                }
            }
        }
    }

    setup_token_decoder(pbi, data + first_partition_length_in_bytes);

    xd->current_bc = &pbi->mbc[0];

    /* Read the default quantizers. */
    {
        int Q, q_update;

        Q = vp8_read_literal(bc, 7);  /* AC 1st order Q = default */
        pc->base_qindex = Q;
        q_update = 0;
        pc->y1dc_delta_q = get_delta_q(bc, pc->y1dc_delta_q, &q_update);
        pc->y2dc_delta_q = get_delta_q(bc, pc->y2dc_delta_q, &q_update);
        pc->y2ac_delta_q = get_delta_q(bc, pc->y2ac_delta_q, &q_update);
        pc->uvdc_delta_q = get_delta_q(bc, pc->uvdc_delta_q, &q_update);
        pc->uvac_delta_q = get_delta_q(bc, pc->uvac_delta_q, &q_update);

        if (q_update)
            vp8cx_init_de_quantizer(pbi);

        /* MB level dequantizer setup */
        vp8_mb_init_dequantizer(pbi, &pbi->mb);
    }

    /* Determine if the golden frame or ARF buffer should be updated and how.
     * For all non key frames the GF and ARF refresh flags and sign bias
     * flags must be set explicitly.
     */
    if (pc->frame_type != KEY_FRAME)
    {
        /* Should the GF or ARF be updated from the current frame */
        pc->refresh_golden_frame = vp8_read_bit(bc);
#if CONFIG_ERROR_CONCEALMENT
        /* Assume we shouldn't refresh golden if the bit is missing */
        xd->corrupted |= vp8dx_bool_error(bc);
        if (pbi->ec_active && xd->corrupted)
            pc->refresh_golden_frame = 0;
#endif

        pc->refresh_alt_ref_frame = vp8_read_bit(bc);
#if CONFIG_ERROR_CONCEALMENT
        /* Assume we shouldn't refresh altref if the bit is missing */
        xd->corrupted |= vp8dx_bool_error(bc);
        if (pbi->ec_active && xd->corrupted)
            pc->refresh_alt_ref_frame = 0;
#endif

        /* Buffer to buffer copy flags. */
        pc->copy_buffer_to_gf = 0;

        if (!pc->refresh_golden_frame)
            pc->copy_buffer_to_gf = vp8_read_literal(bc, 2);

#if CONFIG_ERROR_CONCEALMENT
        /* Assume we shouldn't copy to the golden if the bit is missing */
        xd->corrupted |= vp8dx_bool_error(bc);
        if (pbi->ec_active && xd->corrupted)
            pc->copy_buffer_to_gf = 0;
#endif

        pc->copy_buffer_to_arf = 0;

        if (!pc->refresh_alt_ref_frame)
            pc->copy_buffer_to_arf = vp8_read_literal(bc, 2);

#if CONFIG_ERROR_CONCEALMENT
        /* Assume we shouldn't copy to the alt-ref if the bit is missing */
        xd->corrupted |= vp8dx_bool_error(bc);
        if (pbi->ec_active && xd->corrupted)
            pc->copy_buffer_to_arf = 0;
#endif


        pc->ref_frame_sign_bias[GOLDEN_FRAME] = vp8_read_bit(bc);
        pc->ref_frame_sign_bias[ALTREF_FRAME] = vp8_read_bit(bc);
    }

    pc->refresh_entropy_probs = vp8_read_bit(bc);
#if CONFIG_ERROR_CONCEALMENT
    /* Assume we shouldn't refresh the probabilities if the bit is
     * missing */
    xd->corrupted |= vp8dx_bool_error(bc);
    if (pbi->ec_active && xd->corrupted)
        pc->refresh_entropy_probs = 0;
#endif
    if (pc->refresh_entropy_probs == 0)
    {
        memcpy(&pc->lfc, &pc->fc, sizeof(pc->fc));
    }

    pc->refresh_last_frame = pc->frame_type == KEY_FRAME  ||  vp8_read_bit(bc);

#if CONFIG_ERROR_CONCEALMENT
    /* Assume we should refresh the last frame if the bit is missing */
    xd->corrupted |= vp8dx_bool_error(bc);
    if (pbi->ec_active && xd->corrupted)
        pc->refresh_last_frame = 1;
#endif

    if (0)
    {
        FILE *z = fopen("decodestats.stt", "a");
        fprintf(z, "%6d F:%d,G:%d,A:%d,L:%d,Q:%d\n",
                pc->current_video_frame,
                pc->frame_type,
                pc->refresh_golden_frame,
                pc->refresh_alt_ref_frame,
                pc->refresh_last_frame,
                pc->base_qindex);
        fclose(z);
    }

    {
        pbi->independent_partitions = 1;

        /* read coef probability tree */
        for (i = 0; i < BLOCK_TYPES; i++)
            for (j = 0; j < COEF_BANDS; j++)
                for (k = 0; k < PREV_COEF_CONTEXTS; k++)
                    for (l = 0; l < ENTROPY_NODES; l++)
                    {

                        vp8_prob *const p = pc->fc.coef_probs [i][j][k] + l;

                        if (vp8_read(bc, vp8_coef_update_probs [i][j][k][l]))
                        {
                            *p = (vp8_prob)vp8_read_literal(bc, 8);

                        }
                        if (k > 0 && *p != pc->fc.coef_probs[i][j][k-1][l])
                            pbi->independent_partitions = 0;

                    }
    }

    /* clear out the coeff buffer */
    memset(xd->qcoeff, 0, sizeof(xd->qcoeff));

    vp8_decode_mode_mvs(pbi);

#if CONFIG_ERROR_CONCEALMENT
    if (pbi->ec_active &&
            pbi->mvs_corrupt_from_mb < (unsigned int)pc->mb_cols * pc->mb_rows)
    {
        /* Motion vectors are missing in this frame. We will try to estimate
         * them and then continue decoding the frame as usual */
        vp8_estimate_missing_mvs(pbi);
    }
#endif

    memset(pc->above_context, 0, sizeof(ENTROPY_CONTEXT_PLANES) * pc->mb_cols);
    pbi->frame_corrupt_residual = 0;

#if CONFIG_MULTITHREAD
    if (pbi->b_multithreaded_rd && pc->multi_token_partition != ONE_PARTITION)
    {
        unsigned int thread;
        vp8mt_decode_mb_rows(pbi, xd);
        vp8_yv12_extend_frame_borders(yv12_fb_new);
        for (thread = 0; thread < pbi->decoding_thread_count; ++thread)
            corrupt_tokens |= pbi->mb_row_di[thread].mbd.corrupted;
    }
    else
#endif
    {
        decode_mb_rows(pbi);
        corrupt_tokens |= xd->corrupted;
    }

    /* Collect information about decoder corruption. */
    /* 1. Check first boolean decoder for errors. */
    yv12_fb_new->corrupted = vp8dx_bool_error(bc);
    /* 2. Check the macroblock information */
    yv12_fb_new->corrupted |= corrupt_tokens;

    if (!pbi->decoded_key_frame)
    {
        if (pc->frame_type == KEY_FRAME &&
            !yv12_fb_new->corrupted)
            pbi->decoded_key_frame = 1;
        else
            vpx_internal_error(&pbi->common.error, VPX_CODEC_CORRUPT_FRAME,
                               "A stream must start with a complete key frame");
    }

    /* vpx_log("Decoder: Frame Decoded, Size Roughly:%d bytes  \n",bc->pos+pbi->bc2.pos); */

    if (pc->refresh_entropy_probs == 0)
    {
        memcpy(&pc->fc, &pc->lfc, sizeof(pc->fc));
        pbi->independent_partitions = prev_independent_partitions;
    }

#ifdef PACKET_TESTING
    {
        FILE *f = fopen("decompressor.VP8", "ab");
        unsigned int size = pbi->bc2.pos + pbi->bc.pos + 8;
        fwrite((void *) &size, 4, 1, f);
        fwrite((void *) pbi->Source, size, 1, f);
        fclose(f);
    }
#endif

    return 0;
}
