/*
 *	$Id$
 */

#ifndef  __ML_MODEL_H__
#define  __ML_MODEL_H__


#include  <kiklib/kik_types.h>

#include  "ml_char.h"
#include  "ml_line.h"


typedef struct  ml_model
{
	/* private */
	ml_line_t *  lines ;
	
	/* public(readonly) */
	u_int  num_of_cols ;
	u_int  num_of_rows ;
	u_int  num_of_filled_rows ;
	
	/* private */
	int  beg_row ;			/* used for scrolling */

} ml_model_t ;


int  ml_model_init( ml_model_t *  model , u_int  num_of_cols , u_int  num_of_rows) ;

int  ml_model_final( ml_model_t *  model) ;

int  ml_model_reset( ml_model_t *  model) ;

int  ml_model_resize( ml_model_t *  model , u_int  num_of_cols , u_int  num_of_rows) ;

int  ml_model_end_row( ml_model_t *  model) ;

ml_line_t *  ml_model_get_line( ml_model_t *  model , int  row) ;

ml_line_t *  ml_model_get_end_line( ml_model_t *  model) ;

u_int  ml_model_reserve_boundary( ml_model_t *  model , u_int  size) ;

u_int  ml_model_break_boundary( ml_model_t *  model , u_int  size , ml_char_t *  sp_ch) ;

u_int  ml_model_shrink_boundary( ml_model_t *  model , u_int  size) ;

int  ml_model_scroll_upward( ml_model_t *  model , u_int  size) ;

int  ml_model_scroll_downward( ml_model_t *  model , u_int  size) ;


#endif
