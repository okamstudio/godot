/*
 * Copyright © 2022  Behdad Esfahbod
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#ifndef HB_LIMITS_HH
#define HB_LIMITS_HH

#include "hb.hh"


#ifndef HB_BUFFER_MAX_LEN_FACTOR
#define HB_BUFFER_MAX_LEN_FACTOR 64
#endif
#ifndef HB_BUFFER_MAX_LEN_MIN
#define HB_BUFFER_MAX_LEN_MIN 16384
#endif
#ifndef HB_BUFFER_MAX_LEN_DEFAULT
#define HB_BUFFER_MAX_LEN_DEFAULT 0x3FFFFFFF /* Shaping more than a billion chars? Let us know! */
#endif

#ifndef HB_BUFFER_MAX_OPS_FACTOR
#define HB_BUFFER_MAX_OPS_FACTOR 1024
#endif
#ifndef HB_BUFFER_MAX_OPS_MIN
#define HB_BUFFER_MAX_OPS_MIN 16384
#endif
#ifndef HB_BUFFER_MAX_OPS_DEFAULT
#define HB_BUFFER_MAX_OPS_DEFAULT 0x1FFFFFFF /* Shaping more than a billion operations? Let us know! */
#endif


#ifndef HB_MAX_NESTING_LEVEL
#define HB_MAX_NESTING_LEVEL 64
#endif


#ifndef HB_MAX_CONTEXT_LENGTH
#define HB_MAX_CONTEXT_LENGTH 64
#endif

#ifndef HB_CLOSURE_MAX_STAGES
/*
 * The maximum number of times a lookup can be applied during shaping.
 * Used to limit the number of iterations of the closure algorithm.
 * This must be larger than the number of times add_gsub_pause() is
 * called in a collect_features call of any shaper.
 */
#define HB_CLOSURE_MAX_STAGES 12
#endif

#ifndef HB_MAX_SCRIPTS
#define HB_MAX_SCRIPTS 500
#endif

#ifndef HB_MAX_LANGSYS
#define HB_MAX_LANGSYS 2000
#endif

#ifndef HB_MAX_LANGSYS_FEATURE_COUNT
#define HB_MAX_LANGSYS_FEATURE_COUNT 50000
#endif

#ifndef HB_MAX_FEATURE_INDICES
#define HB_MAX_FEATURE_INDICES 1500
#endif

#ifndef HB_MAX_LOOKUP_VISIT_COUNT
#define HB_MAX_LOOKUP_VISIT_COUNT 35000
#endif

#ifndef HB_MAX_GRAPH_EDGE_COUNT
#define HB_MAX_GRAPH_EDGE_COUNT 2048
#endif

#ifndef HB_VAR_COMPOSITE_MAX_AXES
#define HB_VAR_COMPOSITE_MAX_AXES 4096
#endif

#ifndef HB_GLYF_MAX_POINTS
#define HB_GLYF_MAX_POINTS 200000
#endif

#ifndef HB_CFF_MAX_OPS
#define HB_CFF_MAX_OPS 10000
#endif

#ifndef HB_MAX_COMPOSITE_OPERATIONS_PER_GLYPH
#define HB_MAX_COMPOSITE_OPERATIONS_PER_GLYPH 64
#endif


#endif /* HB_LIMITS_HH */
