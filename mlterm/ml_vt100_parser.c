/*
 *	$Id$
 */

#include  "ml_vt100_parser.h"

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* memmove */
#include  <stdlib.h>		/* atoi */
#include  <fcntl.h>		/* open */
#include  <unistd.h>		/* write */
#ifdef  DEBUG
#include  <stdarg.h>		/* va_list */
#endif

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* malloc/free */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */
#include  <kiklib/kik_conf_io.h>/* kik_get_user_rc_path */
#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup */
#include  <kiklib/kik_args.h>
#include  <kiklib/kik_unistd.h>	/* kik_usleep */
#include  <mkf/mkf_ucs4_map.h>	/* mkf_map_to_ucs4 */
#include  <mkf/mkf_ucs_property.h>
#include  <mkf/mkf_locale_ucs4_map.h>
#include  <mkf/mkf_ko_kr_map.h>

#include  "ml_iscii.h"
#include  "ml_config_proto.h"
#include  "ml_drcs.h"


/*
 * kterm BUF_SIZE in ptyx.h is 4096.
 * Maximum size of sequence parsed once is PTY_RD_BUFFER_SIZE * 3.
 * (see ml_parse_vt100_sequence)
 */
#define  PTY_RD_BUFFER_SIZE  3072

#define  CTL_BEL	0x07
#define  CTL_BS		0x08
#define  CTL_TAB	0x09
#define  CTL_LF		0x0a
#define  CTL_VT		0x0b
#define  CTL_FF		0x0c
#define  CTL_CR		0x0d
#define  CTL_SO		0x0e
#define  CTL_SI		0x0f
#define  CTL_ESC	0x1b

#define  CURRENT_STR_P(vt100_parser) \
	( (vt100_parser)->r_buf.chars + \
		(vt100_parser)->r_buf.filled_len - (vt100_parser)->r_buf.left)

#define  HAS_XTERM_LISTENER(vt100_parser,method) \
	((vt100_parser)->xterm_listener && ((vt100_parser)->xterm_listener->method))

#define  HAS_CONFIG_LISTENER(vt100_parser,method) \
	((vt100_parser)->config_listener && ((vt100_parser)->config_listener->method))

#if  1
#define  MAX_PS_DIGIT  0xffff
#endif

#if  0
#define  EDIT_DEBUG
#endif

#if  0
#define  EDIT_ROUGH_DEBUG
#endif

#if  0
#define  INPUT_DEBUG
#endif

#if  0
#define  ESCSEQ_DEBUG
#endif

#if  0
#define  OUTPUT_DEBUG
#endif

#if  0
#define  DUMP_HEX
#endif

#if  0
#define  SUPPORT_VTE_CJK_WIDTH
#endif


/* --- static variables --- */

static int  use_dec_special_font ;
static int  use_alt_buffer = 1 ;
static int  use_ansi_colors = 1 ;


/* --- static functions --- */

/* XXX This function should be moved to kiklib */
static void
str_replace(
	char *  str ,
	int  c1 ,
	int  c2
	)
{
	while( *str)
	{
		if( *str == c1)
		{
			*str = c2 ;
		}

		str ++ ;
	}
}

/* XXX This function should be moved to mkf */
static u_char
convert_ucs_to_dec_special(
	u_int16_t  ucs
	)
{
	static struct
	{
		u_int16_t  ucs ;
		u_char  decsp ;

	} ucs_to_decsp_table[] =
	{
		/* Not line characters */
	#if  0
		{ 0xa3 , '}' } ,
		{ 0xb0 , 'f' } ,
		{ 0xb1 , 'g' } ,
		{ 0xb7 , '~' } ,
		{ 0x3c0 , '{' } ,
		{ 0x2260 , '|' } ,
		{ 0x2264 , 'y' } ,
		{ 0x2265 , 'z' } ,
	#endif

		/* Line characters */
		{ 0x23ba , 'o' } ,
		{ 0x23bb , 'p' } ,
		{ 0x23bc , 'r' } ,
		{ 0x23bd , 's' } ,
		{ 0x2500 , 'q' } ,
		{ 0x2502 , 'x' } ,
		{ 0x250c , 'l' } ,
		{ 0x2510 , 'k' } ,
		{ 0x2514 , 'm' } ,
		{ 0x2518 , 'j' } ,
		{ 0x251c , 't' } ,
		{ 0x2524 , 'u' } ,
		{ 0x252c , 'w' } ,
		{ 0x2534 , 'v' } ,
		{ 0x253c , 'n' } ,

		{ 0x2592 , 'a' } ,
		{ 0x25c6 , '`' } ,
	} ;
	int  l_idx ;
	int  h_idx ;
	int  idx ;

	l_idx = 0 ;
	h_idx = sizeof(ucs_to_decsp_table) / sizeof(ucs_to_decsp_table[0]) - 1 ;

	if( ucs < ucs_to_decsp_table[l_idx].ucs ||
	    ucs_to_decsp_table[h_idx].ucs < ucs)
	{
		return  0 ;
	}

	while( 1)
	{
		idx = (l_idx + h_idx) / 2 ;

		if( ucs == ucs_to_decsp_table[idx].ucs)
		{
			return  ucs_to_decsp_table[idx].decsp ;
		}
		else if( ucs < ucs_to_decsp_table[idx].ucs)
		{
			h_idx = idx ;
		}
		else
		{
			l_idx = idx + 1 ;
		}

		if( l_idx >= h_idx)
		{
			return  0 ;
		}
	}
}

static void
start_vt100_cmd(
	ml_vt100_parser_t *  vt100_parser ,
	int  trigger_xterm_event	/* dispatch to x_screen or not. */
	)
{
	ml_set_use_char_combining( vt100_parser->use_char_combining) ;
	ml_set_use_multi_col_char( vt100_parser->use_multi_col_char) ;

	if( trigger_xterm_event && HAS_XTERM_LISTENER(vt100_parser,start))
	{
		/*
		 * XXX Adhoc implementation.
		 * Converting visual -> logical in xterm_listener->start.
		 */
		(*vt100_parser->xterm_listener->start)( vt100_parser->xterm_listener->self) ;
	}
	else
	{
		ml_screen_logical( vt100_parser->screen) ;
	}
}

static void
stop_vt100_cmd(
	ml_vt100_parser_t *  vt100_parser ,
	int  trigger_xterm_event	/* dispatch to x_screen or not. */
	)
{
	ml_screen_render( vt100_parser->screen) ;
	ml_screen_visual( vt100_parser->screen) ;

	if( trigger_xterm_event && HAS_XTERM_LISTENER(vt100_parser,stop))
	{
		(*vt100_parser->xterm_listener->stop)( vt100_parser->xterm_listener->self) ;
	}
}

static int
change_read_buffer_size(
	ml_read_buffer_t *  r_buf ,
	size_t  len
	)
{
	void *  p ;

	if( ! ( p = realloc( r_buf->chars , len)))
	{
		return  0 ;
	}

	r_buf->chars = p ;
	r_buf->len = len ;

	/*
	 * Not check if r_buf->left and r_buf->filled_len is larger than r_buf->len.
	 * It should be checked before calling this function.
	 */

	return  1 ;
}

static int
receive_bytes(
	ml_vt100_parser_t *  vt100_parser
	)
{
	size_t  ret ;

	if( 0 < vt100_parser->r_buf.left &&
	    vt100_parser->r_buf.left < vt100_parser->r_buf.filled_len)
	{
		memmove( vt100_parser->r_buf.chars , CURRENT_STR_P(vt100_parser) ,
			vt100_parser->r_buf.left * sizeof( u_char)) ;
	}
	else if( vt100_parser->r_buf.left == vt100_parser->r_buf.len)
	{
		/* Buffer is full => Expand buffer */

		if( ! change_read_buffer_size( &vt100_parser->r_buf ,
			vt100_parser->r_buf.len + PTY_RD_BUFFER_SIZE))
		{
			return  1 ;
		}
	}

	if( ( ret = ml_read_pty( vt100_parser->pty ,
			vt100_parser->r_buf.chars + vt100_parser->r_buf.left ,
			/*
			 * vt100_parser->r_buf.left must be always less than
			 * vt100_parser->r_buf.len
			 */
			vt100_parser->r_buf.len - vt100_parser->r_buf.left)) == 0)
	{
		vt100_parser->r_buf.filled_len = vt100_parser->r_buf.left ;
	
		return  0 ;
	}

	if( vt100_parser->logging_vt_seq)
	{
		if( vt100_parser->log_file == -1)
		{
			char *  dev ;
			char *  path ;

			if( ! ( path = alloca( 11 +
			                       strlen( ( dev = ml_pty_get_slave_name(
			                                        vt100_parser->pty)) + 5) + 1)))
			{
				goto  end ;
			}

			/* +5 removes "/dev/" */
			sprintf( path , "mlterm/%s.log" , dev + 5) ;
			str_replace( path + 7 , '/' , '_') ;

			if( ( path = kik_get_user_rc_path( path)) == NULL)
			{
				goto  end ;
			}

			if( ( vt100_parser->log_file =
				open( path , O_CREAT | O_APPEND | O_WRONLY , 0600)) == -1)
			{
				free( path) ;

				goto  end ;
			}

			free( path) ;

			kik_file_set_cloexec( vt100_parser->log_file) ;
		}

		write( vt100_parser->log_file ,
			vt100_parser->r_buf.chars + vt100_parser->r_buf.left , ret) ;
	#ifndef  USE_WIN32API
		fsync( vt100_parser->log_file) ;
	#endif
	}
	else
	{
		if ( vt100_parser->log_file != -1)
		{
			close( vt100_parser->log_file) ;
			vt100_parser->log_file = -1 ;
		}
        }

end:
	vt100_parser->r_buf.filled_len = ( vt100_parser->r_buf.left += ret) ;

	if( vt100_parser->r_buf.filled_len <= PTY_RD_BUFFER_SIZE)
	{
		/* Shrink buffer */

		if( ! change_read_buffer_size( &vt100_parser->r_buf , PTY_RD_BUFFER_SIZE))
		{
			return  1 ;
		}
	}

#ifdef  INPUT_DEBUG
	{
		size_t  count ;

		kik_debug_printf( KIK_DEBUG_TAG " pty msg (len %d) is received:" ,
			vt100_parser->r_buf.left) ;

		for( count = 0 ; count < vt100_parser->r_buf.left ; count ++)
		{
		#ifdef  DUMP_HEX
			if( isprint( vt100_parser->r_buf.chars[count]))
			{
				kik_msg_printf( "%c " , vt100_parser->r_buf.chars[count]) ;
			}
			else
			{
				kik_msg_printf( "%.2x " , vt100_parser->r_buf.chars[count]) ;
			}
		#else
			kik_msg_printf( "%c" , vt100_parser->r_buf.chars[count]) ;
		#endif
		}

		kik_msg_printf( "[END]\n") ;
	}
#endif

	return  1 ;
}

/*
 * If buffer exists, vt100_parser->w_buf.last_ch is cached.
 * If buffer doesn't exist, vt100_parser->w_buf.last_ch is cleared.
 */
static int
flush_buffer(
	ml_vt100_parser_t *  vt100_parser
	)
{
	ml_write_buffer_t *  buffer ;

	buffer = &vt100_parser->w_buf ;

	if( buffer->filled_len == 0)
	{
		/* last_ch is cleared. */
		buffer->last_ch = NULL ;

		return  0 ;
	}
	
#ifdef  OUTPUT_DEBUG
	{
		u_int  count ;

		kik_msg_printf( "\nflushing chars(%d)...==>" , buffer->filled_len) ;
		for( count = 0 ; count < buffer->filled_len ; count ++)
		{
			char *  bytes ;

			bytes = ml_char_bytes( &buffer->chars[count]) ;

			if( ml_char_size( &buffer->chars[count]) == 2)
			{
			#ifdef  DUMP_HEX
				kik_msg_printf( "%x%x" , bytes[0] | 0x80 , bytes[1] | 0x80) ;
			#else
				kik_msg_printf( "%c%c" , bytes[0] | 0x80 , bytes[1] | 0x80) ;
			#endif
			}
			else
			{
			#ifdef  DUMP_HEX
				kik_msg_printf( "%x" , bytes[0]) ;
			#else
				kik_msg_printf( "%c" , bytes[0]) ;
			#endif
			}
		}

		kik_msg_printf( "<===\n") ;
	}
#endif

	(*buffer->output_func)( vt100_parser->screen , buffer->chars , buffer->filled_len) ;

	/* last_ch which will be used & cleared in REP sequence is cached. */
	buffer->last_ch = &buffer->chars[buffer->filled_len - 1] ;
	/* buffer is cleared. */
	buffer->filled_len = 0 ;

#ifdef  EDIT_DEBUG
	ml_edit_dump( vt100_parser->screen->edit) ;
#endif

	return  1 ;
}

static void
put_char(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  ch ,
	size_t  len ,
	mkf_charset_t  cs ,
	mkf_property_t  prop
	)
{
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	int  is_biwidth ;
	int  is_comb ;

	if( vt100_parser->w_buf.filled_len == PTY_WR_BUFFER_SIZE)
	{
		flush_buffer( vt100_parser) ;
	}

	/*
	 * checking width property of the char.
	 */

	is_biwidth = 0 ;

	if( prop & MKF_BIWIDTH)
	{
		is_biwidth = 1 ;
	}
	else if( prop & MKF_AWIDTH)
	{
	#ifdef  SUPPORT_VTE_CJK_WIDTH
		char *  env ;
	#endif

		if( vt100_parser->col_size_of_width_a == 2)
		{
			is_biwidth = 1 ;
		}
	#if  1
		/* XTERM compatibility */
		else if( ch[2] == 0x30 && ch[0] == 0x0 && ch[1] == 0x0 &&
			(ch[3] == 0x0a || ch[3] == 0x0b || ch[3] == 0x1a || ch[3] == CTL_ESC) )
		{
			is_biwidth = 1 ;
		}
	#endif
	#ifdef  SUPPORT_VTE_CJK_WIDTH
		else if( ( env = getenv( "VTE_CJK_WIDTH")) &&
		         ( strcmp( env , "wide") == 0 || strcmp( env , "1") == 0))
		{
			is_biwidth = 1 ;
		}
	#endif
	}

#ifdef  __DEBUG
	kik_debug_printf( "%.2x%.2x%.2x%.2x %d %x => %s\n" , ch[0] , ch[1] , ch[2] , ch[3] ,
			len , cs , is_biwidth ? "Biwidth" : "Single") ;
#endif

	if( prop & MKF_COMBINING)
	{
		is_comb = 1 ;
	}
	else
	{
		is_comb = 0 ;
	}

	if( vt100_parser->cs != cs)
	{
		vt100_parser->cs = cs ;
	}

	if( vt100_parser->is_reversed)
	{
		fg_color = vt100_parser->bg_color ;
		bg_color = vt100_parser->fg_color ;
	}
	else
	{
		fg_color = vt100_parser->fg_color ;
		bg_color = vt100_parser->bg_color ;
	}

	if( ! vt100_parser->screen->use_dynamic_comb && is_comb)
	{
		if( vt100_parser->w_buf.filled_len == 0)
		{
			/*
			 * ml_line_set_modified() is done in ml_screen_combine_with_prev_char()
			 * internally.
			 */
			if( ml_screen_combine_with_prev_char( vt100_parser->screen ,
				ch , len , vt100_parser->cs , is_biwidth , is_comb ,
				fg_color , bg_color ,
				vt100_parser->is_bold , vt100_parser->is_underlined))
			{
				return ;
			}
		}
		else
		{
			if( ml_char_combine(
				&vt100_parser->w_buf.chars[vt100_parser->w_buf.filled_len - 1] ,
				ch , len , vt100_parser->cs , is_biwidth , is_comb ,
				fg_color , bg_color ,
				vt100_parser->is_bold , vt100_parser->is_underlined))
			{
				return ;
			}
		}

		/*
		 * if combining failed , char is normally appended.
		 */
	}

	ml_char_set( &vt100_parser->w_buf.chars[vt100_parser->w_buf.filled_len++] , ch , len ,
		vt100_parser->cs , is_biwidth , is_comb ,
		fg_color , bg_color , vt100_parser->is_bold , vt100_parser->is_underlined) ;

	if( ! vt100_parser->screen->use_dynamic_comb && cs == ISO10646_UCS4_1)
	{
		/*
		 * Arabic combining
		 */

		ml_char_t *  prev2 ;
		ml_char_t *  prev ;
		ml_char_t *  cur ;
		int  n ;

		cur = &vt100_parser->w_buf.chars[vt100_parser->w_buf.filled_len - 1] ;
		n = 0 ;

		if( vt100_parser->w_buf.filled_len >= 2)
		{
			prev = cur - 1 ;
		}
		else
		{
			if( ! ( prev = ml_screen_get_n_prev_char( vt100_parser->screen , ++n)))
			{
				return ;
			}
		}
		
		if( vt100_parser->w_buf.filled_len >= 3)
		{
			prev2 = cur - 2  ;
		}
		else
		{
			/* possibly NULL */
			prev2 = ml_screen_get_n_prev_char( vt100_parser->screen , ++n) ;
		}
		
		if( ml_is_arabic_combining( prev2 , prev , cur))
		{
			if( vt100_parser->w_buf.filled_len >= 2)
			{
				if( ml_char_combine( prev ,
					ch , len , vt100_parser->cs , is_biwidth , is_comb ,
					fg_color , bg_color ,
					vt100_parser->is_bold , vt100_parser->is_underlined))
				{
					vt100_parser->w_buf.filled_len -- ;
				}
			}
			else
			{
				/*
				 * ml_line_set_modified() is done in
				 * ml_screen_combine_with_prev_char() internally.
				 */
				if( ml_screen_combine_with_prev_char( vt100_parser->screen ,
					ch , len , vt100_parser->cs , is_biwidth , is_comb ,
					fg_color , bg_color ,
					vt100_parser->is_bold , vt100_parser->is_underlined))
				{
					vt100_parser->w_buf.filled_len -- ;
				}
			}
		}
	}
}

