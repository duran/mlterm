/*
 *	$Id$
 */

#ifndef  __ML_EDIT_H__
#define  __ML_EDIT_H__


#include  <kiklib/kik_types.h>

#include  "ml_char.h"
#include  "ml_render_hint.h"
#include  "ml_line.h"
#include  "ml_model.h"


typedef struct  ml_cursor
{
	int  row ;
	int  char_index ;
	int  col ;
	
	ml_color_t  orig_fg ;
	ml_color_t  orig_bg ;
	int8_t  is_highlighted ;

	int  saved_row ;
	int  saved_char_index ;
	int  saved_col ;
	int8_t  is_saved ;
	
} ml_cursor_t ;

typedef struct ml_edit_scroll_event_listener
{
	void *  self ;

	void (*receive_upward_scrolled_out_line)( void * , ml_line_t *) ;
	
	int  (*window_scroll_upward_region)( void * , int , int , u_int) ;
	int  (*window_scroll_downward_region)( void * , int , int , u_int) ;

} ml_edit_scroll_event_listener_t ;

typedef struct  ml_edit
{
	ml_model_t  model ;
	ml_cursor_t  cursor ;

	/* used for rendering */
	ml_render_hint_t  render_hint ;

	int8_t  is_logging ;

	u_int8_t *  tab_stops ;
	u_int  tab_size ;

	ml_char_t  sp_ch ;
	ml_char_t  bce_ch ;

	/* used for line overlapping */
	ml_line_t *  wraparound_ready_line ;
	ml_char_t  prev_recv_ch ;	/* a char which was inserted or overwritten an opearation before */
	
	int  scroll_region_beg ;
	int  scroll_region_end ;

	ml_edit_scroll_event_listener_t *  scroll_listener ;

} ml_edit_t ;


int  ml_edit_init( ml_edit_t *  edit , ml_edit_scroll_event_listener_t *  scroll_listener ,
	u_int  num_of_cols , u_int  num_of_rows , u_int  tab_size , int  is_logging) ;

int  ml_edit_final( ml_edit_t *  edit) ;

int  ml_edit_resize( ml_edit_t *  edit , u_int  num_of_cols , u_int  num_of_rows) ;

int  ml_edit_insert_chars( ml_edit_t *  edit , ml_char_t *  chars , u_int  num_of_chars) ;

int  ml_edit_insert_blank_chars( ml_edit_t *  edit , u_int  num_of_blank_chars) ;

int  ml_edit_overwrite_chars( ml_edit_t *  edit , ml_char_t *  chars , u_int  num_of_chars) ;

int  ml_edit_delete_cols( ml_edit_t *  edit , u_int  delete_cols) ;

int  ml_edit_delete_cols_bce( ml_edit_t *  edit , u_int  delete_cols) ;

int  ml_edit_clear_cols( ml_edit_t *  edit , u_int  cols) ;

int  ml_edit_clear_cols_bce( ml_edit_t *  edit , u_int  cols) ;

int  ml_edit_insert_new_line( ml_edit_t *  edit) ;

int  ml_edit_delete_line( ml_edit_t *  edit) ;

int  ml_edit_clear_line_to_right( ml_edit_t *  edit) ;

int  ml_edit_clear_line_to_right_bce( ml_edit_t *  edit) ;

int  ml_edit_clear_line_to_left( ml_edit_t *  edit) ;

int  ml_edit_clear_line_to_left_bce( ml_edit_t *  edit) ;

int  ml_edit_clear_below( ml_edit_t *  edit) ;

int  ml_edit_clear_below_bce( ml_edit_t *  edit) ;

int  ml_edit_clear_above( ml_edit_t *  edit) ;

int  ml_edit_clear_above_bce( ml_edit_t *  edit) ;

int  ml_edit_set_scroll_region( ml_edit_t *  edit , int  beg , int  end) ;

int  ml_edit_scroll_upward( ml_edit_t *  edit , u_int  size) ;

int  ml_edit_scroll_downward( ml_edit_t *  edit , u_int  size) ;

int  ml_edit_vertical_tab( ml_edit_t *  edit) ;

int  ml_edit_set_tab_size( ml_edit_t *  edit , u_int  tab_size) ;

int  ml_edit_set_tab_stop( ml_edit_t *  edit) ;

int  ml_edit_clear_tab_stop( ml_edit_t *  edit) ;

int  ml_edit_clear_all_tab_stops( ml_edit_t *  edit) ;

ml_line_t *  ml_edit_get_line( ml_edit_t *  edit , int  row) ;

int  ml_cursor_go_forward( ml_edit_t *  edit , int  flag) ;

int  ml_cursor_go_back( ml_edit_t *  edit , int  flag) ;

int  ml_cursor_go_upward( ml_edit_t *  edit , int  flag) ;

int  ml_cursor_go_downward( ml_edit_t *  edit , int  flag) ;

int  ml_cursor_goto_beg_of_line( ml_edit_t *  edit) ;

int  ml_cursor_goto_home( ml_edit_t *  edit) ;

int  ml_cursor_goto_end( ml_edit_t *  edit) ;

int  ml_cursor_goto( ml_edit_t *  edit , int  col , int  row , int  flag) ;

int  ml_cursor_is_beg_of_line( ml_edit_t *  edit) ;

int  ml_cursor_save( ml_edit_t *  edit) ;

int  ml_cursor_restore( ml_edit_t *  edit) ;

int  ml_highlight_cursor( ml_edit_t *  edit) ;

int  ml_unhighlight_cursor( ml_edit_t *  edit) ;

int  ml_cursor_char_index( ml_edit_t *  edit) ;

int  ml_cursor_col( ml_edit_t *  edit) ;

int  ml_cursor_row( ml_edit_t *  edit) ;

int  ml_edit_set_modified_all( ml_edit_t *  edit) ;

u_int ml_edit_get_cols( ml_edit_t *  edit) ;

u_int ml_edit_get_rows( ml_edit_t *  edit) ;

int  ml_edit_fill_all( ml_edit_t *  edit , ml_char_t *  ch) ;
	
#ifdef  DEBUG

void  ml_edit_dump( ml_edit_t *  edit) ;

void  ml_edit_dump_updated( ml_edit_t *  edit) ;

#endif


#endif
