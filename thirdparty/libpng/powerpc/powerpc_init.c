/* powerpc_init.c - POWERPC optimised filter functions
 *
 * Copyright (c) 2018 Cosmin Truta
 * Copyright (c) 2017 Glenn Randers-Pehrson
 * Written by Vadim Barkov, 2017.
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */

/* Below, after checking __linux__, various non-C90 POSIX 1003.1 functions are
 * called.
 */
#define _POSIX_SOURCE 1

#include <stdio.h>
#include "../pngpriv.h"

#ifdef PNG_READ_SUPPORTED

#if PNG_POWERPC_VSX_OPT > 0
#ifdef PNG_POWERPC_VSX_CHECK_SUPPORTED /* Do run-time checks */
/* WARNING: it is strongly recommended that you do not build libpng with
 * run-time checks for CPU features if at all possible.  In the case of the PowerPC
 * VSX instructions there is no processor-specific way of detecting the
 * presence of the required support, therefore run-time detection is extremely
 * OS specific.
 *
 * You may set the macro PNG_POWERPC_VSX_FILE to the file name of file containing
 * a fragment of C source code which defines the png_have_vsx function.  There
 * are a number of implementations in contrib/powerpc-vsx, but the only one that
 * has partial support is contrib/powerpc-vsx/linux.c - a generic Linux
 * implementation which reads /proc/cpufino.
 */
#ifndef PNG_POWERPC_VSX_FILE
#  ifdef __linux__
#     define  PNG_POWERPC_VSX_FILE "contrib/powerpc-vsx/linux_aux.c"
#  endif
#endif

#ifdef PNG_POWERPC_VSX_FILE

#include <signal.h> /* for sig_atomic_t */
static int png_have_vsx(png_structp png_ptr);
#include PNG_POWERPC_VSX_FILE

#else  /* PNG_POWERPC_VSX_FILE */
#  error "PNG_POWERPC_VSX_FILE undefined: no support for run-time POWERPC VSX checks"
#endif /* PNG_POWERPC_VSX_FILE */
#endif /* PNG_POWERPC_VSX_CHECK_SUPPORTED */

void
png_init_filter_functions_vsx(png_structp pp, unsigned int bpp)
{
   /* The switch statement is compiled in for POWERPC_VSX_API, the call to
    * png_have_vsx is compiled in for POWERPC_VSX_CHECK. If both are defined
    * the check is only performed if the API has not set the PowerPC option on
    * or off explicitly. In this case the check controls what happens.
    */

#ifdef PNG_POWERPC_VSX_API_SUPPORTED
   switch ((pp->options >> PNG_POWERPC_VSX) & 3)
   {
      case PNG_OPTION_UNSET:
         /* Allow the run-time check to execute if it has been enabled -
          * thus both API and CHECK can be turned on.  If it isn't supported
          * this case will fall through to the 'default' below, which just
          * returns.
          */
#endif /* PNG_POWERPC_VSX_API_SUPPORTED */
#ifdef PNG_POWERPC_VSX_CHECK_SUPPORTED
         {
            static volatile sig_atomic_t no_vsx = -1; /* not checked */

            if (no_vsx < 0)
               no_vsx = !png_have_vsx(pp);

            if (no_vsx)
               return;
         }
#ifdef PNG_POWERPC_VSX_API_SUPPORTED
         break;
#endif
#endif /* PNG_POWERPC_VSX_CHECK_SUPPORTED */

#ifdef PNG_POWERPC_VSX_API_SUPPORTED
      default: /* OFF or INVALID */
         return;

      case PNG_OPTION_ON:
         /* Option turned on */
         break;
   }
#endif

   /* IMPORTANT: any new internal functions used here must be declared using
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
   pp->read_filter[PNG_FILTER_VALUE_UP-1] = png_read_filter_row_up_vsx;

   if (bpp == 3)
   {
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub3_vsx;
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg3_vsx;
      pp->read_filter[PNG_FILTER_VALUE_PAETH-1] = png_read_filter_row_paeth3_vsx;
   }

   else if (bpp == 4)
   {
      pp->read_filter[PNG_FILTER_VALUE_SUB-1] = png_read_filter_row_sub4_vsx;
      pp->read_filter[PNG_FILTER_VALUE_AVG-1] = png_read_filter_row_avg4_vsx;
      pp->read_filter[PNG_FILTER_VALUE_PAETH-1] = png_read_filter_row_paeth4_vsx;
   }
}
#endif /* PNG_POWERPC_VSX_OPT > 0 */
#endif /* READ */