static void
push_to_saved_names(
	ml_vt100_saved_names_t *  saved ,
	char *  name
	)
{
	void *  p ;

	if( ! ( p = realloc( saved->names , (saved->num + 1) * sizeof(name))))
	{
		return ;
	}

	saved->names = p ;
	saved->names[saved->num ++] = name ? strdup( name) : NULL ;
}

static char *
pop_from_saved_names(
	ml_vt100_saved_names_t *  saved
	)
{
	char *  name ;

	name = saved->names[--saved->num] ;

	if( saved->num == 0)
	{
		free( saved->names) ;
		saved->names = NULL ;
	}

	return  name ;
}


/*
 * VT100_PARSER Escape Sequence Commands.
 */

static void
save_cursor(
	ml_vt100_parser_t *  vt100_parser
	)
{
	ml_vt100_storable_states_t *  dest ;

	dest = (ml_screen_is_alternative_edit( vt100_parser->screen)) ?
		&(vt100_parser->saved_alternate)
		: &(vt100_parser->saved_normal) ;
	dest->is_saved = 1 ;
	dest->fg_color = vt100_parser->fg_color ;
	dest->bg_color = vt100_parser->bg_color ;
	dest->is_bold  = vt100_parser->is_bold ;
	dest->is_underlined = vt100_parser->is_underlined ;
	dest->is_reversed   = vt100_parser->is_reversed ;
	dest->cs = vt100_parser->cs ;

	ml_screen_save_cursor( vt100_parser->screen) ;
}

static void
restore_cursor(
	ml_vt100_parser_t *  vt100_parser
	)
{
        ml_vt100_storable_states_t *src;

        src = (ml_screen_is_alternative_edit(  vt100_parser->screen) ) ?
                &(vt100_parser->saved_alternate)
                : &(vt100_parser->saved_normal) ;
	if( src->is_saved)
	{
		vt100_parser->fg_color = src->fg_color ;
		vt100_parser->bg_color = src->bg_color ;
		vt100_parser->is_bold  = src->is_bold ;
		vt100_parser->is_underlined = src->is_underlined ;
		vt100_parser->is_reversed = src->is_reversed ;
		if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
		{
			if( ( src->cs == DEC_SPECIAL)
			 && ( src->cs != vt100_parser->cs) )
			{
				/* force grapchics mode by sending \E(0 to current parser*/
				u_char  DEC_SEQ[] = { CTL_ESC, '(', '0'} ;
				mkf_char_t  ch ;
				mkf_parser_t *  parser;
					
				ml_init_encoding_parser( vt100_parser) ;
				parser = vt100_parser->cc_parser;
				(*parser->set_str)( parser, DEC_SEQ, sizeof(DEC_SEQ)) ;
				(*parser->next_char)( parser, &ch) ;
			}
		}
		else
		{
			/* XXX: what to do for g0/g1? */
			if( src->cs == DEC_SPECIAL)
			{
				vt100_parser->gl = DEC_SPECIAL ;
			}
			else
			{
				vt100_parser->gl = US_ASCII ;
			}
		}
	}
	ml_screen_restore_cursor( vt100_parser->screen) ;
}

static void
resize(
	ml_vt100_parser_t *  vt100_parser ,
	u_int  width ,
	u_int  height ,
	int  by_char
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,resize))
	{
		if( by_char)
		{
			/*
			 * XXX Not compatible with xterm.
			 * width(cols) or height(rows) == 0 means full screen width
			 * or height in xterm.
			 * Note that ml_vt100_parser.c depends on following behavior.
			 */
			if( width == 0)
			{
				width = ml_screen_get_cols( vt100_parser->screen) ;
			}

			if( height == 0)
			{
				height = ml_screen_get_rows( vt100_parser->screen) ;
			}

			ml_set_pty_winsize( vt100_parser->pty , width , height) ;
			ml_screen_resize( vt100_parser->screen , width , height) ;

			/*
			 * xterm_listener::resize(0,0) means that screen should be
			 * resized according to the size of pty.
			 */
			width = 0 ;
			height = 0 ;
		}

		stop_vt100_cmd( vt100_parser , 0) ;
		(*vt100_parser->xterm_listener->resize)(
			vt100_parser->xterm_listener->self , width , height) ;
		start_vt100_cmd( vt100_parser , 0) ;
	}
}

static void
reverse_video(
	ml_vt100_parser_t *  vt100_parser ,
	int  flag
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,reverse_video))
	{
		stop_vt100_cmd( vt100_parser , 0) ;
		(*vt100_parser->xterm_listener->reverse_video)(
			vt100_parser->xterm_listener->self , flag) ;
		start_vt100_cmd( vt100_parser , 0) ;
	}
}

static void
set_mouse_report(
	ml_vt100_parser_t *  vt100_parser ,
	ml_mouse_report_mode_t  mode
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,set_mouse_report))
	{
		stop_vt100_cmd( vt100_parser , 0) ;

		(*vt100_parser->xterm_listener->set_mouse_report)(
			vt100_parser->xterm_listener->self ,
			(vt100_parser->mouse_mode = mode)) ;

		start_vt100_cmd( vt100_parser , 0) ;
	}
}

static void
set_window_name(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  name		/* should be malloc'ed or NULL. */
	)
{
	free( vt100_parser->win_name) ;
	vt100_parser->win_name = name ;

	if( HAS_XTERM_LISTENER(vt100_parser,set_window_name))
	{
	#if  0
		stop_vt100_cmd( vt100_parser , 0) ;
	#endif
		(*vt100_parser->xterm_listener->set_window_name)(
			vt100_parser->xterm_listener->self , name) ;
	#if  0
		start_vt100_cmd( vt100_parser , 0) ;
	#endif
	}
}

static void
set_icon_name(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  name		/* should be malloc'ed or NULL. */
	)
{
	free( vt100_parser->icon_name) ;
	vt100_parser->icon_name = name ;

	if( HAS_XTERM_LISTENER(vt100_parser,set_icon_name))
	{
	#if  0
		stop_vt100_cmd( vt100_parser , 0) ;
	#endif
		(*vt100_parser->xterm_listener->set_icon_name)(
			vt100_parser->xterm_listener->self , name) ;
	#if  0
		start_vt100_cmd( vt100_parser , 0) ;
	#endif
	}
}

static void
switch_im_mode(
	ml_vt100_parser_t *  vt100_parser
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,switch_im_mode))
	{
	#if  0
		stop_vt100_cmd( vt100_parser , 0) ;
	#endif
		(*vt100_parser->xterm_listener->switch_im_mode)(
					vt100_parser->xterm_listener->self) ;
	#if  0
		start_vt100_cmd( vt100_parser , 0) ;
	#endif
	}
}

static int
im_is_active(
	ml_vt100_parser_t *  vt100_parser
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,im_is_active))
	{
		return  (*vt100_parser->xterm_listener->im_is_active)(
					vt100_parser->xterm_listener->self) ;
	}
	else
	{
		return  0 ;
	}
}

static void
set_modkey_mode(
	ml_vt100_parser_t *  vt100_parser ,
	int  key ,
	int  mode
	)
{
#if  0
	if( key == 1 && mode <= 3)
	{
		vt100_parser->modify_cursor_keys = mode ;
	}
	else if( key == 2 && mode <= 3)
	{
		vt100_parser->modify_function_keys = mode ;
	}
	else
#endif
	if( key == 4 && mode <= 2)
	{
		vt100_parser->modify_other_keys = mode ;
	}
}

static void
report_window_size(
	ml_vt100_parser_t *  vt100_parser ,
	int  by_char
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,get_window_size))
	{
		int  ps ;
		u_int  width ;
		u_int  height ;
		char seq[ 5 + 1 /* ps */ + DIGIT_STR_LEN(u_int) * 2 + 1] ;

		if( by_char)
		{
			width = ml_screen_get_logical_cols( vt100_parser->screen) ;
			height = ml_screen_get_logical_rows( vt100_parser->screen) ;
			ps = 8 ;
		}
		else
		{
			if( ! (*vt100_parser->xterm_listener->get_window_size)(
					vt100_parser->xterm_listener->self , &width , &height))
			{
				return ;
			}

			ps = 4 ;
		}

		sprintf( seq , "\x1b[%d;%d;%dt" , ps , height , width) ;
		ml_write_to_pty( vt100_parser->pty , seq , strlen(seq)) ;
	}
}

#ifdef  ENABLE_SIXEL
static void
show_picture(
	ml_vt100_parser_t *  vt100_parser ,
	char *  file_path ,
	int  clip_beg_col ,
	int  clip_beg_row ,
	int  clip_cols ,
	int  clip_rows ,
	int  img_cols ,
	int  img_rows
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,get_picture_data))
	{
		ml_char_t *  data ;

		ml_set_use_char_combining( 1) ;

		if( ( data = (*vt100_parser->xterm_listener->get_picture_data)(
				vt100_parser->xterm_listener->self ,
				file_path , &img_cols , &img_rows)) &&
		    clip_beg_row < img_rows && clip_beg_col < img_cols)
		{
			ml_char_t *  p ;
			int  row ;
			int  cursor_col ;

			if( clip_cols == 0)
			{
				clip_cols = img_cols - clip_beg_col ;
			}

			if( clip_rows == 0)
			{
				clip_rows = img_rows - clip_beg_row ;
			}

			if( clip_beg_row + clip_rows > img_rows)
			{
				clip_rows = img_rows - clip_beg_row ;
			}

			if( clip_beg_col + clip_cols > img_cols)
			{
				clip_cols = img_cols - clip_beg_col ;
			}

			p = data + (img_cols * clip_beg_row) ;
			row = 0 ;
			cursor_col = ml_screen_cursor_col( vt100_parser->screen) ;
			while( 1)
			{
				ml_screen_overwrite_chars( vt100_parser->screen ,
					p + clip_beg_col , clip_cols) ;

				if( ++row >= clip_rows)
				{
					break ;
				}

				ml_screen_line_feed( vt100_parser->screen) ;
				ml_screen_go_horizontally( vt100_parser->screen , cursor_col) ;

				p += img_cols ;
			}

			ml_str_delete( data , img_cols * img_rows) ;
		}

		ml_set_use_char_combining( vt100_parser->use_char_combining) ;
	}
}
#endif


/*
 * This function will destroy the content of pt.
 */
static void  soft_reset( ml_vt100_parser_t *  vt100_parser) ;

static void
config_protocol_set(
	ml_vt100_parser_t *  vt100_parser ,
	char *  pt ,
	int  save
	)
{
	char *  dev ;

	dev = ml_parse_proto_prefix( &pt) ;

	if( strcmp( pt , "gen_proto_challenge") == 0)
	{
		ml_gen_proto_challenge() ;
	}
	else if( strcmp( pt , "full_reset") == 0)
	{
		soft_reset( vt100_parser) ;
	}
#ifdef  ENABLE_SIXEL
	else if( strncmp( pt , "show_picture " , 13) == 0)
	{
		int  clip_beg_col = 0 ;
		int  clip_beg_row = 0 ;
		int  clip_cols = 0 ;
		int  clip_rows = 0 ;
		int  img_cols = 0 ;
		int  img_rows = 0 ;
		char **  argv ;
		int  argc ;

		argv = kik_arg_str_to_array( &argc , pt) ;
		if( argc == 1)
		{
			return ;
		}

		if( argc >= 3)
		{
			int  has_img_size ;

			if( strchr( argv[argc - 1] , '+'))
			{
				sscanf( argv[argc - 1] , "%dx%d+%d+%d" ,
					&clip_cols , &clip_rows ,
					&clip_beg_col , &clip_beg_row) ;

				has_img_size = (argc >= 4) ;
			}
			else
			{
				has_img_size = 1 ;
			}

			if( has_img_size)
			{
				sscanf( argv[2] , "%dx%d" , &img_cols , &img_rows) ;
			}
		}

		show_picture( vt100_parser , argv[1] ,
			clip_beg_col , clip_beg_row , clip_cols , clip_rows ,
				img_cols , img_rows) ;
	}
#endif
#ifdef  USE_LIBSSH2
	else if( strncmp( pt , "scp " , 4) == 0)
	{
		char **  argv ;
		int  argc ;

		argv = kik_arg_str_to_array( &argc , pt) ;

		if( argc == 3 || argc == 4)
		{
			ml_char_encoding_t  encoding ;

			if( ! argv[3] ||
			    ( encoding = ml_get_char_encoding( argv[3])) == ML_UNKNOWN_ENCODING)
			{
				encoding = vt100_parser->encoding ;
			}

			ml_pty_ssh_scp( vt100_parser->pty ,
				vt100_parser->encoding , encoding , argv[2] , argv[1]) ;
		}
	}
#endif
	else if( dev && strlen( dev) <= 7 && strstr( dev , "font"))
	{
		char *  key ;
		char *  val ;

		if( ml_parse_proto( NULL , &key , &val , &pt , 0 , 0) && val &&
		    HAS_CONFIG_LISTENER(vt100_parser,set_font))
		{
			/*
			 * Screen is redrawn not in vt100_parser->config_listener->set_font
			 * but in stop_vt100_cmd, so it is not necessary to hold
			 * vt100_parser->config_listener->set_font between stop_vt100_cmd and
			 * start_vt100_cmd.
			 */
		#if  0
			stop_vt100_cmd( vt100_parser , 0) ;
		#endif

			(*vt100_parser->config_listener->set_font)(
				vt100_parser->config_listener->self ,
				dev , key , val , save) ;

		#if  0
			start_vt100_cmd( vt100_parser , 0) ;
		#endif
		}
	}
	else if( dev && strcmp( dev , "color") == 0)
	{
		char *  key ;
		char *  val ;

		if( ml_parse_proto( NULL , &key , &val , &pt , 0 , 0) && val &&
		    HAS_CONFIG_LISTENER(vt100_parser,set_color))
		{
			/*
			 * Screen is redrawn not in vt100_parser->config_listener->set_color
			 * but in stop_vt100_cmd, so it is not necessary to hold
			 * vt100_parser->config_listener->set_font between stop_vt100_cmd and
			 * start_vt100_cmd.
			 */
		#if  0
			stop_vt100_cmd( vt100_parser , 0) ;
		#endif

			(*vt100_parser->config_listener->set_color)(
				vt100_parser->config_listener->self , dev , key , val, save) ;

		#if  0
			start_vt100_cmd( vt100_parser , 0) ;
		#endif
		}
	}
	else
	{
		stop_vt100_cmd( vt100_parser , 0) ;

		if( ( ! HAS_CONFIG_LISTENER(vt100_parser,exec) ||
	              ! (*vt100_parser->config_listener->exec)(
				vt100_parser->config_listener->self , pt)))
		{
			kik_conf_write_t *  conf ;

			if( save)
			{
				char *  path ;

				/* XXX */
				if( ( path = kik_get_user_rc_path( "mlterm/main")) == NULL)
				{
					return ;
				}

				conf = kik_conf_write_open( path) ;
				free( path) ;
			}
			else
			{
				conf = NULL ;
			}

			/* accept multiple key=value pairs. */
			while( pt)
			{
				char *  key ;
				char *  val ;

				if( ! ml_parse_proto( dev ? NULL : &dev ,
						&key , &val , &pt , 0 , 1))
				{
					break ;
				}

				if( conf)
				{
					/* XXX */
					if( strcmp( key , "encoding") == 0)
					{
						key = "ENCODING" ;
					}

					/* XXX */
					if( strcmp( key , "xim") != 0)
					{
						kik_conf_io_write( conf , key , val) ;
					}
				}

				if( HAS_CONFIG_LISTENER(vt100_parser,set))
				{
					(*vt100_parser->config_listener->set)(
						vt100_parser->config_listener->self ,
						dev , key , val) ;

					if( ! vt100_parser->config_listener)
					{
						/* pty changed. */
						break ;
					}
				}

				dev = NULL ;
			}

			if( conf)
			{
				kik_conf_write_close( conf) ;

				if( HAS_CONFIG_LISTENER(vt100_parser,saved))
				{
					(*vt100_parser->config_listener->saved)() ;
				}
			}
		}

		start_vt100_cmd( vt100_parser , 0) ;
	}
}

