/*
 *	$Id$
 */

#ifndef  __X_DISPLAY_H__
#define  __X_DISPLAY_H__


#include  <kiklib/kik_types.h>		/* u_int */

#include  "x.h"
#include  "x_gc.h"

#define  XC_nil	 1000

/* Defined in x_window.h */
typedef struct  x_window *  x_window_ptr_t ;

typedef struct  x_modifier_mapping
{
	u_long  serial ;
	XModifierKeymap *  map ;

} x_modifier_mapping_t ;

typedef struct  x_display
{
	/*
	 * Public(read only)
	 */
	Display *  display ;	/* Don't change position, which pixmap_engine depends on. */
	int  screen ;		/* DefaultScreen */
	char *  name ;

	Window  my_window ;	/* DefaultRootWindow */

#ifdef  USE_XLIB
	/* Only one visual, colormap or depth is permitted per display. */
	Visual *  visual ;
	Colormap  colormap ;
#endif
	u_int  depth ;
	x_gc_t *  gc ;

	u_int  width ;
	u_int  height ;

	/*
	 * Private
	 */
	x_window_ptr_t *  roots ;
	u_int  num_of_roots ;

	x_window_ptr_t  selection_owner ;

	x_modifier_mapping_t  modmap ;

#if  ! defined(USE_FRAMEBUFFER) && ! defined(USE_CONSOLE)
	Cursor  cursors[3] ;
#endif

} x_display_t ;


x_display_t *  x_display_open( char *  disp_name , u_int  depth) ;

int  x_display_close( x_display_t *  disp) ;

int  x_display_close_all(void) ;

x_display_t **  x_get_opened_displays( u_int *  num) ;

int  x_display_fd( x_display_t *  disp) ;

int  x_display_show_root( x_display_t *  disp , x_window_ptr_t  root ,
	int  x , int  y , int  hint , char *  app_name , Window  parent_window) ;

int  x_display_remove_root( x_display_t *  disp , x_window_ptr_t  root) ;

void  x_display_idling( x_display_t *  disp) ;

int  x_display_receive_next_event( x_display_t *  disp) ;

#if  defined(USE_FRAMEBUFFER) || defined(USE_CONSOLE) || defined(USE_QUARTZ)
#define  x_display_sync(disp)  (0)
#elif  defined(USE_WIN32GUI)
#define  x_display_sync(disp)  x_display_receive_next_event(disp)
#else
void  x_display_sync( x_display_t *  disp) ;
#endif


/*
 * Folloing functions called from x_window.c
 */

int  x_display_own_selection( x_display_t *  disp , x_window_ptr_t  win) ;

int  x_display_clear_selection( x_display_t *  disp , x_window_ptr_t  win) ;

XModifierKeymap *  x_display_get_modifier_mapping( x_display_t *  disp) ;

void  x_display_update_modifier_mapping( x_display_t *  disp ,	u_int  serial) ;

XID  x_display_get_group_leader( x_display_t *  disp) ;

#if  defined(USE_FRAMEBUFFER) || defined(USE_CONSOLE)
void  x_display_set_use_ansi_colors( int  use) ;

int  x_display_reset_cmap(void) ;

void  x_display_rotate( int  rotate) ;

void  x_display_reset_input_method_window(void) ;
#else
#define  x_display_set_use_ansi_colors(use)  (0)
#define  x_display_reset_cmap()  (0)
#endif

#ifdef  USE_CONSOLE
#include  <ml_char_encoding.h>

void  x_display_set_char_encoding( x_display_t *  disp , ml_char_encoding_t  encoding) ;

#ifdef  USE_LIBSIXEL
void  x_display_set_sixel_colors( x_display_t *  disp , const char *  colors) ;
#else
#define  x_display_set_sixel_colors( disp , colors)  (0)
#endif

void  x_display_set_default_cell_size( u_int  width , u_int  height) ;
#endif


#endif
