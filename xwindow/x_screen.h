/*
 *	$Id$
 */

#ifndef  __X_SCREEN_H__
#define  __X_SCREEN_H__


#include  <mkf/mkf_parser.h>
#include  <kiklib/kik_types.h>		/* u_int/int8_t/size_t */
#include  <ml_term.h>

#include  "x_window.h"
#include  "x_selection.h"
#include  "x_shortcut.h"
#include  "x_termcap.h"
#include  "x_mod_meta_mode.h"
#include  "x_bel_mode.h"
#include  "x_sb_mode.h"
#include  "x_kbd.h"


typedef struct x_screen *  x_screen_ptr_t ;

typedef struct x_system_event_listener
{
	void *  self ;

	void  (*open_screen)( void *) ;
	void  (*close_screen)( void * , x_screen_ptr_t) ;
	
	void  (*open_pty)( void * , x_screen_ptr_t , char *) ;
	void  (*next_pty)( void * , x_screen_ptr_t) ;
	void  (*prev_pty)( void * , x_screen_ptr_t) ;
	void  (*close_pty)( void * , x_screen_ptr_t) ;
	
	void  (*pty_closed)( void * , x_screen_ptr_t) ;

	ml_term_t *  (*get_pty)( void * , char *) ;

	char *  (*pty_list)( void *) ;
	
	/* for debug */
	void  (*exit)( void * , int) ;

} x_system_event_listener_t ;

typedef struct  x_screen_scroll_event_listener
{
	void *  self ;

	void  (*bs_mode_entered)( void *) ;
	void  (*bs_mode_exited)( void *) ;
	void  (*scrolled_upward)( void * , u_int) ;
	void  (*scrolled_downward)( void * , u_int) ;
	void  (*log_size_changed)( void * , u_int) ;
	void  (*line_height_changed)( void * , u_int) ;
	void  (*change_fg_color)( void * , char *) ;
	char *  (*fg_color)( void *) ;
	void  (*change_bg_color)( void * , char *) ;
	char *  (*bg_color)( void *) ;
	void  (*change_view)( void * , char *) ;
	char *  (*view_name)( void *) ;
	void  (*transparent_state_changed)( void * , int , x_picture_modifier_t *) ;
	x_sb_mode_t  (*sb_mode)( void *) ;
	void  (*change_sb_mode)( void * , x_sb_mode_t) ;
	void  (*term_changed)( void * , u_int , u_int) ;

} x_screen_scroll_event_listener_t ;

typedef struct  x_screen
{
	x_window_t  window ;

	x_font_manager_t *  font_man ;
	
	x_color_manager_t *  color_man ;

	ml_term_t *  term ;
	
	x_selection_t  sel ;

	ml_screen_event_listener_t  screen_listener ;
	ml_xterm_event_listener_t  xterm_listener ;
	ml_config_event_listener_t  config_listener ;
	ml_pty_event_listener_t  pty_listener ;

	x_sel_event_listener_t  sel_listener ;
	x_xim_event_listener_t  xim_listener ;

	x_shortcut_t *  shortcut ;
	x_termcap_entry_t *  termcap ;

	x_kbd_t *  kbd ;

	char *  mod_meta_key ;
	x_mod_meta_mode_t  mod_meta_mode ;
	u_int  mod_meta_mask ;

	x_bel_mode_t  bel_mode ;

	u_int  line_space ;

	u_int  screen_width_ratio ;
	u_int  screen_height_ratio ;

	x_system_event_listener_t *  system_listener ;
	x_screen_scroll_event_listener_t *  screen_scroll_listener ;

	mkf_parser_t *  xct_parser ;
	mkf_parser_t *  utf8_parser ;
	
	mkf_parser_t *  ml_str_parser ;
	mkf_conv_t *  utf8_conv ;
	mkf_conv_t *  xct_conv ;
	
	int  scroll_cache_rows ;
	int  scroll_cache_boundary_start ;
	int  scroll_cache_boundary_end ;

	char *  pic_file_path ;
	x_picture_modifier_t  pic_mod ;

	/*
	 * These members mustn't be changed by mlterm configuration protocol,
	 * since they can be executed.
	 */
	char *  conf_menu_path_1 ;
	char *  conf_menu_path_2 ;
	char *  conf_menu_path_3 ;

	u_int8_t  fade_ratio ;
	int8_t  is_focused ;
	int8_t  receive_string_via_ucs ;
	int8_t  xim_open_in_startup ;
	int8_t  use_vertical_cursor ;
	int8_t  use_extended_scroll_shortcut ;

} x_screen_t ;


x_screen_t *  x_screen_new( ml_term_t *  term , x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man , x_termcap_entry_t *  termcap ,
	u_int  brightness , u_int contrast , u_int gamma ,
	u_int  fade_ratio , x_shortcut_t *  shortcut ,
	u_int  screen_width_ratio , u_int  screen_height_ratio ,
	int  xim_open_in_startup , char *  mod_meta_key , x_mod_meta_mode_t  mod_meta_mode ,
	x_bel_mode_t  bel_mode , int  receive_string_via_ucs , char *  pic_file_path ,
	int  use_transbg , int  use_vertical_cursor , int  big5_buggy ,
	char *  conf_menu_path_1 , char *  conf_menu_path_2 , char *  conf_menu_path_3 ,
	int  use_extended_scroll_shortcut , u_int  line_space) ;

int  x_screen_delete( x_screen_t *  screen) ;

int  x_screen_attach( x_screen_t *  screen , ml_term_t *  term) ;

ml_term_t *  x_screen_detach( x_screen_t *  screen) ;

int  x_set_system_listener( x_screen_t *  screen ,
	x_system_event_listener_t *  system_listener) ;

int  x_set_screen_scroll_listener( x_screen_t *  screen ,
	x_screen_scroll_event_listener_t *  screen_scroll_listener) ;

	
int  x_screen_scroll_upward( x_screen_t *  screen , u_int  size) ;

int  x_screen_scroll_downward( x_screen_t *  screen , u_int  size) ;

int  x_screen_scroll_to( x_screen_t *  screen , int  row) ;


u_int  x_col_width( x_screen_t *  screen) ;

u_int  x_line_height( x_screen_t *  screen) ;

u_int  x_line_height_to_baseline( x_screen_t *  screen) ;

u_int  x_line_top_margin( x_screen_t *  screen) ;

u_int  x_line_bottom_margin( x_screen_t *  screen) ;


x_picture_modifier_t *  x_screen_get_picture_modifier( x_screen_t *  screen) ;


#endif