static void
config_protocol_set_simple(
	ml_vt100_parser_t *  vt100_parser ,
	char *  key ,
	char *  val
	)
{
	if( HAS_CONFIG_LISTENER(vt100_parser,set))
	{
		stop_vt100_cmd( vt100_parser , 0) ;

		(*vt100_parser->config_listener->set)(
			vt100_parser->config_listener->self , NULL , key , val) ;

		start_vt100_cmd( vt100_parser , 0) ;
	}
}

/*
 * This function will destroy the content of pt.
 */
static void
config_protocol_get(
	ml_vt100_parser_t *  vt100_parser ,
	char *  pt ,
	int  to_menu
	)
{
	char *  dev ;
	char *  key ;
	int  ret ;

	/*
	 * It is assumed that screen is not redrawn not in
	 * vt100_parser->config_listener->get, so vt100_parser->config_listener->get
	 * is not held between stop_vt100_cmd and start_vt100_cmd.
	 */
#if  0
	stop_vt100_cmd( vt100_parser , 0) ;
#endif

	ret = ml_parse_proto( &dev , &key , NULL , &pt , to_menu == 0 , 0) ;
	if( ret == -1)
	{
		/*
		 * do_challenge is failed.
		 * to_menu is necessarily 0, so it is pty that msg should be returned to.
		 */

		char  msg[] = "#forbidden\n" ;

		ml_write_to_pty( vt100_parser->pty , msg , sizeof( msg) - 1) ;

		return ;
	}

	if( ret == 0)
	{
		key = "error" ;
	}

	if( dev && strlen( dev) <= 7 && strstr( dev , "font"))
	{
		char *  cs ;

		if( ret == 0)
		{
			cs = key ;
		}
		else if( ! ( cs = kik_str_sep( &key , ",")) || ! key)
		{
			return ;
		}
		
		if( HAS_CONFIG_LISTENER(vt100_parser,get_font))
		{
			(*vt100_parser->config_listener->get_font)(
				vt100_parser->config_listener->self ,
				dev , key /* font size */ , cs , to_menu) ;
		}
	}
	else if( HAS_CONFIG_LISTENER(vt100_parser,get))
	{
		(*vt100_parser->config_listener->get)(
			vt100_parser->config_listener->self , dev , key , to_menu) ;
	}

#if  0
	start_vt100_cmd( vt100_parser , 0) ;
#endif
}

static int
change_char_fine_color(
	ml_vt100_parser_t *  vt100_parser ,
	int *  ps ,
	int  num
	)
{
	int  proceed ;
	ml_color_t  color ;

	if( ps[0] != 38 && ps[0] != 48)
	{
		return  0 ;
	}

	if( num >= 3 && ps[1] == 5)
	{
		proceed = 3 ;
		color = ps[2] ;
	}
	else if( num >= 5 && ps[1] == 2)
	{
		proceed = 5 ;
		color = ml_get_closest_color( ps[2] , ps[3] , ps[4]) ;
	}
	else
	{
		return  1 ;
	}

	if( use_ansi_colors)
	{
		if( ps[0] == 38)
		{
			vt100_parser->fg_color = color ;
			ml_screen_set_bce_fg_color( vt100_parser->screen , color) ;
		}
		else /* if( ps[0] == 48) */
		{
			vt100_parser->bg_color = color ;
			ml_screen_set_bce_bg_color( vt100_parser->screen , color) ;
		}
	}

	return  proceed ;
}

static void
change_char_attr(
	ml_vt100_parser_t *  vt100_parser ,
	int  flag
	)
{
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;

	fg_color = vt100_parser->fg_color ;
	bg_color = vt100_parser->bg_color ;

	if( flag == 0)
	{
		/* Normal */
		fg_color = ML_FG_COLOR ;
		bg_color = ML_BG_COLOR ;
		vt100_parser->is_bold = 0 ;
		vt100_parser->is_underlined = 0 ;
		vt100_parser->is_reversed = 0 ;
	}
	else if( flag == 1)
	{
		/* Bold */
		vt100_parser->is_bold = 1 ;
	}
	else if( flag == 4)
	{
		/* Underscore */
		vt100_parser->is_underlined = 1 ;
	}
	else if( flag == 5)
	{
		/* Blink */
	}
	else if( flag == 7)
	{
		/* Inverse */
		vt100_parser->is_reversed = 1 ;
	}
#if  0
	else if( flag == 8)
	{
		/* Hidden */
	}
#endif
	else if( flag == 22)
	{
		/* Bold */
		vt100_parser->is_bold = 0 ;
	}
	else if( flag == 24)
	{
		vt100_parser->is_underlined = 0 ;
	}
	else if( flag == 25)
	{
		/* blink */
	}
	else if( flag == 27)
	{
		vt100_parser->is_reversed = 0 ;
	}
#if  0
	else if( flag == 28)
	{
		/* Not hidden */
	}
#endif
	else if( use_ansi_colors)
	{
		/* Color attributes */

		if( 30 <= flag && flag <= 37)
		{
			/* 30=ML_BLACK(0) ... 37=ML_WHITE(7) */
			fg_color = flag - 30 ;
		}
		else if( flag == 39)
		{
			/* default fg */
			fg_color = ML_FG_COLOR ;
		}
		else if( 40 <= flag && flag <= 47)
		{
			/* 40=ML_BLACK(0) ... 47=ML_WHITE(7) */
			bg_color = flag - 40 ;
		}
		else if( flag == 49)
		{
			bg_color = ML_BG_COLOR ;
		}
		else if( 90 <= flag && flag <= 97)
		{
			fg_color = (flag - 90) | ML_BOLD_COLOR_MASK ;
		}
		else if( 100 <= flag && flag <= 107)
		{
			bg_color = (flag - 100) | ML_BOLD_COLOR_MASK ;
		}
	#ifdef  DEBUG
		else
		{
			kik_warn_printf( KIK_DEBUG_TAG " unknown char attr flag(%d).\n" , flag) ;
		}
	#endif
	}

	if( fg_color != vt100_parser->fg_color)
	{
		vt100_parser->fg_color = fg_color ;
		ml_screen_set_bce_fg_color( vt100_parser->screen , fg_color) ;
	}

	if( bg_color != vt100_parser->bg_color)
	{
		vt100_parser->bg_color = bg_color ;
		ml_screen_set_bce_bg_color( vt100_parser->screen , bg_color) ;
	}
}

static void
get_rgb(
	ml_vt100_parser_t *  vt100_parser ,
	int  ps ,
	ml_color_t  color
	)
{
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	char  rgb[] = "rgb:xxxx/xxxx/xxxx" ;
	char  seq[2 + (DIGIT_STR_LEN(int) + 1) * 2 + sizeof(rgb) + 1] ;

	if( ps >= 10) /* IS_FG_BG_COLOR(color) */
	{
		if( ! HAS_XTERM_LISTENER(vt100_parser,get_rgb) ||
		    ! (*vt100_parser->xterm_listener->get_rgb)(
			vt100_parser->xterm_listener->self , &red , &green , &blue , color))
		{
			return ;
		}
	}
	else if( ! ml_get_color_rgba( color , &red , &green , &blue , NULL))
	{
		return ;
	}

	sprintf( rgb + 4 , "%.2x%.2x/%.2x%.2x/%.2x%.2x" ,
		red , red , green , green , blue , blue) ;

	if( ps >= 10)
	{
		/* ps: 10 = fg , 11 = bg , 12 = cursor bg */
		sprintf( seq , "\x1b]%d;%s\x07" , ps , rgb) ;
	}
	else
	{
		/* ps: 4 , 5 */
		sprintf( seq , "\x1b]%d;%d;%s\x07" , ps , color , rgb) ;
	}

	ml_write_to_pty( vt100_parser->pty , seq , strlen(seq)) ;
}

/*
 * This function will destroy the content of pt.
 */
static void
change_color_rgb(
	ml_vt100_parser_t *  vt100_parser,
	u_char *  pt
	)
{
	char *  p ;
	
	if( ( p = strchr( pt, ';')))
	{
		if( strcmp( p + 1 , "?") == 0)
		{
			ml_color_t  color ;

			*p = '\0' ;

			if( ( color = ml_get_color( pt)) != ML_UNKNOWN_COLOR)
			{
				get_rgb( vt100_parser , 4 , color) ;
			}
		}
		else
		{
			*p = '=' ;

			if( ( p = alloca( 6 + strlen( pt) + 1)))
			{
				sprintf( p , "color:%s" , pt) ;

				config_protocol_set( vt100_parser , p , 0) ;
			}
		}
	}
}

static void
change_special_color(
	ml_vt100_parser_t *  vt100_parser,
	u_char *  pt
	)
{
	char *  key ;

	if( *pt == '0')
	{
		key = "bd_color" ;
	}
	else if( *pt == '1')
	{
		key = "ul_color" ;
	}
	else
	{
		return ;
	}

	if( *(++pt) == ';' &&
	    strcmp( ++pt , "?") != 0)	/* ?: query rgb */
	{
		config_protocol_set_simple( vt100_parser , key , pt) ;
	}
}

static void
set_selection(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  encoded
	)
{
	if( HAS_XTERM_LISTENER(vt100_parser,set_selection))
	{
		size_t  e_len ;
		u_char *  decoded ;
		size_t  d_pos ;
		size_t  e_pos ;
		/* ASCII -> Base64 order */
		int8_t  conv_tbl[] =
		{
			/* 0x2b - */
			62, -1, -1, -1, 63,
			/* 0x30 - */
			52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -2, -1, -1,
			/* 0x40 - */
			-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
			/* 0x50 - */
			15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
			/* 0x60 - */
			-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
			/* 0x70 - 7a */
			41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
		} ;
		mkf_char_t  ch ;
		ml_char_t *  str ;
		u_int  str_len ;

		if( ( e_len = strlen( encoded)) < 4 || ! ( decoded = alloca( e_len)))
		{
			return ;
		}

		d_pos = e_pos = 0 ;

		while( e_len >= e_pos + 4)
		{
			size_t  count ;
			int8_t  bytes[4] ;

			for( count = 0 ; count < 4 ; count++)
			{
				if( encoded[e_pos] < 0x2b || 0x7a < encoded[e_pos] ||
				    (bytes[count] = conv_tbl[encoded[e_pos++] - 0x2b]) == -1)
				{
				#ifdef  DEBUG
					kik_debug_printf( KIK_DEBUG_TAG
						" Illegal Base64 %s\n" , encoded) ;
				#endif

					return ;
				}
			}

			decoded[d_pos++] = (((bytes[0] << 2) & 0xfc) | ((bytes[1] >> 4) & 0x3)) ;

			if( bytes[2] != -2)
			{
				decoded[d_pos++] =
					(((bytes[1] << 4) & 0xf0) | ((bytes[2] >> 2) & 0xf)) ;
			}
			else
			{
				break ;
			}

			if( bytes[3] != -2)
			{
				decoded[d_pos++] = (((bytes[2] << 6) & 0xc0) | (bytes[3] & 0x3f)) ;
			}
			else
			{
				break ;
			}
		}

	#ifdef  DEBUG
		decoded[d_pos] = '\0' ;
		kik_debug_printf( KIK_DEBUG_TAG " Base64 Decode %s => %s\n" , encoded , decoded) ;
	#endif

		if( ! ( str = ml_str_new( d_pos)))
		{
			return ;
		}

		str_len = 0 ;
		(*vt100_parser->cc_parser->set_str)( vt100_parser->cc_parser , decoded , d_pos) ;
		while( (*vt100_parser->cc_parser->next_char)( vt100_parser->cc_parser , &ch))
		{
			ml_char_set( &str[str_len++] , ch.ch , ch.size , ch.cs , 0 , 0 ,
				0 , 0 , 0 , 0) ;
		}

		/*
		 * It is assumed that screen is not redrawn not in
		 * vt100_parser->config_listener->get, so vt100_parser->config_listener->get
		 * is not held between stop_vt100_cmd and start_vt100_cmd.
		 */
	#if  0
		stop_vt100_cmd( vt100_parser , 0) ;
	#endif

		(*vt100_parser->xterm_listener->set_selection)(
			vt100_parser->xterm_listener->self , str , str_len) ;

	#if  0
		start_vt100_cmd( vt100_parser , 0) ;
	#endif
	}
}


static void
clear_line_all(
	ml_vt100_parser_t *  vt100_parser
	)
{
	/*
	 * XXX
	 * cursor position should be restored.
	 */
	ml_screen_goto_beg_of_line( vt100_parser->screen) ;
	ml_screen_clear_line_to_right( vt100_parser->screen) ;
}

static void
clear_display_all(
	ml_vt100_parser_t *  vt100_parser
	)
{
	/*
	 * XXX
	 * cursor position should be restored.
	 */
	ml_screen_goto_home( vt100_parser->screen) ;
	ml_screen_clear_below( vt100_parser->screen) ;
}

static void
soft_reset(
	ml_vt100_parser_t *  vt100_parser
	)
{
	/*
	 * XXX insufficient implementation.
	 */

	/* "CSI ? 25 h" (DECTCEM) */
	ml_screen_cursor_visible( vt100_parser->screen) ;

	/* "CSI l" (IRM) */
	vt100_parser->w_buf.output_func = ml_screen_overwrite_chars ;

	/* "CSI ? 6 l" (DECOM) */
	ml_screen_set_absolute_origin( vt100_parser->screen) ;

	/*
	 * "CSI ? 7 h" (DECAWM) (xterm compatible behavior)
	 * ("CSI ? 7 l" according to VT220 reference manual)
	 */
	ml_screen_set_auto_wrap( vt100_parser->screen , 1) ;

	/* "CSI r" (DECSTBM) */
	ml_screen_set_scroll_region( vt100_parser->screen , -1 , -1) ;

	/* "CSI m" (SGR) */
	change_char_attr( vt100_parser , 0) ;

	ml_init_encoding_parser( vt100_parser) ;

	( ml_screen_is_alternative_edit( vt100_parser->screen) ?
		&vt100_parser->saved_alternate
		: &vt100_parser->saved_normal)->is_saved = 0 ;

	vt100_parser->mouse_mode = 0 ;
	vt100_parser->ext_mouse_mode = 0 ;
	vt100_parser->is_app_keypad = 0 ;
	vt100_parser->is_app_cursor_keys = 0 ;
	vt100_parser->is_app_escape = 0 ;
	vt100_parser->is_bracketed_paste_mode = 0 ;

	vt100_parser->im_is_active = 0 ;
}

