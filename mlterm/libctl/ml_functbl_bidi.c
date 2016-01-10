/*
 *	$Id$
 */

#include  "../ml_ctl_loader.h"


/* Dummy declaration */
void  ml_line_set_use_bidi(void) ;
void  ml_line_bidi_convert_logical_char_index_to_visual(void) ;
void  ml_line_bidi_convert_visual_char_index_to_logical(void) ;
void  ml_line_bidi_copy_logical_str(void) ;
void  ml_line_bidi_is_rtl(void) ;
void  ml_bidi_copy(void) ;
void  ml_bidi_reset(void) ;
void  ml_line_bidi_need_shape(void) ;
void  ml_line_bidi_render(void) ;
void  ml_line_bidi_visual(void) ;
void  ml_line_bidi_logical(void) ;


/* --- global variables --- */

void *  ml_ctl_bidi_func_table[MAX_CTL_BIDI_FUNCS] =
{
	(void*)CTL_API_COMPAT_CHECK_MAGIC ,
	ml_line_set_use_bidi ,
	ml_line_bidi_convert_logical_char_index_to_visual ,
	ml_line_bidi_convert_visual_char_index_to_logical ,
	ml_line_bidi_copy_logical_str ,
	ml_line_bidi_is_rtl ,
	ml_shape_arabic ,
	ml_is_arabic_combining ,
	ml_is_rtl_char ,
	ml_bidi_copy ,
	ml_bidi_reset ,
	ml_line_bidi_need_shape ,
	ml_line_bidi_render ,
	ml_line_bidi_visual ,
	ml_line_bidi_logical ,

} ;
