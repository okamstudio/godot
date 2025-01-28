/* loongarch_lsx_init.c - LSX optimized filter functions
 *
 * Copyright (c) 2021 Loongson Technology Corporation Limited
 * All rights reserved.
 * Contributed by Jin Bo <jinbo@loongson.cn>
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */

#include "../pngpriv.h"

#ifdef PNG_READ_SUPPORTED
#if PNG_LOONGARCH_LSX_IMPLEMENTATION == 1

#include <sys/auxv.h>

#define LA_HWCAP_LSX    (1<<4)
static int png_has_lsx(void)
{
    int flags = 0;
    int flag  = (int)getauxval(AT_HWCAP);

    if (flag & LA_HWCAP_LSX)
        return 1;

    return 0;
}

void
png_init_filter_functions_lsx(png_structp pp, unsigned int bpp)
{
   /* IMPORTANT: any new external functions used here must be declared using
    * PNG_INTERNAL_FUNCTION in ../pngpriv.h.  This is required so that the
    * 'prefix' option to configure works:
    *
    *    ./configure --with-libpng-prefix=foobar_
    *
    * Verify you have got this right by running the above command, doing a build
    * and examining pngprefix.h; it must contain a #define for every external
    * function you add.  (Notice that this happens automatically for the
    * initialization function.)
    */

   if (png_has_lsx())
   {
      pp->read_filter[PNG_FILTER_VALUE_UP-1] = png_read_filter_row_up_lsx;
      if (bpp == 3)
      {
         pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub3_lsx;
         pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg3_lsx;
         pp->read_filter[PNG_FILTER_VALUE_PAETH-1] = png_read_filter_row_paeth3_lsx;
      }
      else if (bpp == 4)
      {
         pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub4_lsx;
         pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg4_lsx;
         pp->read_filter[PNG_FILTER_VALUE_PAETH-1] = png_read_filter_row_paeth4_lsx;
      }
   }
}

#endif /* PNG_LOONGARCH_LSX_IMPLEMENTATION == 1 */
#endif /* PNG_READ_SUPPORTED */