static void
send_device_attributes(
	ml_pty_ptr_t  pty ,
	int  rank
	)
{
	char *  seq ;

	if( rank == 1)
	{
		/* vt100 answerback */
	#ifdef  ENABLE_SIXEL
		seq = "\x1b[?1;2;4;7c" ;
	#else
		seq = "\x1b[?1;2;7c" ;
	#endif
	}
	else if( rank == 2)
	{
		/*
		 * If Pv is greater than 95, vim sets ttymouse=xterm2
		 * automatically.
		 */
		seq = "\x1b[>1;96;0c" ;
	}
	else
	{
		return ;
	}

	ml_write_to_pty( pty , seq , strlen(seq)) ;
}


/*
 * For string outside escape sequences.
 */
static int
increment_str(
	u_char **  str ,
	size_t *  left
	)
{
	if( -- (*left) == 0)
	{
		return  0 ;
	}

	(*str) ++ ;

	return  1 ;
}

/*
 * For string inside escape sequences.
 */
static int
inc_str_in_esc_seq(
	ml_screen_t *  screen ,
	u_char **  str_p ,
	size_t *  left ,
	int  want_ctrl
	)
{
	while( 1)
	{
		if( increment_str( str_p , left) == 0)
		{
			return  0 ;
		}

		if( **str_p < 0x20 || 0x7e < **str_p)
		{
			/*
			 * cursor-control characters inside ESC sequences
			 */
			if( CTL_LF <= **str_p && **str_p <= CTL_FF)
			{
				ml_screen_line_feed( screen) ;
			}
			else if( **str_p == CTL_CR)
			{
				ml_screen_goto_beg_of_line( screen) ;
			}
			else if( **str_p == CTL_BS)
			{
				ml_screen_go_back( screen , 1) ;
			}
			else if( want_ctrl)
			{
				return  1 ;
			}
			else
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG
					" Ignored 0x%x inside escape sequences.\n" , **str_p) ;
			#endif
			}
		}
		else
		{
			return  1 ;
		}
	}
}

static char *
get_pt_in_esc_seq(
	u_char **  str ,
	size_t *  left ,
	int  bel_terminate	/* OSC is terminated by not only ST(ESC \) but also BEL. */
	)
{
	u_char *  pt ;

	pt = *str ;

	/* UTF-8 uses 0x80-0x9f as printable characters. */
	while( 0x20 <= **str && **str != 0x7f)
	{
		if( ! increment_str( str , left))
		{
			return  NULL ;
		}
	}

	if( bel_terminate && **str == CTL_BEL)
	{
		**str = '\0' ;
	}
	else if(
		/*
		 * 0x9c is not regarded as ST here, because it is used by utf8 encoding
		 * for non control characters.
		 */
	#if  0
		**str == 0x9c ||
	#endif
	         ( **str == CTL_ESC && *left > 1 && *((*str) + 1) == '\\'))
	{
		**str = '\0' ;
		increment_str( str , left) ;
	}
	else
	{
		/* Reset position ahead of unprintable character for compat with xterm. */
		(*str) -- ;
		(*left) ++ ;

		return  NULL ;
	}

	return  pt ;
}

#ifdef  DEBUG
static void
debug_print_unknown(
	const char *  format ,
	...
	)
{
	va_list  arg_list ;

	va_start( arg_list , format) ;

	kik_debug_printf( KIK_DEBUG_TAG " received unknown sequence ") ;
	vfprintf( stderr , format , arg_list) ;
}
#endif


#ifdef  USE_VT52
inline static int
parse_vt52_escape_sequence(
	ml_vt100_parser_t *  vt100_parser	/* vt100_parser->r_buf.left must be more than 0 */
	)
{
	u_char *  str_p ;
	size_t  left ;

	str_p = CURRENT_STR_P(vt100_parser) ;
	left = vt100_parser->r_buf.left ;

	if( ! inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0))
	{
		return  0 ;
	}

	if( *str_p == 'A')
	{
		ml_screen_go_upward( vt100_parser->screen , 1) ;
	}
	else if( *str_p == 'B')
	{
		ml_screen_go_downward( vt100_parser->screen , 1) ;
	}
	else if( *str_p == 'C')
	{
		ml_screen_go_forward( vt100_parser->screen , 1) ;
	}
	else if( *str_p == 'D')
	{
		ml_screen_go_back( vt100_parser->screen , 1) ;
	}
	else if( *str_p == 'F')
	{
		/* Enter graphics mode */
	}
	else if( *str_p == 'G')
	{
		/* Exit graphics mode */
	}
	else if( *str_p == 'H')
	{
		ml_screen_goto( vt100_parser->screen , 0 , 0) ;
	}
	else if( *str_p == 'I')
	{
		if( ml_screen_cursor_row( vt100_parser->screen) == 0)
		{
			ml_screen_scroll_downward( vt100_parser->screen , 1) ;
		}
		else
		{
			ml_screen_go_upward( vt100_parser->screen , 1) ;
		}
	}
	else if( *str_p == 'J')
	{
		ml_screen_clear_below( vt100_parser->screen) ;
	}
	else if( *str_p == 'K')
	{
		ml_screen_clear_line_to_right( vt100_parser->screen) ;
	}
	else if( *str_p == 'Y')
	{
		int  row ;
		int  col ;

		if( ! inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0))
		{
			return  0 ;
		}

		if( *str_p < ' ')
		{
			goto  end ;
		}

		row = *str_p - ' ' ;

		if( ! inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0))
		{
			return  0 ;
		}

		if( *str_p < ' ')
		{
			goto  end ;
		}

		col = *str_p - ' ' ;

		ml_screen_goto( vt100_parser->screen , col , row) ;
	}
	else if( *str_p == 'Z')
	{
		char  msg[] = "\x1b/Z" ;

		ml_write_to_pty( vt100_parser->pty , msg , sizeof( msg) - 1) ;
	}
	else if( *str_p == '=')
	{
		/* Enter altenate keypad mode */
	}
	else if( *str_p == '>')
	{
		/* Exit altenate keypad mode */
	}
	else if( *str_p == '<')
	{
		vt100_parser->is_vt52_mode = 0 ;
	}
	else
	{
		/* not VT52 control sequence */

		return  1 ;
	}

end:
	vt100_parser->r_buf.left = left - 1 ;

	return  1 ;
}
#endif

/*
 * Parse escape/control sequence. Note that *any* valid format sequence
 * is parsed even if mlterm doesn't support it.
 *
 * Return value:
 * 0 => vt100_parser->r_buf.left == 0
 * 1 => Finished parsing. (If current vt100_parser->r_buf.chars is not escape sequence,
 *      return without doing anthing.)
 */
