/*
 *	$Id$
 */

#ifndef  __ML_SB_VIEW_FACTORY_H__
#define  __ML_SB_VIEW_FACTORY_H__


#include  "ml_sb_view.h"


typedef  ml_sb_view_t * (*ml_sb_view_new_func_t)(void) ;


ml_sb_view_t *  ml_sb_view_new( char *  name) ;

ml_sb_view_t *  ml_transparent_scrollbar_view_new( char *  name) ;

int  ml_unload_scrollbar_view_lib( char *  name) ;


#endif