inline static int
parse_vt100_escape_sequence(
	ml_vt100_parser_t *  vt100_parser	/* vt100_parser->r_buf.left must be more than 0 */
	)
{
	u_char *  str_p ;
	size_t  left ;

#if  0
	if( vt100_parser->r_buf.left == 0)
	{
		/* end of string */

		return  1 ;
	}
#endif

	str_p = CURRENT_STR_P(vt100_parser) ;
	left = vt100_parser->r_buf.left ;

	if( *str_p == CTL_ESC)
	{
	#ifdef  ESCSEQ_DEBUG
		kik_msg_printf( "RECEIVED ESCAPE SEQUENCE(current left = %d: ESC", left) ;
	#endif

	#ifdef  USE_VT52
		if( vt100_parser->is_vt52_mode)
		{
			return  parse_vt52_escape_sequence( vt100_parser) ;
		}
	#endif

		if( ! inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0))
		{
			return  0 ;
		}

		if( *str_p == '7')
		{
			/* "ESC 7" save cursor */

			save_cursor( vt100_parser) ;
		}
		else if( *str_p == '8')
		{
			/* "ESC 8" restore cursor */

			restore_cursor( vt100_parser) ;
		}
		else if( *str_p == '=')
		{
			/* "ESC =" application keypad */

			vt100_parser->is_app_keypad = 1 ;
		}
		else if( *str_p == '>')
		{
			/* "ESC >" normal keypad */

			vt100_parser->is_app_keypad = 0 ;
		}
		else if( *str_p == 'D')
		{
			/* "ESC D" index(scroll up) */

			ml_screen_index( vt100_parser->screen) ;
		}
		else if( *str_p == 'E')
		{
			/* "ESC E" next line */

			ml_screen_line_feed( vt100_parser->screen) ;
			ml_screen_goto_beg_of_line( vt100_parser->screen) ;
		}
	#if  0
		else if( *str_p == 'F')
		{
			/* "ESC F" cursor to lower left corner of screen */
		}
	#endif
		else if( *str_p == 'H')
		{
			/* "ESC H" set tab */

			ml_screen_set_tab_stop( vt100_parser->screen) ;
		}
		else if( *str_p == 'M')
		{
			/* "ESC M" reverse index(scroll down) */

			ml_screen_reverse_index( vt100_parser->screen) ;
		}
		else if( *str_p == 'Z')
		{
			/* "ESC Z" return terminal id (Obsolete form of CSI c) */

			send_device_attributes( vt100_parser->pty , 1) ;
		}
		else if( *str_p == 'c')
		{
			/* "ESC c" full reset */

			soft_reset( vt100_parser) ;
			clear_display_all( vt100_parser) ; /* cursor goes home in it. */
		}
	#if  0
		else if( *str_p == 'l')
		{
			/* "ESC l" memory lock */
		}
	#endif
	#if  0
		else if( *str_p == 'm')
		{
			/* "ESC m" memory unlock */
		}
	#endif
		else if( *str_p == '[')
		{
			/*
			 * "ESC [" (CSI)
			 * CSI P.....P I.....I F
			 *     060-077 040-057 100-176
			 */

		#define  MAX_NUM_OF_PS  10

			u_char  pre_ch ;
			int  ps[MAX_NUM_OF_PS] ;
			int  num ;

			if( ! inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0))
			{
				return  0 ;
			}

			/* Parameter characters(0x30-0x3f) */

			if( 0x3c <= *str_p && *str_p <= 0x3f)
			{
				/* parameter character except numeric, ':' and ';' (< = > ?). */
				pre_ch = *str_p ;

				if( ! inc_str_in_esc_seq( vt100_parser->screen ,
						&str_p , &left , 0))
				{
					return  0 ;
				}
			}
			else
			{
				pre_ch = '\0' ;
			}

			num = 0 ;
			while( 1)
			{
				if( *str_p == ';')
				{
					/*
					 * "CSI ; n" is regarded as "CSI -1 ; n"
					 */
					if( num < MAX_NUM_OF_PS)
					{
						ps[num ++] = -1 ;
					}
				}
				else
				{
					if( '0' <= *str_p && *str_p <= '9')
					{
						u_char  digit[DIGIT_STR_LEN(int) + 1] ;
						int  count ;

						if( *str_p == '0')
						{
							/* 000000001 -> 01 */
							while( left > 1 && *(str_p + 1) == '0')
							{
								str_p ++ ;
								left -- ;
							}
						}

						digit[0] = *str_p ;

						for( count = 1 ; count < DIGIT_STR_LEN(int) ;
							count++)
						{
							if( ! inc_str_in_esc_seq(
									vt100_parser->screen ,
									&str_p , &left , 0))
							{
								return  0 ;
							}

							if( *str_p < '0' || '9' < *str_p)
							{
								break ;
							}

							digit[count] = *str_p ;
						}

						digit[count] = '\0' ;
						if( num < MAX_NUM_OF_PS)
						{
							ps[num ++] = atoi( digit) ;
						#ifdef  MAX_PS_DIGIT
							if( ps[num - 1] > MAX_PS_DIGIT)
							{
								ps[num - 1] = MAX_PS_DIGIT ;
							}
						#endif
						}
					}

					if( *str_p < 0x30 || 0x3f < *str_p)
					{
						/* Is not a parameter character(0x30-0x3f). */

						/*
						 * "CSI 0 n" is *not* regarded as "CSI 0 ; -1 n"
						 * "CSI n" is regarded as "CSI -1 n"
						 * "CSI 0 ; n" is regarded as "CSI 0 ; -1 n"
						 * "CSI ; n" which has been already regarded as
						 * "CSI -1 ; n" is regarded as "CSI -1 ; -1 n"
						 */
						if( num == 0 ||
						    (*(str_p - 1) == ';' && num < MAX_NUM_OF_PS) )
						{
							ps[num ++] = -1 ;
						}

						/* num is always greater than 0 */

						break ;
					}
				}

				if( ! inc_str_in_esc_seq( vt100_parser->screen ,
						&str_p , &left , 0))
				{
					return  0 ;
				}
			}

			/*
			 * Skip trailing paremeter(0x30-0x3f) and intermediate(0x20-0x2f)
			 * characters.
			 */
			while( 0x20 <= *str_p && *str_p <= 0x3f)
			{
				if( pre_ch == '\0')
				{
					pre_ch = *str_p ;
				}

				if( ! inc_str_in_esc_seq( vt100_parser->screen ,
						&str_p , &left , 0))
				{
					return  0 ;
				}
			}

			if( pre_ch == '?')
			{
				if( *str_p == 'h')
				{
					/* "CSI ? h" DEC Private Mode Set */
					int  count ;

					for( count = 0 ; count < num ; count++)
					{
						if( ps[count] == 1)
						{
							/* "CSI ? 1 h" */

							vt100_parser->is_app_cursor_keys = 1 ;
						}
					#ifdef  USE_VT52
						else if( ps[count] == 2)
						{
							/* "CSI ? 2 h" */
							vt100_parser->is_vt52_mode = 0 ;

							/*
							 * USASCII should be designated for G0-G3
							 * here, but it is temporized by using
							 * ml_init_encoding_parser() etc for now.
							 */

							ml_init_encoding_parser( vt100_parser) ;
						}
					#endif
						else if( ps[count] == 3)
						{
							/* "CSI ? 3 h" */

							/* XTERM compatibility [#1048321] */
							clear_display_all( vt100_parser) ;

							resize( vt100_parser , 132 , 0 , 1) ;
						}
					#if  0
						else if( ps[count] == 4)
						{
							/* "CSI ? 4 h" smooth scrolling */
						}
					#endif
						else if( ps[count] == 5)
						{
							/* "CSI ? 5 h" */

							reverse_video( vt100_parser , 1) ;
						}
						else if( ps[count] == 6)
						{
							/* "CSI ? 6 h" relative origins */

							ml_screen_set_relative_origin(
								vt100_parser->screen) ;
							/*
							 * cursor position is reset
							 * (the same behavior of xterm 4.0.3,
							 * kterm 6.2.0 or so)
							 */
							ml_screen_goto( vt100_parser->screen ,
								0 , 0) ;
						}
						else if( ps[count] == 7)
						{
							/* "CSI ? 7 h" auto wrap */

							ml_screen_set_auto_wrap(
								vt100_parser->screen , 1) ;
						}
					#if  0
						else if( ps[count] == 8)
						{
							/* "CSI ? 8 h" auto repeat */
						}
					#endif
					#if  0
						else if( ps[count] == 9)
						{
							/* "CSI ? 9 h" X10 mouse reporting */
						}
					#endif
						else if( ps[count] == 25)
						{
							/* "CSI ? 25 h" */
							ml_screen_cursor_visible(
								vt100_parser->screen) ;
						}
					#if  0
						else if( ps[count] == 35)
						{
							/* "CSI ? 35 h" shift keys */
						}
					#endif
					#if  0
						else if( ps[count] == 40)
						{
							/* "CSI ? 40 h" 80 <-> 132 */
						}
					#endif
						else if( ps[count] == 47)
						{
							/*
							 * "CSI ? 47 h"
							 * Use alternate screen buffer
							 */

							if( use_alt_buffer)
							{
								ml_screen_use_alternative_edit(
									vt100_parser->screen) ;
							}
						}
						else if( ps[count] == 66)
						{
							/* "CSI ? 66 h" application key pad */
							vt100_parser->is_app_keypad = 1 ;
						}
					#if  0
						else if( ps[count] == 67)
						{
							/* "CSI ? 67 h" have back space */
						}
					#endif
						else if( ps[count] == 1000)
						{
							/* "CSI ? 1000 h" */

							set_mouse_report( vt100_parser ,
								MOUSE_REPORT) ;
						}
					#if  0
						else if( ps[count] == 1001)
						{
							/* "CSI ? 1001 h" X11 mouse highlighting */
						}
					#endif
						else if( ps[count] == 1002)
						{
							/* "CSI ? 1002 h" */

							set_mouse_report( vt100_parser ,
								BUTTON_EVENT_MOUSE_REPORT) ;
						}
						else if( ps[count] == 1003)
						{
							/* "CSI ? 1003 h" */

							set_mouse_report( vt100_parser ,
								ANY_EVENT_MOUSE_REPORT) ;
						}
						else if( ps[count] == 1004)
						{
							/* "CSI ? 1004 h" */

							vt100_parser->want_focus_event = 1 ;
						}
						else if( ps[count] == 1005)
						{
							/* "CSI ? 1005 h" */

							vt100_parser->ext_mouse_mode =
								EXTENDED_MOUSE_REPORT_UTF8 ;
						}
						else if( ps[count] == 1006)
						{
							/* "CSI ? 1006 h" */

							vt100_parser->ext_mouse_mode =
								EXTENDED_MOUSE_REPORT_SGR ;
						}
						else if( ps[count] == 1015)
						{
							/* "CSI ? 1015 h" */

							vt100_parser->ext_mouse_mode =
								EXTENDED_MOUSE_REPORT_URXVT ;
						}
					#if  0
						else if( ps[count] == 1010)
						{
							/*
							 * "CSI ? 1010 h"
							 * scroll to bottom on tty output inhibit
							 */
						}
					#endif
					#if  0
						else if( ps[count] == 1011)
						{
							/*
							 * "CSI ? 1011 h"
							 * "scroll to bottom on key press
							 */
						}
					#endif
						else if( ps[count] == 1047)
						{
							/* "CSI ? 1047 h" */

							if( use_alt_buffer)
							{
								ml_screen_use_alternative_edit(
									vt100_parser->screen) ;
							}
						}
						else if( ps[count] == 1048)
						{
							/* "CSI ? 1048 h" */

							if( use_alt_buffer)
							{
								save_cursor( vt100_parser) ;
							}
						}
						else if( ps[count] == 1049)
						{
							/* "CSI ? 1049 h" */

							if( use_alt_buffer)
							{
								save_cursor( vt100_parser) ;
								ml_screen_use_alternative_edit(
									vt100_parser->screen) ;
								clear_display_all( vt100_parser) ;
							}
						}
						else if( ps[count] == 2004)
						{
							/* "CSI ? 2004 h" */

							vt100_parser->is_bracketed_paste_mode = 1 ;
						}
						else if( ps[count] == 7727)
						{
							/* "CSI ? 7727 h" */

							vt100_parser->is_app_escape = 1 ;
						}
						else if( ps[count] == 8428)
						{
							/* "CSI ? 8428 h" (RLogin original) */

							vt100_parser->col_size_of_width_a = 1 ;
						}
						else if( ps[count] == 9500)
						{
							/* "CSI ? 9500 h" */

							config_protocol_set_simple( vt100_parser ,
								"use_local_echo" , "true") ;
						}
					#ifdef  DEBUG
						else
						{
							debug_print_unknown( "ESC [ ? %d h\n" ,
								ps[count]) ;
						}
					#endif
					}
				}
				else if( *str_p == 'l')
				{
					/* DEC Private Mode Reset */
					int  count ;

					for( count = 0 ; count < num ; count++)
					{
						if( ps[count] == 1)
						{
							/* "CSI ? 1 l" */

							vt100_parser->is_app_cursor_keys = 0 ;
						}
					#ifdef  USE_VT52
						else if( ps[count] == 2)
						{
							/* "CSI ? 2 l" */
							vt100_parser->is_vt52_mode = 1 ;
						}
					#endif
						else if( ps[count] == 3)
						{
							/* "CSI ? 3 l" */

							/* XTERM compatibility [#1048321] */
							clear_display_all( vt100_parser) ;

							resize( vt100_parser , 80 , 0 , 1) ;
						}
					#if  0
						else if( ps[count] == 4)
						{
							/* "CSI ? 4 l" smooth scrolling */
						}
					#endif
						else if( ps[count] == 5)
						{
							/* "CSI ? 5 l" */

							reverse_video( vt100_parser , 0) ;
						}
						else if( ps[count] == 6)
						{
							/* "CSI ? 6 l" absolute origins */

							ml_screen_set_absolute_origin(
								vt100_parser->screen) ;
							/*
							 * cursor position is reset
							 * (the same behavior of xterm 4.0.3,
							 * kterm 6.2.0 or so)
							 */
							ml_screen_goto( vt100_parser->screen ,
								0 , 0) ;
						}
						else if( ps[count] == 7)
						{
							/* "CSI ? 7 l" auto wrap */

							ml_screen_set_auto_wrap(
								vt100_parser->screen , 0) ;
						}
					#if  0
						else if( ps[count] == 8)
						{
							/* "CSI ? 8 l" auto repeat */
						}
					#endif
					#if  0
						else if( ps[count] == 9)
						{
							/* "CSI ? 9 l" X10 mouse reporting */
						}
					#endif
						else if( ps[count] == 25)
						{
							/* "CSI ? 25 l" */

							ml_screen_cursor_invisible(
								vt100_parser->screen) ;
						}
					#if  0
						else if( ps[count] == 35)
						{
							/* "CSI ? 35 l" shift keys */
						}
					#endif
					#if  0
						else if( ps[count] == 40)
						{
							/* "CSI ? 40 l" 80 <-> 132 */
						}
					#endif
						else if( ps[count] == 47)
						{
							/* "CSI ? 47 l" Use normal screen buffer */

							if( use_alt_buffer)
							{
								ml_screen_use_normal_edit(
									vt100_parser->screen) ;
							}
						}
						else if( ps[count] == 66)
						{
							/* "CSI ? 66 l" application key pad */

							vt100_parser->is_app_keypad = 0 ;
						}
					#if  0
						else if( ps[count] == 67)
						{
							/* "CSI ? 67 l" have back space */
						}
					#endif
						else if( ps[count] == 1000 ||
							ps[count] == 1002 || ps[count] == 1003)
						{
							/*
							 * "CSI ? 1000 l" "CSI ? 1002 l"
							 * "CSI ? 1003 l"
							 */

							set_mouse_report( vt100_parser , 0) ;
						}
					#if  0
						else if( ps[count] == 1001)
						{
							/* "CSI ? 1001 l" X11 mouse highlighting */
						}
					#endif
						else if( ps[count] == 1004)
						{
							/* "CSI ? 1004 h" */

							vt100_parser->want_focus_event = 0 ;
						}
						else if( ps[count] == 1005 ||
						         ps[count] == 1006 ||
							 ps[count] == 1015)
						{
							/*
							 * "CSI ? 1005 l"
							 * "CSI ? 1006 l"
							 * "CSI ? 1015 l"
							 */
							vt100_parser->ext_mouse_mode = 0 ;
						}
					#if  0
						else if( ps[count] == 1010)
						{
							/*
							 * "CSI ? 1010 l"
							 * scroll to bottom on tty output inhibit
							 */
						}
					#endif
					#if  0
						else if( ps[count] == 1011)
						{
							/*
							 * "CSI ? 1011 l"
							 * scroll to bottom on key press
							 */
						}
					#endif
						else if( ps[count] == 1047)
						{
							/* "CSI ? 1047 l" */

							if( use_alt_buffer)
							{
								clear_display_all( vt100_parser) ;
								ml_screen_use_normal_edit(
									vt100_parser->screen) ;
							}
						}
						else if( ps[count] == 1048)
						{
							/* "CSI ? 1048 l" */

							if( use_alt_buffer)
							{
								restore_cursor( vt100_parser) ;
							}
						}
						else if( ps[count] == 1049)
						{
							/* "CSI ? 1049 l" */

							if( use_alt_buffer)
							{
								ml_screen_use_normal_edit(
									vt100_parser->screen) ;
								restore_cursor( vt100_parser) ;
							}
						}
						else if( ps[count] == 2004)
						{
							/* "CSI ? 2004 l" */

							vt100_parser->is_bracketed_paste_mode = 0 ;
						}
						else if( ps[count] == 7727)
						{
							/* "CSI ? 7727 l" */

							vt100_parser->is_app_escape = 0 ;
						}
						else if( ps[count] == 8428)
						{
							/* "CSI ? 8428 l" (RLogin original) */

							vt100_parser->col_size_of_width_a = 2 ;
						}
						else if( ps[count] == 9500)
						{
							/* "CSI ? 9500 l" */

							config_protocol_set_simple( vt100_parser ,
								"use_local_echo" , "false") ;
						}
					#ifdef  DEBUG
						else
						{
							debug_print_unknown( "ESC [ ? %d l\n" ,
								ps[count]) ;
						}
					#endif
					}
				}
				else if( *str_p == 'n')
				{
					if( ps[0] == 8840)
					{
						/* "CSI ? 8840 n" (TNREPTAMB) */

						char  msg[] = "\x1b[?884Xn" ;

						msg[6] = vt100_parser->col_size_of_width_a + 0x30 ;
						ml_write_to_pty( vt100_parser->pty , msg ,
							sizeof(msg) - 1) ;
					}
				}
			#if  0
				else if( *str_p == 'r')
				{
					/* "CSI ? r"  Restore DEC Private Mode */
				}
			#endif
			#if  0
				else if( *str_p == 's')
				{
					/* "CSI ? s" Save DEC Private Mode */
				}
			#endif
				/* Other final character */
				else if( 0x40 <= *str_p && *str_p <= 0x7e)
				{
				#ifdef  DEBUG
					debug_print_unknown( "ESC [ %c\n" , *str_p) ;
				#endif
				}
				else
				{
					/* not VT100 control sequence. */

				#ifdef  ESCSEQ_DEBUG
					kik_msg_printf( "=> not VT100 control sequence.\n") ;
				#endif

					return  1 ;
				}
			}
			else if( pre_ch == '!')
			{
				if( *str_p == 'p')
				{
					/* "CSI ! p" Soft terminal reset */

					soft_reset( vt100_parser) ;
				}
			}
			else if( pre_ch == '$')
			{
				if( num >= 4)
				{
					int  count ;

					/* Set the default values. */
					for( count = 0 ; count < num ; count++)
					{
						if( ps[count] <= 0)
						{
							if( count == 2)
							{
								ps[count] = ml_screen_get_rows(
								            vt100_parser->screen) ;
							}
							else if( count == 3)
							{
								ps[count] = ml_screen_get_cols(
								            vt100_parser->screen) ;
							}
							else
							{
								ps[count] = 1 ;
							}
						}
					}

					if( ps[3] >= ps[1] && ps[2] >= ps[0])
					{
						if( *str_p == 'z')
						{
							ml_screen_erase_area(
								vt100_parser->screen ,
								ps[1] - 1 , ps[0] - 1 ,
								ps[3] - ps[1] + 1 ,
								ps[2] - ps[0] + 1) ;
						}
						else if( *str_p == 'v' && num == 8)
						{
							/* "CSI ... $ v" DECCRA */

							ml_screen_copy_area(
								vt100_parser->screen ,
								ps[1] - 1 , ps[0] - 1 ,
								ps[3] - ps[1] + 1 ,
								ps[2] - ps[0] + 1 ,
								ps[6] - 1 , ps[5] - 1) ;
						}
					}
				}
			}
			else if( pre_ch == '<')
			{
				/* Teraterm compatible IME control sequence */

				if( *str_p == 'r')
				{
					/* Restore IME state */
					if( vt100_parser->im_is_active !=
						im_is_active( vt100_parser))
					{
						switch_im_mode( vt100_parser) ;
					}
				}
				else if( *str_p == 's')
				{
					/* Save IME state */

					vt100_parser->im_is_active = im_is_active( vt100_parser) ;
				}
				else if( *str_p == 't')
				{
					/* ps[0] = 0 (Close), ps[0] = 1 (Open) */

					if( ps[0] != im_is_active( vt100_parser))
					{
						switch_im_mode( vt100_parser) ;
					}
				}
			}
			else if( pre_ch == '>')
			{
				if( *str_p == 'c')
				{
					/* "CSI > c" Secondary DA */

					send_device_attributes( vt100_parser->pty , 2) ;
				}
				else if( *str_p == 'm')
				{
					/* "CSI > m" */

					if( ps[0] == -1)
					{
						/* reset to initial value. */
						set_modkey_mode( vt100_parser , 1 , 2) ;
						set_modkey_mode( vt100_parser , 2 , 2) ;
						set_modkey_mode( vt100_parser , 4 , 0) ;
					}
					else
					{
						if( num == 1 || ps[1] == -1)
						{
							if( ps[0] == 1 || /* modifyCursorKeys */
							    ps[0] == 2)   /* modifyFunctionKeys */
							{
								/* The default is 2. */
								ps[1] = 2 ;
							}
							else /* if( ps[0] == 4) */
							{
								/*
								 * modifyOtherKeys
								 * The default is 0.
								 */
								ps[1] = 0 ;
							}
						}
						
						set_modkey_mode( vt100_parser , ps[0] , ps[1]) ;
					}
				}
				else if( *str_p == 'n')
				{
					/* "CSI > n" */

					if( num == 1)
					{
						if( ps[0] == -1)
						{
							ps[0] = 2 ;
						}

						/* -1: don't send modifier key code. */
						set_modkey_mode( vt100_parser , ps[0] , -1) ;
					}
				}
				else
				{
					/*
					 * "CSI > T", "CSI > c", "CSI > p", "CSI > t"
					 */
				}
			}
			else if( pre_ch == ' ')
			{
				if( *str_p == 'q')
				{
					if( ps[0] < 2)
					{
						config_protocol_set_simple( vt100_parser ,
							"blink_cursor" , "true") ;
					}
					else if( ps[0] == 2)
					{
						config_protocol_set_simple( vt100_parser ,
							"blink_cursor" , "false") ;
					}
				}
				else
				{
					/*
					 * "CSI SP t"(DECSWBV), "CSI SP u"(DECSMBV)
					 */
				}
			}
			/* Other pre_ch(0x20-0x2f or 0x3a-0x3f) */
			else if( pre_ch)
			{
				/*
				 * "CSI " p"(DECSCL), "CSI " q"(DECSCA)
				 * "CSI ' {"(DECSLE), "CSI ' |"(DECRQLP)
				 * etc
				 */

			#ifdef  DEBUG
				debug_print_unknown( "ESC [ %c %c\n" , pre_ch , *str_p) ;
			#endif
			}
			else if( *str_p == '@')
			{
				/* "CSI @" insert blank chars */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				/* inserting ps[0] blank characters. */
				ml_screen_insert_blank_chars( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'A' || *str_p == 'k')
			{
				/* "CSI A" = CUU , "CSI k" = VPB */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_upward( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'B' || *str_p == 'e')
			{
				/* "CSI B" = CUD , "CSI e" = VPR */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_downward( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'C' || *str_p == 'a')
			{
				/* "CSI C" = CUF , "CSI a" = HPR */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_forward( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'D' || *str_p == 'j')
			{
				/* "CSI D" = CUB , "CSI j" = HPB */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_back( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'E')
			{
				/* "CSI E" down and goto first column */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_downward( vt100_parser->screen , ps[0]) ;
				ml_screen_goto_beg_of_line( vt100_parser->screen) ;
			}
			else if( *str_p == 'F')
			{
				/* "CSI F" up and goto first column */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_upward( vt100_parser->screen , ps[0]) ;
				ml_screen_goto_beg_of_line( vt100_parser->screen) ;
			}
			else if( *str_p == 'G' || *str_p == '`')
			{
				/*
				 * "CSI G"(CHA) "CSI `"(HPA)
				 * cursor position absolute.
				 */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_horizontally( vt100_parser->screen , ps[0] - 1) ;
			}
			else if( *str_p == 'H' || *str_p == 'f')
			{
				/* "CSI H" "CSI f" */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				if( num <= 1 || ps[1] <= 0)
				{
					ps[1] = 1 ;
				}

				ml_screen_goto( vt100_parser->screen , ps[1] - 1 , ps[0] - 1) ;
			}
			else if( *str_p == 'I')
			{
				/* "CSI I" cursor forward tabulation (CHT) */

				if( ps[0] == -1)
				{
					/*
					 * "CSI 0 I" => No tabulation.
					 * "CSI I" => 1 taburation.
					 */
					ps[0] = 1 ;
				}

				ml_screen_vertical_forward_tabs( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'J')
			{
				/* "CSI J" Erase in Display */

				if( ps[0] <= 0)
				{
					ml_screen_clear_below( vt100_parser->screen) ;
				}
				else if( ps[0] == 1)
				{
					ml_screen_clear_above( vt100_parser->screen) ;
				}
				else if( ps[0] == 2)
				{
					clear_display_all( vt100_parser) ;
				}
			}
			else if( *str_p == 'K')
			{
				/* "CSI K" Erase in Line */

				if( ps[0] <= 0)
				{
					ml_screen_clear_line_to_right( vt100_parser->screen) ;
				}
				else if( ps[0] == 1)
				{
					ml_screen_clear_line_to_left( vt100_parser->screen) ;
				}
				else if( ps[0] == 2)
				{
					clear_line_all( vt100_parser) ;
				}
			}
			else if( *str_p == 'L')
			{
				/* "CSI L" */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_insert_new_lines( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'M')
			{
				/* "CSI M" */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_delete_lines( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'P')
			{
				/* "CSI P" delete chars */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_delete_cols( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'S')
			{
				/* "CSI S" scroll up */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_scroll_upward( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'T')
			{
				/* "CSI T" scroll down */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_scroll_downward( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'X')
			{
				/* "CSI X" erase characters */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_clear_cols( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'Z')
			{
				/* "CSI Z" cursor backward tabulation (CBT) */

				if( ps[0] == -1)
				{
					/*
					 * "CSI 0 Z" => No tabulation.
					 * "CSI Z" => 1 taburation.
					 */
					ps[0] = 1 ;
				}

				ml_screen_vertical_backward_tabs( vt100_parser->screen , ps[0]) ;
			}
			else if( *str_p == 'b')
			{
				/* "CSI b" repeat the preceding graphic character(REP) */

				if( vt100_parser->w_buf.last_ch)
				{
					int  count ;

					if( ps[0] <= 0)
					{
						ps[0] = 1 ;
					}

					for( count = 0 ; count < ps[0] ; count++)
					{
						(*vt100_parser->w_buf.output_func)(
							vt100_parser->screen ,
							vt100_parser->w_buf.last_ch , 1) ;
					}

					vt100_parser->w_buf.last_ch = NULL ;
				}
			}
			else if( *str_p == 'c')
			{
				/* "CSI c" Primary DA */

				send_device_attributes( vt100_parser->pty , 1) ;
			}
			else if( *str_p == 'd')
			{
				/* "CSI d" line position absolute(VPA) */

				if( ps[0] <= 0)
				{
					ps[0] = 1 ;
				}

				ml_screen_go_vertically( vt100_parser->screen , ps[0] - 1) ;
			}
			else if( *str_p == 'g')
			{
				/* "CSI g" tab clear */

				if( ps[0] <= 0)
				{
					ml_screen_clear_tab_stop( vt100_parser->screen) ;
				}
				else if( ps[0] == 3)
				{
					ml_screen_clear_all_tab_stops( vt100_parser->screen) ;
				}
			}
			else if( *str_p == 'l')
			{
				/* "CSI l" */
				int  count ;

				for( count = 0 ; count < num ; count++)
				{
					if( ps[count] == 4)
					{
						/* replace mode */

						vt100_parser->w_buf.output_func =
							ml_screen_overwrite_chars ;
					}
				}
			}
			else if( *str_p == 'h')
			{
				/* "CSI h" */
				int  count ;

				for( count = 0 ; count < num ; count++)
				{
					if( ps[count] == 4)
					{
						/* insert mode */

						vt100_parser->w_buf.output_func =
							ml_screen_insert_chars ;
					}
				#ifdef  DEBUG
					else
					{
						debug_print_unknown( "ESC [ %d h\n" ,
							ps[count]) ;
					}
				#endif
				}
			}
			else if( *str_p == 'm')
			{
				/* "CSI m" */
				int  count ;

				for( count = 0 ; count < num ; )
				{
					int  proceed ;

					if( ( proceed = change_char_fine_color( vt100_parser ,
								ps + count , num - count)))
					{
						count += proceed ;
					}
					else
					{
						if( ps[count] <= 0)
						{
							ps[count] = 0 ;
						}

						change_char_attr( vt100_parser , ps[count++]) ;
					}
				}
			}
			else if( *str_p == 'n')
			{
				/* "CSI n" device status report */

				if( ps[0] == 5)
				{
					ml_write_to_pty( vt100_parser->pty , "\x1b[0n" , 4) ;
				}
				else if( ps[0] == 6)
				{
					char  seq[4 + DIGIT_STR_LEN(u_int) * 2 + 1] ;

					sprintf( seq , "\x1b[%d;%dR" ,
						ml_screen_cursor_row( vt100_parser->screen) + 1 ,
						ml_screen_cursor_col( vt100_parser->screen) + 1) ;

					ml_write_to_pty( vt100_parser->pty , seq , strlen( seq)) ;
				}
			}
			else if( *str_p == 'r')
			{
				/* "CSI r" set scroll region */

				if( ps[0] <= 0)
				{
					ps[0] = 0 ;
				}

				if( num <= 1 || ps[1] <= 0)
				{
					ps[1] = 0 ;
				}

				if( ml_screen_set_scroll_region( vt100_parser->screen ,
						ps[0] - 1 , ps[1] - 1))
				{
					ml_screen_goto( vt100_parser->screen , 0 , 0) ;
				}
			}
			else if( *str_p == 's')
			{
				/* "CSI s" */

				save_cursor( vt100_parser) ;
			}
			else if( *str_p == 't')
			{
				/* "CSI t" */

				if( num == 3)
				{
					if( ps[0] == 4)
					{
						resize( vt100_parser , ps[2] , ps[1] , 0) ;
					}
					else if( ps[0] == 8)
					{
						resize( vt100_parser , ps[2] , ps[1] , 1) ;
					}
				}
				else if( num == 2)
				{
					if( ps[0] == 22)
					{
						if( ps[1] == 0 || ps[1] == 1)
						{
							push_to_saved_names(
								&vt100_parser->saved_icon_names ,
								vt100_parser->icon_name) ;
						}

						if( ps[1] == 0 || ps[1] == 2)
						{
							push_to_saved_names(
								&vt100_parser->saved_win_names ,
								vt100_parser->win_name) ;
						}
					}
					else if( ps[0] == 23)
					{
						if( ( ps[1] == 0 || ps[1] == 1) &&
						    vt100_parser->saved_icon_names.num > 0)
						{
							set_icon_name( vt100_parser ,
								pop_from_saved_names(
								&vt100_parser->saved_icon_names)) ;
						}

						if( ( ps[1] == 0 || ps[1] == 2) &&
						    vt100_parser->saved_win_names.num > 0)
						{
							set_window_name( vt100_parser ,
								pop_from_saved_names(
								&vt100_parser->saved_win_names)) ;
						}
					}
				}
				else
				{
					if( ps[0] == 14)
					{
						report_window_size( vt100_parser , 0) ;
					}
					else if( ps[0] == 18)
					{
						report_window_size( vt100_parser , 1) ;
					}
				}
			}
			else if( *str_p == 'u')
			{
				/* "CSI u" */

				restore_cursor( vt100_parser) ;
			}
			else if( *str_p == 'x')
			{
				/* "CSI x" request terminal parameters */

				/* XXX the same as rxvt */

				if( ps[0] <= 0)
				{
					ps[0] = 0 ;
				}

				if( ps[0] == 0 || ps[0] == 1)
				{
					char seq[] = "\x1b[X;1;1;112;112;1;0x" ;

					/* '+ 0x30' lets int to char */
					seq[2] = ps[0] + 2 + 0x30 ;

					ml_write_to_pty( vt100_parser->pty ,
						seq , sizeof( seq) - 1) ;
				}
			}
		#if  0
			else if( *str_p == '^')
			{
				/* "CSI ^" initiate hilite mouse tracking. */
			}
		#endif
			/* Other final character */
			else if( 0x40 <= *str_p && *str_p <= 0x7e)
			{
			#ifdef  DEBUG
				debug_print_unknown( "ESC [ %c\n" , *str_p) ;
			#endif
			}
			else
			{
				/* not VT100 control sequence. */

			#ifdef  ESCSEQ_DEBUG
				kik_msg_printf( "=> not VT100 control sequence.\n") ;
			#endif

				return  1 ;
			}
		}
		else if( *str_p == ']')
		{
			/* "ESC ]" (OSC) */

			char  digit[DIGIT_STR_LEN(int) + 1] ;
			int  count ;
			int  ps ;
			u_char *  pt ;

			if( ! inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0))
			{
				return  0 ;
			}

			for( count = 0 ; count < DIGIT_STR_LEN(int) ; count++)
			{
				if( '0' <= *str_p && *str_p <= '9')
				{
					digit[count] = *str_p ;

					if( ! inc_str_in_esc_seq( vt100_parser->screen ,
							&str_p , &left , 0))
					{
						return  0 ;
					}
				}
				else
				{
					break ;
				}
			}

			if( count > 0 && *str_p == ';')
			{
				digit[count] = '\0' ;

				/* if digit is illegal , ps is set 0. */
				ps = atoi( digit) ;
			#ifdef  MAX_PS_DIGIT
				if( ps > MAX_PS_DIGIT)
				{
					ps = MAX_PS_DIGIT ;
				}
			#endif

				if( ! inc_str_in_esc_seq( vt100_parser->screen ,
						&str_p , &left , 1))
				{
					return  0 ;
				}
			}
			else
			{
				/* Illegal OSC format */
				ps = -1 ;
			}

			if( ! ( pt = get_pt_in_esc_seq( &str_p , &left , 1)))
			{
				if( left == 0)
				{
					return  0 ;
				}
			#ifdef  DEBUG
				else
				{
					debug_print_unknown( "ESC ] %d ; ???\n" , ps) ;
				}
			#endif
			}
			else if( ps == 0)
			{
				/* "OSC 0" change icon name and window title */

				if( *pt != '\0')
				{
					set_window_name( vt100_parser , strdup( pt)) ;
					set_icon_name( vt100_parser , strdup( pt)) ;
				}
			}
			else if( ps == 1)
			{
				/* "OSC 1" change icon name */

				if( *pt != '\0')
				{
					set_icon_name( vt100_parser , strdup( pt)) ;
				}
			}
			else if( ps == 2)
			{
				/* "OSC 2" change window title */

				if( *pt != '\0')
				{
					set_window_name( vt100_parser , strdup( pt)) ;
				}
			}
			else if( ps == 4)
			{
				/* "OSC 4" change 256 color */

				change_color_rgb( vt100_parser , pt) ;
			}
			else if( ps == 5)
			{
				/* "OSC 5" change colorBD/colorUL */

				change_special_color( vt100_parser , pt) ;
			}
			else if( ps == 10)
			{
				/* "OSC 10" fg color */

				if( strcmp( pt , "?") == 0)	/* ?:query rgb */
				{
					get_rgb( vt100_parser , ps , ML_FG_COLOR) ;
				}
				else
				{
					config_protocol_set_simple( vt100_parser ,
						"fg_color" , pt) ;
				}
			}
			else if( ps == 11)
			{
				/* "OSC 11" bg color */

				if( strcmp( pt , "?") == 0)	/* ?:query rgb */
				{
					get_rgb( vt100_parser , ps , ML_BG_COLOR) ;
				}
				else
				{
					config_protocol_set_simple( vt100_parser ,
						"bg_color" , pt) ;
				}
			}
			else if( ps == 12)
			{
				/* "OSC 12" cursor bg color */

				if( strcmp( pt , "?") != 0)	/* ?:query rgb */
				{
					config_protocol_set_simple( vt100_parser ,
						"cursor_bg_color" , pt) ;
				}
			}
			else if( ps == 20)
			{
				/* "OSC 20" (Eterm compatible) */

				/* edit commands */
				char *  p ;

				/* XXX discard all adjust./op. settings.*/
				/* XXX may break multi-byte char string. */
				if( ( p = strchr( pt , ';')))
				{
					*p = '\0';
				}
				if( ( p = strchr( pt , ':')))
				{
					*p = '\0';
				}

				if( *pt == '\0')
				{
					/*
					 * Do not change current edit but
					 * alter diaplay setting.
					 * XXX nothing can be done for now.
					 */

					return  0 ;
				}

				config_protocol_set_simple( vt100_parser , "wall_picture" , pt) ;
			}
		#if  0
			else if( ps == 46)
			{
				/* "OSC 46" change log file */
			}
			else if( ps == 50)
			{
				/* "OSC 50" set font */
			}
		#endif
			else if( ps == 52)
			{
				u_char *  p ;

				if( ( p = strchr( pt , ';')))
				{
					pt = p + 1 ;
				}

				set_selection( vt100_parser , pt) ;
			}
			else if( ps == 5379)
			{
				/* "OSC 5379" set */

				config_protocol_set( vt100_parser , pt , 0) ;
			}
			else if( ps == 5380)
			{
				/* "OSC 5380" get */

				config_protocol_get( vt100_parser , pt , 0) ;
			}
			else if( ps == 5381)
			{
				/* "OSC 5381" get(menu) */

				config_protocol_get( vt100_parser , pt , 1) ;
			}
			else if( ps == 5383)
			{
				/* "OSC 5383" set&save */

				config_protocol_set( vt100_parser , pt , 1) ;
			}
		#ifdef  DEBUG
			else if( ps == -1)
			{
				debug_print_unknown( "ESC ] %s\n" , pt) ;
			}
			else
			{
				debug_print_unknown( "ESC ] %d ; %s\n" , ps , pt) ;
			}
		#endif
		}
		else if( *str_p == 'P')
		{
			/* "ESC P" DCS */

			u_char *  dcs_beg ;
		#ifdef  ENABLE_SIXEL
			char *  dir_path ;
		#endif

			while(1)
			{
				/* ESC P ... */
				dcs_beg = str_p - 1 ;
				break ;

		#ifdef  ENABLE_SIXEL
			parse_dcs:
				/* 0x90 ... */
				dcs_beg = str_p ;
				break ;
		#endif
			}

			do
			{
				if( ! increment_str( &str_p , &left))
				{
					return  0 ;
				}
			}
			while( *str_p == ';' || ('0' <= *str_p && *str_p <= '9')) ;

		#ifdef  ENABLE_SIXEL
			if( ( *str_p == 'q' /* sixel */
			    /* || *str_p == 'p' */ ) &&		/* ReGis */
			    ( dir_path = kik_get_user_rc_path( "mlterm/")) )
			{
				char *  dev ;
				char *  file_path ;
				int  is_end ;
				FILE *  fp ;

				dev = ml_pty_get_slave_name( vt100_parser->pty) + 5 ;
				file_path = alloca( strlen( dir_path) + strlen( dev) + 5) ;
				sprintf( file_path , "%s%s.six" , dir_path , dev) ;
				str_replace( file_path + strlen( dir_path) , '/' , '_') ;
				free( dir_path) ;

				if( left > 2 && *(str_p + 1) == '\0')
				{
					fp = fopen( file_path , "a") ;
					is_end = *(str_p + 2) ;
					dcs_beg = (str_p += 3) ;
					left -= 3 ;
				}
				else
				{
					fp = fopen( file_path , "w") ;
					is_end = 0 ;
				}

				while( 1)
				{
					if( ! increment_str( &str_p , &left))
					{
						fwrite( dcs_beg , 1 , str_p - dcs_beg + 1 , fp) ;

						vt100_parser->r_buf.left = 0 ;
						if( ! receive_bytes( vt100_parser))
						{
							fclose( fp) ;

							memcpy( vt100_parser->r_buf.chars ,
								"\x1bPq\0" , 4) ;
							vt100_parser->r_buf.chars[4] = is_end ;
							vt100_parser->r_buf.filled_len =
								vt100_parser->r_buf.left = 5 ;

							return  0 ;
						}

						dcs_beg = str_p = CURRENT_STR_P(vt100_parser) ;
						left = vt100_parser->r_buf.left ;
					}

					if( is_end == 2)
					{
						if( /* ^L (FF) */
						    *str_p == 0x0c ||
						    /* ^] (GS), ^^ (RS), ^_ (US) */
						    ( 0x1d <= *str_p && *str_p <= 0x1f) ||
						    *str_p == 0x90 ||
						    /* XXX If left == 0 and next char is 'P'... */
						    ( *str_p == CTL_ESC && left > 1 &&
						      *(str_p + 1) == 'P') )
						{
							/* continued ... */
							is_end = 0 ;
						}
						else
						{
							str_p -- ;
							left ++ ;
							break ;
						}
					}
					/*
					 * 0x9c is regarded as ST here, because sixel sequence
					 * unuses it certainly.
					 */
					else if( *str_p == 0x9c)
					{
						is_end = 2 ;
					}
					else if( *str_p == CTL_ESC)
					{
						is_end = 1 ;
					}
					else if( is_end == 1)
					{
						if( *str_p == '\\')
						{
							is_end = 2 ;
						}
						else
						{
							is_end = 0 ;
						}
					}
				}

				fwrite( dcs_beg , 1 , str_p - dcs_beg + 1 , fp) ;
				fclose( fp) ;
				show_picture( vt100_parser , file_path , 0 , 0 , 0 , 0 , 0 , 0) ;
			}
			else
		#endif  /* ENABLE_SIXEL */
			if( *str_p == '{')
			{
				/* DECDLD */

				u_char *  pt ;
				ml_drcs_t *  font ;
				int  num ;
				u_char *  p ;
				int  ps[8] ;
				int  idx ;
				int  is_end ;
				u_int  width ;
				u_int  height ;

				while( 1)
				{
					if( *str_p == 0x9c ||
					    ( *str_p == CTL_ESC && left > 1 &&
					      *(str_p + 1) == '\\'))
					{
						*str_p = '\0' ;
						increment_str( &str_p , &left) ;
						break ;
					}
					else if( ! increment_str( &str_p , &left))
					{
						return  0 ;
					}
				}

				if( *dcs_beg == '\x1b')
				{
					pt = dcs_beg + 2 ;
				}
				else /* if( *dcs_beg == '\x90') */
				{
					pt = dcs_beg + 1 ;
				}

				for( num = 0 ; num < 8 ; num ++)
				{
					p = pt ;

					while( '0' <= *pt && *pt <= '9')
					{
						pt ++ ;
					}

					if( *pt == ';' || *pt == '{')
					{
						*(pt ++) = '\0' ;
					}
					else
					{
						break ;
					}

					ps[num] = *p ? atoi( p) : 0 ;
				}

				if( *pt == ' ')
				{
					/* ESC ( SP Ft */
					pt ++ ;
				}

				if( num == 8 &&
				    0x30 <= *pt && *pt <= 0x7e)
				{
					mkf_charset_t  cs ;

					if( ps[7] == 0)
					{
						idx = ps[1] == 0 ? 1 : ps[1] ;
						cs = CS94SB_ID(*pt) ;
					}
					else
					{
						idx = ps[1] ;
						cs = CS96SB_ID(*pt) ;
					}

					if( ps[2] == 0)
					{
						ml_drcs_final( cs) ;
					}

					font = ml_drcs_get( cs , 1) ;

					if( ps[3] <= 4 || ps[3] >= 255)
					{
						width = 15 ;
					}
					else
					{
						width = ps[3] ;
					}

					if( ps[6] == 0 || ps[6] >= 255)
					{
						height = 12 ;
					}
					else
					{
						height = ps[6] ;
					}

					while( 1)
					{
						p = ++pt ;

						while( *pt == '/' || ('?' <= *pt && *pt <= '~'))
						{
							pt ++ ;
						}

						if( *pt)
						{
							*pt = '\0' ;
							is_end = 0 ;
						}
						else
						{
							is_end = 1 ;
						}

						if( *p)
						{
							if( strlen(p) == (width + 1) *
								((height + 5) / 6) - 1)
							{
								ml_drcs_add( font , idx ,
									p , width , height) ;
							}
						#ifdef  DEBUG
							else
							{
								kik_debug_printf( KIK_DEBUG_TAG
								    "DRCS illegal size %s\n" ,
								    p) ;
							}
						#endif

							idx ++ ;
						}

						if( is_end)
						{
							break ;
						}
					}
				}
			}
			else if( ! get_pt_in_esc_seq( &str_p , &left , 0) && left == 0)
			{
				return  0 ;
			}
		}
		else if( *str_p == 'X' || *str_p == '^' || *str_p == '_')
		{
			/*
			 * "ESC X" SOS
			 * "ESC ^" PM
			 * "ESC _" APC
			 */
			if( ! inc_str_in_esc_seq( vt100_parser->screen , &str_p , &left , 0) ||
			    ( ! get_pt_in_esc_seq( &str_p , &left , 0) && left == 0))
			{
				return  0 ;
			}
		}
		/* Other final character */
		else if( 0x30 <= *str_p && *str_p <= 0x7e)
		{
		#ifdef  DEBUG
			debug_print_unknown( "ESC %c\n" , *str_p) ;
		#endif
		}
		/* intermediate character */
		else if( 0x20 <= *str_p && *str_p <= 0x2f)
		{
			/*
			 * ESC I.....I  F
			 * 033 040-057  060-176
			 */
			u_int  ic_num ;

			ic_num = 0 ;

		#ifdef  DEBUG
			debug_print_unknown( "ESC") ;
		#endif

			/* In case more than one intermediate(0x20-0x2f) chars. */
			do
			{
				ic_num ++ ;

			#ifdef  DEBUG
				kik_msg_printf( " %c" , *str_p) ;
			#endif

				if( ! inc_str_in_esc_seq( vt100_parser->screen ,
							&str_p , &left , 0))
				{
					return  0 ;
				}
			}
			while( 0x20 <= *str_p && *str_p <= 0x2f) ;

		#ifdef  DEBUG
			kik_msg_printf( " %c\n" , *str_p) ;
		#endif

			if( ic_num == 1 || ic_num == 2)
			{
				if( ic_num == 1 && *(str_p - 1) == '#')
				{
					if( *str_p == '8')
					{
						/* "ESC # 8" DEC screen alignment test */
						ml_screen_fill_all_with_e( vt100_parser->screen) ;
					}
				}
				else if( *(str_p - ic_num) == '(')
				{
					/* "ESC ("(Registered CS) or "ESC ( SP"(DRCS) */

					if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
					{
						/* ESC ( will be processed in mkf. */
						return  1 ;
					}

					vt100_parser->g0 = CS94SB_ID(*str_p) ;

					if( ! vt100_parser->is_so)
					{
						vt100_parser->gl = vt100_parser->g0 ;
					}
				}
				else if( *(str_p - ic_num) == ')')
				{
					/* "ESC )"(Registered CS) or "ESC ( SP"(DRCS) */

					if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
					{
						/* ESC ) will be processed in mkf. */
						return  1 ;
					}

					vt100_parser->g1 = CS94SB_ID(*str_p) ;

					if( vt100_parser->is_so)
					{
						vt100_parser->gl = vt100_parser->g1 ;
					}
				}
				else
				{
					/*
					 * "ESC SP F", "ESC SP G", "ESC SP L", "ESC SP M",
					 * "ESC SP N" etc ...
					 */
				}
			}
		}
		else
		{
			/* not VT100 control sequence. */

		#ifdef  ESCSEQ_DEBUG
			kik_msg_printf( "=> not VT100 control sequence.\n") ;
		#endif

			return  1 ;
		}

	#ifdef  ESCSEQ_DEBUG
		kik_msg_printf( "\n") ;
	#endif
	}
	else if( *str_p == CTL_SI)
	{
		if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
		{
			/* SI will be processed in mkf. */
			return  1 ;
		}

	#ifdef  ESCSEQ_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " receiving SI\n") ;
	#endif

		vt100_parser->gl = vt100_parser->g0 ;
		vt100_parser->is_so = 0 ;
	}
	else if( *str_p == CTL_SO)
	{
		if( IS_ENCODING_BASED_ON_ISO2022(vt100_parser->encoding))
		{
			/* SO will be processed in mkf. */
			return  1 ;
		}

	#ifdef  ESCSEQ_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " receiving SO\n") ;
	#endif

		vt100_parser->gl = vt100_parser->g1 ;
		vt100_parser->is_so = 1 ;
	}
	else if( CTL_LF <= *str_p && *str_p <= CTL_FF)
	{
	#ifdef  ESCSEQ_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " receiving LF\n") ;
	#endif

		ml_screen_line_feed( vt100_parser->screen) ;
	}
	else if( *str_p == CTL_CR)
	{
	#ifdef  ESCSEQ_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " receiving CR\n") ;
	#endif

		ml_screen_goto_beg_of_line( vt100_parser->screen) ;
	}
	else if( *str_p == CTL_TAB)
	{
	#ifdef  ESCSEQ_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " receiving TAB\n") ;
	#endif

		ml_screen_vertical_forward_tabs( vt100_parser->screen , 1) ;
	}
	else if( *str_p == CTL_BS)
	{
	#ifdef  ESCSEQ_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " receiving BS\n") ;
	#endif

		ml_screen_go_back( vt100_parser->screen , 1) ;
	}
	else if( *str_p == CTL_BEL)
	{
	#ifdef  ESCSEQ_DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " receiving BEL\n") ;
	#endif

		if( HAS_XTERM_LISTENER(vt100_parser,bel))
		{
			stop_vt100_cmd( vt100_parser , 0) ;
			(*vt100_parser->xterm_listener->bel)(
				vt100_parser->xterm_listener->self) ;
			/*
			 * XXX
			 * start_vt100_cmd( ... , *1*) erases cursor which
			 * xterm_listener::bell drew if bell mode is visual.
			 */
			start_vt100_cmd( vt100_parser , 1) ;
		}
	}
#ifdef  ENABLE_SIXEL
	else if( *str_p == 0x90)
	{
		/*
		 * DCS
		 * (0x90 is used by utf8 encoding for non control characters, so
		 * it is inappropriate to regard it as DCS simply.
		 * But it doesn't cause a seirous problem because mlterm parses
		 * received text by mkf for the first time.)
		 */

		 goto  parse_dcs ;
	}
#endif
	else
	{
		/* not VT100 control sequence */

		return  1 ;
	}

#ifdef  EDIT_DEBUG
	ml_edit_dump( vt100_parser->screen->edit) ;
#endif

	vt100_parser->r_buf.left = left - 1 ;

	return  1 ;
}

/*
 * XXX
 * mkf_map_ucs4_to_iscii() in mkf_ucs4_iscii.h is used directly in
 * parse_vt100_sequence(), though it should be used internally in mkf library
 */
int  mkf_map_ucs4_to_iscii( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;

static int
parse_vt100_sequence(
	ml_vt100_parser_t *  vt100_parser
	)
{
	mkf_char_t  ch ;
	size_t  prev_left ;

	while( 1)
	{
	#ifdef  ENABLE_SIXEL
		u_char *  str ;
	#endif

		prev_left = vt100_parser->r_buf.left ;

		/*
		 * parsing character encoding.
		 */
		(*vt100_parser->cc_parser->set_str)( vt100_parser->cc_parser ,
		#ifdef  ENABLE_SIXEL
			str =
		#endif
			CURRENT_STR_P(vt100_parser) , vt100_parser->r_buf.left) ;
		while(
		#ifdef  ENABLE_SIXEL
			/* C1 is ignored in mkf. */
			*str != 0x90 &&
		#endif
			(*vt100_parser->cc_parser->next_char)( vt100_parser->cc_parser , &ch))
		{
			/*
			 * UCS <-> OTHER CS
			 */
			if( ch.cs == ISO10646_UCS4_1)
			{
				u_char  decsp ;

				if( ch.ch[2] == 0x00 && ch.ch[3] <= 0x7f &&
				    ch.ch[1] == 0x00 && ch.ch[0] == 0x00)
				{
					/* this is always done */
					ch.ch[0] = ch.ch[3] ;
					ch.size = 1 ;
					ch.cs = US_ASCII ;
				}
				else if( use_dec_special_font &&
				         ( decsp = convert_ucs_to_dec_special(
						mkf_bytes_to_int( ch.ch , ch.size))))
				{
					ch.ch[0] = decsp ;
					ch.size = 1 ;
					ch.cs = DEC_SPECIAL ;
				}
				else if( vt100_parser->unicode_policy & NOT_USE_UNICODE_FONT)
				{
					/* convert ucs4 to appropriate charset */

					mkf_char_t  non_ucs ;

					if( mkf_map_locale_ucs4_to( &non_ucs , &ch) )
					{
						if( vt100_parser->unicode_policy &
						    USE_UNICODE_PROPERTY)
						{
							non_ucs.property = ch.property ;
						}
						else if( IS_BIWIDTH_CS( non_ucs.cs))
						{
							non_ucs.property = MKF_BIWIDTH ;
						}

						ch = non_ucs ;
					}
				}
			#if  1
				/* See http://github.com/saitoha/drcsterm/ */
				else if( ch.ch[1] == 0x10 &&
				         0x40 <= ch.ch[2] && ch.ch[2] <= 0x7e &&
					 0x20 <= ch.ch[3] && ch.ch[3] <= 0x7f &&
					 ch.ch[0] == 0x00)
				{
					ch.ch[0] = ch.ch[3] ;
					ch.cs = CS94SB_ID(ch.ch[2]) ;
					ch.size = 1 ;
					ch.property = 0 ;
				}
			#endif
			#if  ! defined(NO_DYNAMIC_LOAD_CTL) || defined(USE_IND)
				else
				{
					mkf_char_t  non_ucs ;

					if( mkf_map_ucs4_to_iscii( &non_ucs ,
						mkf_bytes_to_int( ch.ch , ch.size)))
					{
						if( vt100_parser->unicode_policy &
							USE_UNICODE_PROPERTY)
						{
							non_ucs.property = ch.property ;
						}
						ch = non_ucs ;
					}
				}
			#endif
			}
			else if( ch.cs != US_ASCII)
			{
				int  conv_to_ucs ;

				    /* XXX converting japanese gaiji to ucs. */
				if( ( vt100_parser->unicode_policy & ONLY_USE_UNICODE_FONT) ||
				    ch.cs == JISC6226_1978_NEC_EXT ||
				    ch.cs == JISC6226_1978_NECIBM_EXT ||
				    ch.cs == JISX0208_1983_MAC_EXT ||
				    ch.cs == SJIS_IBM_EXT
				#if  0
				    /* GB18030_2000 2-byte chars(==GBK) are converted to UCS */
				    || ( vt100_parser->encoding == ML_GB18030 && ch.cs == GBK)
				#endif
					)
				{
					conv_to_ucs = 1 ;
				}
				else
				{
					conv_to_ucs = 0 ;
				}

				if( conv_to_ucs ||
				    ( vt100_parser->unicode_policy & USE_UNICODE_PROPERTY))
				{
					mkf_char_t  ucs ;

					if( mkf_map_to_ucs4( &ucs , &ch))
					{
						if( conv_to_ucs)
						{
							ch = ucs ;
						}

						ch.property = mkf_get_ucs_property(
								mkf_bytes_to_int(
									ucs.ch , ucs.size)) ;
					}
				}
				else if( IS_BIWIDTH_CS( ch.cs))
				{
					ch.property = MKF_BIWIDTH ;
				}
			}

			/*
			 * NON UCS <-> NON UCS
			 */

			if( ch.size == 1)
			{
				/* single byte cs */

				if( (ch.ch[0] == 0x0 || ch.ch[0] == 0x7f))
				{
				#ifdef  DEBUG
					kik_warn_printf( KIK_DEBUG_TAG
						" 0x0/0x7f sequence is received , ignored...\n") ;
				#endif
					continue ;
				}
				else if( /* 0x1 <= ch.ch[0] && */ ch.ch[0] <= 0x1f)
				{
					/*
					 * this is a control sequence.
					 * reparsing this char in vt100_escape_sequence() ...
					 */

					vt100_parser->cc_parser->left ++ ;
					vt100_parser->cc_parser->is_eos = 0 ;

					break ;
				}

				if( ml_is_msb_set( ch.cs))
				{
					SET_MSB( ch.ch[0]) ;
				}
				else if( ( ch.cs == US_ASCII &&
				           vt100_parser->gl != US_ASCII))
				{
					ch.cs = vt100_parser->gl ;
				}
			}
			else
			{
				/* multi byte cs */

				/*
				 * XXX hack
				 * how to deal with johab 10-4-4(8-4-4) font ?
				 * is there any uhc font ?
				 */

				if( ch.cs == JOHAB)
				{
					mkf_char_t  uhc ;

					if( mkf_map_johab_to_uhc( &uhc , &ch) == 0)
					{
						continue ;
					}

					ch = uhc ;
				}

				/*
				 * XXX
				 * switching option whether this conversion is done should
				 * be introduced.
				 */
				if( ch.cs == UHC)
				{
					mkf_char_t  ksc ;

					if( mkf_map_uhc_to_ksc5601_1987( &ksc , &ch) == 0)
					{
						continue ;
					}

					ch = ksc ;
				}
			}

			put_char( vt100_parser , ch.ch , ch.size , ch.cs , ch.property) ;

			vt100_parser->r_buf.left = vt100_parser->cc_parser->left ;
		}

		vt100_parser->r_buf.left = vt100_parser->cc_parser->left ;

		flush_buffer( vt100_parser) ;

		if( vt100_parser->cc_parser->is_eos)
		{
			/* expect more input */
			break ;
		}

		/*
		 * parsing other vt100 sequences.
		 * (vt100_parser->w_buf is always flushed here.)
		 */

		if( ! parse_vt100_escape_sequence( vt100_parser))
		{
			/* expect more input */
			break ;
		}

	#ifdef  EDIT_ROUGH_DEBUG
		ml_edit_dump( vt100_parser->screen->edit) ;
	#endif

		if( vt100_parser->r_buf.left == prev_left)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG
				" unrecognized sequence[%.2x] is received , ignored...\n" ,
				*CURRENT_STR_P(vt100_parser)) ;
		#endif

			vt100_parser->r_buf.left -- ;
		}

		if( vt100_parser->r_buf.left == 0)
		{
			break ;
		}
	}

	return  1 ;
}

static int
write_loopback(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  buf ,
	size_t  len ,
	int  enable_local_echo
	)
{
	char *  orig_buf ;
	size_t  orig_left ;

	if( vt100_parser->r_buf.len < len &&
	    ! change_read_buffer_size( &vt100_parser->r_buf , len))
	{
		return  0 ;
	}

	if( (orig_left = vt100_parser->r_buf.left) > 0)
	{
		if( ! ( orig_buf = alloca( orig_left)))
		{
			return  0 ;
		}

		memcpy( orig_buf , CURRENT_STR_P(vt100_parser) , orig_left) ;
	}

	memcpy( vt100_parser->r_buf.chars , buf , len) ;
	vt100_parser->r_buf.filled_len = vt100_parser->r_buf.left = len ;

	start_vt100_cmd( vt100_parser , 1) ;
	if( enable_local_echo)
	{
		ml_screen_enable_local_echo( vt100_parser->screen) ;
	}
	/*
	 * bidi and visual-indian is always stopped from here.
	 * If you want to call {start|stop}_vt100_cmd (where ml_xterm_event_listener is called),
	 * the second argument of it shoule be 0.
	 */
	parse_vt100_sequence( vt100_parser) ;
	stop_vt100_cmd( vt100_parser , 1) ;

	if( orig_left > 0)
	{
		memcpy( vt100_parser->r_buf.chars , orig_buf , orig_left) ;
		vt100_parser->r_buf.filled_len = vt100_parser->r_buf.left = orig_left ;
	}

	return  1 ;
}


/* --- global functions --- */

void
ml_set_use_dec_special_font(
	int  use
	)
{
	use_dec_special_font = use ;
}

void
ml_set_use_alt_buffer(
	int  use
	)
{
	use_alt_buffer = use ;
}

void
ml_set_use_ansi_colors(
	int  use
	)
{
	use_ansi_colors = use ;
}

ml_vt100_parser_t *
ml_vt100_parser_new(
	ml_screen_t *  screen ,
	ml_char_encoding_t  encoding ,
	ml_unicode_policy_t  policy ,
	u_int  col_size_a ,
	int  use_char_combining ,
	int  use_multi_col_char ,
	char *  win_name ,
	char *  icon_name
	)
{
	ml_vt100_parser_t *  vt100_parser ;

	if( ( vt100_parser = calloc( 1 , sizeof( ml_vt100_parser_t))) == NULL)
	{
		return  NULL ;
	}

	ml_str_init( vt100_parser->w_buf.chars , PTY_WR_BUFFER_SIZE) ;
	vt100_parser->w_buf.output_func = ml_screen_overwrite_chars ;

	vt100_parser->screen = screen ;

	vt100_parser->log_file = -1 ;
	
	vt100_parser->cs = UNKNOWN_CS ;
	vt100_parser->fg_color = ML_FG_COLOR ;
	vt100_parser->bg_color = ML_BG_COLOR ;
	vt100_parser->use_char_combining = use_char_combining ;
	vt100_parser->use_multi_col_char = use_multi_col_char ;

	vt100_parser->unicode_policy = policy ;

	if( ( vt100_parser->cc_conv = ml_conv_new( encoding)) == NULL)
	{
		goto  error ;
	}

	if( ( vt100_parser->cc_parser = ml_parser_new( encoding)) == NULL)
	{
		(*vt100_parser->cc_conv->delete)( vt100_parser->cc_conv) ;

		goto  error ;
	}

	vt100_parser->encoding = encoding ;

	if( win_name)
	{
		vt100_parser->win_name = strdup( win_name) ;
	}

	if( icon_name)
	{
		vt100_parser->icon_name = strdup( icon_name) ;
	}

	vt100_parser->gl = US_ASCII ;
	vt100_parser->g0 = US_ASCII ;
	vt100_parser->g1 = US_ASCII ;

	ml_vt100_parser_set_col_size_of_width_a( vt100_parser , col_size_a) ;

#if  0
	/* Default value of modify_*_keys except modify_other_keys is 2. */
	vt100_parser->modify_cursor_keys = 2 ;
	vt100_parser->modify_function_keys = 2 ;
#endif

	return  vt100_parser ;

error:
	free( vt100_parser) ;

	return  NULL ;
}

int
ml_vt100_parser_delete(
	ml_vt100_parser_t *  vt100_parser
	)
{
	ml_str_final( vt100_parser->w_buf.chars , PTY_WR_BUFFER_SIZE) ;
	(*vt100_parser->cc_parser->delete)( vt100_parser->cc_parser) ;
	(*vt100_parser->cc_conv->delete)( vt100_parser->cc_conv) ;

	if( vt100_parser->log_file != -1)
	{
		close( vt100_parser->log_file) ;
	}

	free( vt100_parser->r_buf.chars) ;

	free( vt100_parser->win_name) ;
	free( vt100_parser->icon_name) ;
	free( vt100_parser->saved_win_names.names) ;
	free( vt100_parser->saved_icon_names.names) ;

	free( vt100_parser) ;
	
	return  1 ;
}

int
ml_vt100_parser_set_pty(
	ml_vt100_parser_t *  vt100_parser ,
	ml_pty_ptr_t  pty
	)
{
	vt100_parser->pty = pty ;
	
	return  1 ;
}

int
ml_vt100_parser_set_xterm_listener(
	ml_vt100_parser_t *  vt100_parser ,
	ml_xterm_event_listener_t *  xterm_listener
	)
{
	vt100_parser->xterm_listener = xterm_listener ;

	return  1 ;
}

int
ml_vt100_parser_set_config_listener(
	ml_vt100_parser_t *  vt100_parser ,
	ml_config_event_listener_t *  config_listener
	)
{
	vt100_parser->config_listener = config_listener ;

	return  1 ;
}

int
ml_parse_vt100_sequence(
	ml_vt100_parser_t *  vt100_parser
	)
{
	int  count ;

	if( ml_screen_local_echo_wait( vt100_parser->screen , 500))
	{
		return  1 ;
	}

	if( ! vt100_parser->pty || receive_bytes( vt100_parser) == 0)
	{
		return  0 ;
	}

	start_vt100_cmd( vt100_parser , 1) ;

	ml_screen_disable_local_echo( vt100_parser->screen) ;

	/*
	 * bidi and visual-indian is always stopped from here.
	 * If you want to call {start|stop}_vt100_cmd (where ml_xterm_event_listener is called),
	 * the second argument of it shoule be 0.
	 */

	/* Maximum size of sequence parsed once is PTY_RD_BUFFER_SIZE * 3. */
	count = 0 ;
	do
	{
		parse_vt100_sequence( vt100_parser) ;
	}
	while(  /* (PTY_RD_BUFFER_SIZE / 2) is baseless. */
		vt100_parser->r_buf.filled_len >= (PTY_RD_BUFFER_SIZE / 2) &&
		(++count) < 3 && receive_bytes( vt100_parser)) ;

	stop_vt100_cmd( vt100_parser , 1) ;

	return  1 ;
}

int
ml_vt100_parser_write_loopback(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  buf ,
	size_t  len
	)
{
	return  write_loopback( vt100_parser , buf , len , 0) ;
}

int
ml_vt100_parser_local_echo(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  buf ,
	size_t  len
	)
{
	size_t  count ;

	for( count = 0 ; count < len ; count++)
	{
		if( buf[count] < 0x20)
		{
			ml_screen_local_echo_wait( vt100_parser->screen , 0) ;
			ml_parse_vt100_sequence( vt100_parser) ;

			return  1 ;
		}
	}

	ml_parse_vt100_sequence( vt100_parser) ;

	if( ! vt100_parser->is_underlined)
	{
		char *  new_buf ;
		size_t  new_len ;

		if( ( new_buf = alloca( ( new_len = 4 + len + 5))))
		{
			memcpy( new_buf , "\x1b[4m" , 4) ;
			memcpy( new_buf + 4 , buf , len) ;
			memcpy( new_buf + 4 + len , "\x1b[24m" , 5) ;
			buf = new_buf ;
			len = new_len ;
		}
	}

	write_loopback( vt100_parser , buf , len , 1) ;

	return  1 ;
}

int
ml_vt100_parser_change_encoding(
	ml_vt100_parser_t *  vt100_parser ,
	ml_char_encoding_t  encoding
	)
{
	mkf_parser_t *  cc_parser ;
	mkf_conv_t *  cc_conv ;

	cc_conv = ml_conv_new( encoding) ;
	cc_parser = ml_parser_new( encoding) ;

	if( cc_parser == NULL || cc_conv == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " encoding not changed.\n") ;
	#endif
		if( cc_parser)
		{
			(*cc_parser->delete)( cc_parser) ;
		}

		if( cc_conv)
		{
			(*cc_conv->delete)( cc_conv) ;
		}

		return  0 ;
	}
	
#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " encoding changed.\n") ;
#endif

	(*vt100_parser->cc_parser->delete)( vt100_parser->cc_parser) ;
	(*vt100_parser->cc_conv->delete)( vt100_parser->cc_conv) ;

	vt100_parser->encoding = encoding ;
	vt100_parser->cc_parser = cc_parser ;
	vt100_parser->cc_conv = cc_conv ;

	/* reset */
	vt100_parser->gl = US_ASCII ;
	vt100_parser->g0 = US_ASCII ;
	vt100_parser->g1 = US_ASCII ;
	vt100_parser->is_so = 0 ;
	
	return  1 ;
}

ml_char_encoding_t
ml_vt100_parser_get_encoding(
	ml_vt100_parser_t *  vt100_parser
	)
{
	return  vt100_parser->encoding ;
}

size_t
ml_vt100_parser_convert_to(
	ml_vt100_parser_t *  vt100_parser ,
	u_char *  dst ,
	size_t  len ,
	mkf_parser_t *  parser
	)
{
	return  (*vt100_parser->cc_conv->convert)( vt100_parser->cc_conv , dst , len , parser) ;
}

int
ml_init_encoding_parser(
	ml_vt100_parser_t *  vt100_parser
	)
{
	(*vt100_parser->cc_parser->init)( vt100_parser->cc_parser) ;
	vt100_parser->gl = US_ASCII ;
	vt100_parser->g0 = US_ASCII ;
	vt100_parser->g1 = US_ASCII ;
	vt100_parser->is_so = 0 ;

	return  1 ;
}

int
ml_init_encoding_conv(
	ml_vt100_parser_t *  vt100_parser
	)
{
	(*vt100_parser->cc_conv->init)( vt100_parser->cc_conv) ;

	/*
	 * XXX
	 * this causes unexpected behaviors in some applications(e.g. biew) ,
	 * but this is necessary , since 0x00 - 0x7f is not necessarily US-ASCII
	 * in these encodings but key input or selection paste assumes that
	 * 0x00 - 0x7f should be US-ASCII at the initial state.
	 */
	if( IS_STATEFUL_ENCODING(vt100_parser->encoding))
	{
		ml_init_encoding_parser(vt100_parser) ;
	}

	return  1 ;
}

int
ml_vt100_parser_set_col_size_of_width_a(
	ml_vt100_parser_t *  vt100_parser ,
	u_int  col_size_a
	)
{
	if( col_size_a == 1 || col_size_a == 2)
	{
		vt100_parser->col_size_of_width_a = col_size_a ;
	}
	else
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" col size should be 1 or 2. default value 1 is used.\n") ;
	#endif
	
		vt100_parser->col_size_of_width_a = 1 ;
	}

	return  1 ;
}

int
ml_vt100_parser_set_use_char_combining(
	ml_vt100_parser_t *  vt100_parser ,
	int  flag
	)
{
	vt100_parser->use_char_combining = flag ;

	return  1 ;
}

int
ml_vt100_parser_set_use_multi_col_char(
	ml_vt100_parser_t *  vt100_parser ,
	int  flag
	)
{
	vt100_parser->use_multi_col_char = flag ;

	return  1 ;
}

int
ml_vt100_parser_set_logging_vt_seq(
	ml_vt100_parser_t *  vt100_parser ,
	int  flag
	)
{
	vt100_parser->logging_vt_seq = flag ;
	
	return  1 ;
}
