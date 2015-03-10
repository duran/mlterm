/*
 *	$Id$
 */

#include  <kiklib/kik_config.h>	/* HAVE_WINDOWS_H, WORDS_BIGENDIAN */
#ifdef  HAVE_WINDOWS_H
#include  <windows.h>	/* In Cygwin <windows.h> is not included and error happens in jni.h. */
#endif

#include  "mlterm_MLTermPty.h"

#include  <unistd.h>		/* getcwd */
#include  <kiklib/kik_locale.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_sig_child.h>
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_str.h>	/* kik_str_alloca_dup */
#include  <kiklib/kik_conf.h>
#include  <kiklib/kik_conf_io.h>
#include  <kiklib/kik_path.h>	/* kik_basename */
#include  <kiklib/kik_unistd.h>	/* kik_setenv */
#include  <kiklib/kik_args.h>	/* _kik_arg_str_to_array */
#include  <kiklib/kik_dialog.h>
#include  <mkf/mkf_utf16_conv.h>
#include  <ml_str_parser.h>
#include  <ml_term_manager.h>
#include  <../main/version.h>


#if  defined(USE_WIN32API)
#define CONFIG_PATH "."
#elif  defined(SYSCONFDIR)
#define CONFIG_PATH SYSCONFDIR
#else
#define CONFIG_PATH "/etc"
#endif

/* Same as those defined in SWT.java */
#define  AltMask (1 << 16)
#define  ShiftMask  (1 << 17)
#define  ControlMask  (1 << 18)


#if  1
#define  TUNEUP_HACK
#endif

#if  0
#define  USE_LOCAL_ECHO_BY_DEFAULT
#endif

#if  0
#define  __DEBUG
#endif


typedef struct  native_obj
{
	JNIEnv *  env ;
	jobject  obj ;			/* MLTermPty */
	jobject  listener ;		/* MLTermPtyListener */
	ml_term_t *  term ;
	ml_pty_event_listener_t   pty_listener ;
	ml_config_event_listener_t  config_listener ;
	ml_screen_event_listener_t  screen_listener ;
	ml_xterm_event_listener_t  xterm_listener ;

	u_int16_t  prev_mouse_report_col ;
	u_int16_t  prev_mouse_report_row ;

} native_obj_t ;


/* --- static variables --- */

static mkf_parser_t *  str_parser ;
static mkf_parser_t *  utf8_parser ;
static mkf_conv_t *  utf16_conv ;

#if  defined(USE_WIN32API) && ! defined(USE_LIBSSH2)
static char *  plink ;
#endif

#ifdef  USE_LIBSSH2
static u_int  keepalive_interval ;
#endif


/* --- static functions --- */

static void
pty_closed(
	void *  p
	)
{
	((native_obj_t *)p)->term = NULL ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " PTY CLOSED\n") ;
#endif
}

static void
show_config(
	void *  p ,
	char *  msg
	)
{
	ml_term_show_message( ((native_obj_t *)p)->term , msg) ;
}


#ifdef  USE_LIBSSH2
static JNIEnv *  env_for_dialog ;
static int
dialog_callback(
	kik_dialog_style_t  style ,
	char *  msg
	)
{
	jclass  class ;
	static jmethodID  mid ;

	if( style != KIK_DIALOG_OKCANCEL || ! env_for_dialog)
	{
		return  -1 ;
	}

	/* This function is called rarely, so jclass is not static. */
	class = (*env_for_dialog)->FindClass( env_for_dialog , "mlterm/ConfirmDialog") ;

	if( ! mid)
	{
		mid = (*env_for_dialog)->GetStaticMethodID( env_for_dialog ,
				class , "show" , "(Ljava/lang/String;)Z") ;
	}

	if( (*env_for_dialog)->CallStaticObjectMethod( env_for_dialog , class , mid ,
				(*env_for_dialog)->NewStringUTF( env_for_dialog , msg)))
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}
#endif


static void
set_config(
	void *  p ,
	char *  dev ,
	char *  key ,
	char *  value
	)
{
	native_obj_t *  nativeObj ;

	nativeObj = p ;

	if( ml_term_set_config( nativeObj->term , key , value))
	{
		/* do nothing */
	}
	else if( strcmp( key , "encoding") == 0)
	{
		ml_char_encoding_t  encoding ;

		if( ( encoding = ml_get_char_encoding( value)) != ML_UNKNOWN_ENCODING)
		{
			ml_term_change_encoding( nativeObj->term , encoding) ;
		}
	}
	else if( strcmp( key , "use_multi_column_char") == 0)
	{
		int  flag ;

		if( ( flag = true_or_false( value)) != -1)
		{
			ml_term_set_use_multi_col_char( nativeObj->term , flag) ;
		}
	}
}

static void
get_config(
	void *  p ,
	char *  dev ,
	char *  key ,
	int  to_menu
	)
{
	native_obj_t *  nativeObj ;
	ml_term_t *  term ;
	char *  value ;

	nativeObj = p ;

	if( dev)
	{
		if( ! ( term = ml_get_term( dev)))
		{
			return ;
		}
	}
	else
	{
		term = nativeObj->term ;
	}

	if( ml_term_get_config( term , nativeObj->term , key , to_menu , NULL))
	{
		return ;
	}

	value = NULL ;

	if( strcmp( key , "use_multi_column_char") == 0)
	{
		if( ml_term_is_using_multi_col_char( term))
		{
			value = "true" ;
		}
		else
		{
			value = "false" ;
		}
	}
	else if( strcmp( key , "pty_list") == 0)
	{
		value = ml_get_pty_list() ;
	}

	if( ! value)
	{
		ml_term_response_config( nativeObj->term , "error" , NULL , to_menu) ;
	}
	else
	{
		ml_term_response_config( nativeObj->term , key , value , to_menu) ;
	}
}

static int
exec_cmd(
	void *  p ,
	char *  cmd
	)
{
	static jmethodID  mid ;
	native_obj_t *  nativeObj ;
	jstring  jstr_cmd ;

	nativeObj = p ;

	if( strncmp( cmd , "browser" , 7) != 0)
	{
		return  0 ;
	}

	jstr_cmd = (*nativeObj->env)->NewStringUTF( nativeObj->env , cmd) ;

	if( ! mid)
	{
		mid = (*nativeObj->env)->GetMethodID( nativeObj->env ,
			(*nativeObj->env)->FindClass( nativeObj->env , "mlterm/MLTermPtyListener"),
			"executeCommand" , "(Ljava/lang/String;)V") ;
	}

	if( nativeObj->listener)
	{
		(*nativeObj->env)->CallVoidMethod( nativeObj->env ,
			nativeObj->listener , mid , jstr_cmd) ;
	}

	return  1 ;
}


static int
window_scroll_upward_region(
	void *  p ,
	int  beg_row ,
	int  end_row ,
	u_int  size
	)
{
	static jmethodID  mid ;
	native_obj_t *  nativeObj ;

	nativeObj = p ;

	if( ! mid)
	{
		mid = (*nativeObj->env)->GetMethodID( nativeObj->env ,
					(*nativeObj->env)->FindClass( nativeObj->env ,
						"mlterm/MLTermPtyListener") ,
					"linesScrolledOut" , "(I)V") ;
	}

	if( nativeObj->listener &&
	    beg_row == 0 && end_row + 1 == ml_term_get_logical_rows( nativeObj->term) &&
	    ! ml_screen_is_alternative_edit( nativeObj->term->screen))
	{
		(*nativeObj->env)->CallVoidMethod( nativeObj->env ,
				nativeObj->listener , mid , size) ;
	}

	return  0 ;
}


static void
resize(
	void *  p ,
	u_int  width ,
	u_int  height
	)
{
	static jmethodID  mid ;
	native_obj_t *  nativeObj ;

	nativeObj = p ;

	if( ! mid)
	{
		mid = (*nativeObj->env)->GetMethodID( nativeObj->env ,
					(*nativeObj->env)->FindClass( nativeObj->env ,
						"mlterm/MLTermPtyListener") ,
					"resize" , "(IIII)V") ;
	}

	if( nativeObj->listener)
	{
		(*nativeObj->env)->CallVoidMethod( nativeObj->env ,
						nativeObj->listener , mid ,
						width , height ,
						ml_term_get_cols( nativeObj->term) ,
						ml_term_get_rows( nativeObj->term)) ;
	}
}

static void
set_mouse_report(
	void *  p
	)
{
	if( ! ml_term_get_mouse_report_mode( ((native_obj_t*)p)->term))
	{
		((native_obj_t*)p)->prev_mouse_report_col =
		((native_obj_t*)p)->prev_mouse_report_row = 0 ;
	}
}

static void
bel(
	void *  p
	)
{
	static jmethodID  mid ;
	native_obj_t *  nativeObj ;

	nativeObj = p ;

	if( ! mid)
	{
		mid = (*nativeObj->env)->GetMethodID( nativeObj->env ,
					(*nativeObj->env)->FindClass( nativeObj->env ,
						"mlterm/MLTermPtyListener") ,
					"bell" , "()V") ;
	}

	if( nativeObj->listener)
	{
		(*nativeObj->env)->CallVoidMethod( nativeObj->env , nativeObj->listener , mid) ;
	}
}


/*
 * 2: New style should be added.
 * 1: Previous style is continued.
 * 0: No style.
 */
static int
need_style(
	ml_char_t *  ch ,
	ml_char_t *  prev_ch
	)
{
	int  need_style ;

	if( ml_char_fg_color( ch) != ML_FG_COLOR ||
	    ml_char_bg_color( ch) != ML_BG_COLOR ||
	    ml_char_underline_style( ch) ||
	    ml_char_is_crossed_out( ch) ||
	    (ml_char_font( ch) & (FONT_BOLD|FONT_ITALIC)))
	{
		need_style = 2 ;
	}
	else
	{
		need_style = 0 ;
	}

	if( prev_ch &&
	    ml_char_fg_color( ch) == ml_char_fg_color( prev_ch) &&
	    ml_char_bg_color( ch) == ml_char_bg_color( prev_ch) &&
	    ml_char_underline_style( ch) == ml_char_underline_style( prev_ch) &&
	    ml_char_is_crossed_out( ch) == ml_char_is_crossed_out( prev_ch) &&
	    (ml_char_font( ch) & (FONT_BOLD|FONT_ITALIC)) ==
	    (ml_char_font( prev_ch) & (FONT_BOLD|FONT_ITALIC)) )
	{
		if( need_style)
		{
			/* Continual style */
			return  1 ;
		}
	}

	return  need_style ;
}

static u_int
get_num_of_filled_chars_except_spaces(
	ml_line_t *  line
	)
{
	if( ml_line_is_empty(line))
	{
		return  0 ;
	}
	else
	{
		int  char_index ;

		for( char_index = ml_line_end_char_index(line) ; char_index >= 0 ; char_index --)
		{
			if( ! ml_char_equal( line->chars + char_index , ml_sp_ch()))
			{
				return  char_index + 1 ;
			}
		}

		return  0 ;
	}
}

static void
draw_cursor(
	ml_term_t *  term ,
	int (*func)( ml_line_t * , int)
	)
{
	ml_line_t *  line ;

	if( ( line = ml_term_get_cursor_line( term)))
	{
		(*func)( line , ml_term_cursor_char_index( term)) ;
	}
}


/* --- global functions --- */

JNIEXPORT void JNICALL
Java_mlterm_MLTermPty_setLibDir(
	JNIEnv *  env ,
	jclass  class ,
	jstring  jstr_dir	/* Always ends with '/' or '\\' */
	)
{
	const char *  dir ;
	const char *  value ;
#ifdef  HAVE_WINDOWS_H
	const char *  key = "PATH" ;
#else
	const char *  key = "LD_LIBRARY_PATH" ;
#endif

	dir = (*env)->GetStringUTFChars( env , jstr_dir , NULL) ;

	/*
	 * Reset PATH or LD_LIBRARY_PATH to be able to load shared libraries
	 * in %HOMEPATH%/mlterm/java or ~/.mlterm/java/.
	 */

	if( ( value = getenv( key)))
	{
		char *  p ;

		if( ! ( p = alloca( strlen( value) + 1 + strlen( dir) + 1)))
		{
			return ;
		}

	#ifdef  USE_WIN32API
		sprintf( p , "%s;%s" , dir , value) ;
	#else
		sprintf( p , "%s:%s" , dir , value) ;
	#endif
		value = p ;
	}
	else
	{
		value = dir ;
	}

	kik_setenv( key , value , 1) ;

#ifdef  DEBUG
	{
	#ifdef  HAVE_WINDOWS_H
		char  buf[4096] ;

		GetEnvironmentVariable( key , buf , sizeof(buf)) ;
		value = buf ;
	#endif

		kik_debug_printf( KIK_DEBUG_TAG " setting environment variable %s=%s\n" ,
			key , value) ;
	}
#endif	/* DEBUG */

#if  defined(USE_WIN32API) && ! defined(USE_LIBSSH2)
	/*
	 * SetEnvironmentVariable( "PATH" , %HOMEPATH%\mlterm\java;%PATH) doesn't make effect
	 * for CreateProcess(), differently from LoadLibrary().
	 */
	if( ( plink = malloc( strlen( dir) + 9 + 1)))
	{
		sprintf( plink , "%s%s" , dir , "plink.exe") ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " %s is set for default cmd path.\n" , plink) ;
	#endif
	}
#endif

	(*env)->ReleaseStringUTFChars( env , jstr_dir , dir) ;
}

JNIEXPORT jlong JNICALL
Java_mlterm_MLTermPty_nativeOpen(
	JNIEnv *  env ,
	jobject  obj ,
	jstring  jstr_host ,
	jstring  jstr_pass ,	/* can be NULL */
	jint  cols ,
	jint  rows ,
	jstring  jstr_encoding ,
	jarray  jarray_argv
	)
{
	native_obj_t *  nativeObj ;
	ml_char_encoding_t  encoding ;
	int  is_auto_encoding ;
	char **  argv ;
	char *  host ;
	char *  pass ;
	char *  envv[4] ;
	char *  cmd_path ;
	int  ret ;

	static u_int  tab_size ;
	static ml_char_encoding_t  encoding_default ;
	static int  is_auto_encoding_default ;
	static ml_unicode_policy_t  unicode_policy ;
	static u_int  col_size_a ;
	static int  use_char_combining ;
	static int  use_multi_col_char ;
	static int  use_login_shell ;
	static int  logging_vt_seq ;
	static int  use_local_echo ;
	static int  use_auto_detect ;
	static char *  public_key ;
	static char *  private_key ;
	static char **  default_argv ;
	static char *  default_cmd_path ;

#if  defined(USE_WIN32API) && defined(USE_LIBSSH2)
	static int  wsa_inited = 0 ;

	if( ! wsa_inited)
	{
		WSADATA  wsadata ;
		WSAStartup( MAKEWORD(2,0) , &wsadata) ;
		wsa_inited = 1 ;

		/*
		 * Prevent ml_pty_ssh from spawning a thread to watch pty.
		 * If both ml_pty_ssh and MLTerm.java spawn threads to watch ptys,
		 * problems will happen in reloading (that is, finalizing and
		 * starting simultaneously) an applet page (mltermlet.html)
		 * in a web browser.
		 *
		 * This hack doesn't make sense to ml_pty_pipewin32.c.
		 * In the first place, if ml_pty_pipewin32 is used,
		 * Java_mlterm_MLTermPty_waitForReading() returns JNI_FALSE
		 * and PtyWather thread of MLTerm.java immediately exits.
		 * So threads in ml_pty_pipewin32 should not be prevented.
		 */
		CreateEvent( NULL , FALSE , FALSE , "PTY_READ_READY") ;
	}
#endif

	if( ! str_parser)
	{
		kik_conf_t *  conf ;

		kik_init_prog( "mlterm" , "3.4.4") ;
		kik_set_sys_conf_dir( CONFIG_PATH) ;
		kik_locale_init( "") ;
		kik_sig_child_init() ;
	#ifdef  USE_LIBSSH2
		kik_dialog_set_callback( dialog_callback) ;
	#endif
		ml_term_manager_init(1) ;

		str_parser = ml_str_parser_new() ;
		utf8_parser = ml_parser_new( ML_UTF8) ;
	#ifdef  WORDS_BIGENDIAN
		utf16_conv = mkf_utf16_conv_new() ;
	#else
		utf16_conv = mkf_utf16le_conv_new() ;
	#endif

		tab_size = 8 ;
		encoding_default = ml_get_char_encoding( "auto") ;
		is_auto_encoding_default = 1 ;

		if( strcmp( kik_get_lang() , "ja") == 0)
		{
			col_size_a = 2 ;
		}
		else
		{
			col_size_a = 1 ;
		}

		use_char_combining = 1 ;
		use_multi_col_char = 1 ;
	#ifdef  USE_LOCAL_ECHO_BY_DEFAULT
		use_local_echo = 1 ;
	#else
		use_local_echo = 0 ;
	#endif

		if( ( conf = kik_conf_new()))
		{
			char *  rcpath ;
			char *  value ;

			if( ( rcpath = kik_get_sys_rc_path( "mlterm/main")))
			{
				kik_conf_read( conf , rcpath) ;
				free( rcpath) ;
			}

			if( ( rcpath = kik_get_user_rc_path( "mlterm/main")))
			{
				kik_conf_read( conf , rcpath) ;
				free( rcpath) ;
			}

			if( ( value = kik_conf_get_value( conf , "logging_msg")) &&
			    strcmp( value , "false") == 0)
			{
				kik_set_msg_log_file_name( NULL) ;
			}
			else
			{
				kik_set_msg_log_file_name( "mlterm/msg.log") ;
			}

			if( ( value = kik_conf_get_value( conf , "logging_vt_seq")) &&
			    strcmp( value , "true") == 0)
			{
				logging_vt_seq = 1 ;
			}

			if( ( value = kik_conf_get_value( conf , "tabsize")))
			{
				kik_str_to_uint( &tab_size , value) ;
			}

			if( ( value = kik_conf_get_value( conf , "encoding")))
			{
				ml_char_encoding_t  e ;
				if( ( e = ml_get_char_encoding( value)) != ML_UNKNOWN_ENCODING)
				{
					encoding_default = e ;

					if( strcmp( value , "auto") == 0)
					{
						is_auto_encoding_default = 1 ;
					}
					else
					{
						is_auto_encoding_default = 0 ;
					}
				}
			}

			if( ( value = kik_conf_get_value( conf , "not_use_unicode_font")))
			{
				if( strcmp( value , "true") == 0)
				{
					unicode_policy = NOT_USE_UNICODE_FONT ;
				}
			}

			if( ( value = kik_conf_get_value( conf , "only_use_unicode_font")))
			{
				if( strcmp( value , "true") == 0)
				{
					if( unicode_policy == NOT_USE_UNICODE_FONT)
					{
						unicode_policy = 0 ;
					}
					else
					{
						unicode_policy = ONLY_USE_UNICODE_FONT ;
					}
				}
			}

			if( ( value = kik_conf_get_value( conf , "col_size_of_width_a")))
			{
				kik_str_to_uint( &col_size_a , value) ;
			}

			if( ( value = kik_conf_get_value( conf , "use_combining")))
			{
				if( strcmp( value , "false") == 0)
				{
					use_char_combining = 0 ;
				}
			}

			if( ( value = kik_conf_get_value( conf , "use_muti_col_char")))
			{
				if( strcmp( value , "false") == 0)
				{
					use_multi_col_char = 0 ;
				}
			}

			if( ( value = kik_conf_get_value( conf , "use_login_shell")))
			{
				if( strcmp( value , "true") == 0)
				{
					use_login_shell = 1 ;
				}
			}

			if( ( value = kik_conf_get_value( conf , "use_local_echo")))
			{
			#ifdef  USE_LOCAL_ECHO_BY_DEFAULT
				if( strcmp( value , "false") == 0)
				{
					use_local_echo = 0 ;
				}
			#else
				if( strcmp( value , "true") == 0)
				{
					use_local_echo = 1 ;
				}
			#endif
			}

			if( ( value = kik_conf_get_value( conf , "use_alt_buffer")))
			{
				if( strcmp( value , "false") == 0)
				{
					ml_set_use_alt_buffer( 0) ;
				}
			}

			if( ( value = kik_conf_get_value( conf , "use_ansi_colors")))
			{
				if( strcmp( value , "false") == 0)
				{
					ml_set_use_ansi_colors( 0) ;
				}
			}

			if( ( value = kik_conf_get_value( conf , "auto_detect_encodings")))
			{
				ml_set_auto_detect_encodings( value) ;
			}

			if( ( value = kik_conf_get_value( conf , "use_auto_detect")))
			{
				if( strcmp( value , "true") == 0)
				{
					use_auto_detect = 1 ;
				}
			}

		#ifdef  USE_LIBSSH2
			if( ( value = kik_conf_get_value( conf , "ssh_public_key")))
			{
				public_key = strdup( value) ;
			}

			if( ( value = kik_conf_get_value( conf , "ssh_private_key")))
			{
				private_key = strdup( value) ;
			}

			if( ( value = kik_conf_get_value( conf , "cipher_list")))
			{
				ml_pty_ssh_set_cipher_list( strdup( value)) ;
			}

			if( ( value = kik_conf_get_value( conf , "ssh_keepalive_interval")))
			{
				if( kik_str_to_uint( &keepalive_interval , value) &&
				    keepalive_interval > 0)
				{
					ml_pty_ssh_set_keepalive_interval( keepalive_interval) ;
				}
			}

			if( ( value = kik_conf_get_value( conf , "ssh_x11_forwarding")))
			{
				if( strcmp( value , "true") == 0)
				{
					ml_pty_ssh_set_use_x11_forwarding( NULL , 1) ;
				}
			}

			if( ( value = kik_conf_get_value( conf , "allow_scp")))
			{
				if( strcmp( value , "true") == 0)
				{
					ml_set_use_scp_full( 1) ;
				}
			}
		#endif

		#if  0
			/* XXX How to get password ? */
			if( ( value = kik_conf_get_value( conf , "default_server")))
			{
				default_server = strdup( value) ;
			}
		#endif

			if( ( ( value = kik_conf_get_value( conf , "exec_cmd"))
			#if  defined(USE_WIN32API) && ! defined(USE_LIBSSH2)
			      || ( value = plink)
			#endif
			      ) &&
			    ( default_argv = malloc( sizeof(char*) *
						kik_count_char_in_str( value , ' ') + 2)))
			{
				int  argc ;

				_kik_arg_str_to_array( default_argv , &argc , strdup( value)) ;
				default_cmd_path = default_argv[0] ;
			}

			kik_conf_delete( conf) ;
		}

		unicode_policy |= ONLY_USE_UNICODE_BOXDRAW_FONT ;
	}

	if( ! ( nativeObj = calloc( sizeof( native_obj_t) , 1)))
	{
		return  0 ;
	}

	encoding = encoding_default ;
	is_auto_encoding = is_auto_encoding_default ;

	if( jstr_encoding)
	{
		char *  p ;
		ml_char_encoding_t  e ;

		p = (*env)->GetStringUTFChars( env , jstr_encoding , NULL) ;

		if( ( e = ml_get_char_encoding( p)) != ML_UNKNOWN_ENCODING)
		{
			encoding = e ;

			if( strcmp( p , "auto") == 0)
			{
				is_auto_encoding = 1 ;
			}
			else
			{
				is_auto_encoding = 0 ;
			}
		}

		(*env)->ReleaseStringUTFChars( env , jstr_encoding , p) ;
	}

	if( ! ( nativeObj->term = ml_create_term( "xterm" , cols , rows , tab_size , 0 , encoding ,
					is_auto_encoding , use_auto_detect , logging_vt_seq ,
					unicode_policy , col_size_a ,
					use_char_combining , use_multi_col_char ,
					0 /* use_ctl */ , 0 /* bidi_mode */ ,
					NULL /* bidi_separators */ ,
					0 /* use_dynamic_comb */ , BSM_STATIC ,
					0 /* vertical_mode */ , use_local_echo , NULL , NULL , 0)))
	{
		goto  error ;
	}

	nativeObj->pty_listener.self = nativeObj ;
	nativeObj->pty_listener.closed = pty_closed ;
	nativeObj->pty_listener.show_config = show_config ;

	nativeObj->config_listener.self = nativeObj ;
	nativeObj->config_listener.set = set_config ;
	nativeObj->config_listener.get = get_config ;
	nativeObj->config_listener.exec = exec_cmd ;

	nativeObj->screen_listener.self = nativeObj ;
	nativeObj->screen_listener.window_scroll_upward_region = window_scroll_upward_region ;

	nativeObj->xterm_listener.self = nativeObj ;
	nativeObj->xterm_listener.resize = resize ;
	nativeObj->xterm_listener.set_mouse_report = set_mouse_report ;
	nativeObj->xterm_listener.bel = bel ;

	ml_term_attach( nativeObj->term , &nativeObj->xterm_listener ,
		&nativeObj->config_listener , &nativeObj->screen_listener ,
		&nativeObj->pty_listener) ;

	if( jstr_host)
	{
		host = (*env)->GetStringUTFChars( env , jstr_host , NULL) ;
	}
	else if( ! ( host = getenv( "DISPLAY")))
	{
		host = ":0.0" ;
	}

	if( jstr_pass)
	{
		pass = (*env)->GetStringUTFChars( env , jstr_pass , NULL) ;
	}
	else
	{
		pass = NULL ;
	}

	if( jarray_argv)
	{
		jsize  len ;
		jsize  count ;

		len = (*env)->GetArrayLength( env , jarray_argv) ;
		argv = alloca( sizeof(char*) * (len + 1)) ;

		for( count = 0 ; count < len ; count++)
		{
			argv[count] = (*env)->GetStringUTFChars( env ,
						(*env)->GetObjectArrayElement( env ,
							jarray_argv , count) ,
						NULL) ;
		}
		argv[count] = NULL ;
		cmd_path = argv[0] ;
	}
	else if( default_argv)
	{
		argv = default_argv ;
		cmd_path = default_cmd_path ;
	}
	else
	{
	#ifndef  USE_WIN32API
		if( pass)
	#endif
		{
			cmd_path = NULL ;
			argv = alloca( sizeof(char*)) ;
			argv[0] = NULL ;
		}
	#ifndef  USE_WIN32API
		else
		{
			cmd_path = getenv( "SHELL") ;
			argv = alloca( sizeof(char*) * 2) ;
			argv[1] = NULL ;
		}
	#endif
	}

	if( cmd_path)
	{
		if( use_login_shell)
		{
			argv[0] = alloca( strlen( cmd_path) + 2) ;
			sprintf( argv[0] , "-%s" , kik_basename( cmd_path)) ;
		}
		else
		{
			argv[0] = kik_basename( cmd_path) ;
		}
	}

	envv[0] = alloca( 8 + strlen( host) + 1) ;
	sprintf( envv[0] , "DISPLAY=%s" , host) ;
	envv[1] = "TERM=xterm" ;
	envv[2] = "COLORFGBG=default;default" ;
	envv[3] = NULL ;

#ifdef  USE_LIBSSH2
	env_for_dialog = env ;
#endif

	ret = ml_term_open_pty( nativeObj->term , cmd_path , argv , envv ,
			host , NULL , pass , public_key , private_key , 0 , 0) ;

#ifdef  USE_LIBSSH2
	env_for_dialog = NULL ;
#endif

	if( jarray_argv)
	{
		jsize  count ;

		argv[0] = cmd_path ;

		for( count = 0 ; argv[count] ; count++)
		{
			(*env)->ReleaseStringUTFChars( env ,
				(*env)->GetObjectArrayElement( env , jarray_argv , count) ,
				argv[count]) ;
		}
	}

	if( jstr_host)
	{
		(*env)->ReleaseStringUTFChars( env , jstr_host , host) ;
	}

	if( pass)
	{
		(*env)->ReleaseStringUTFChars( env , jstr_pass , pass) ;
	}

	if( ret)
	{
		return  nativeObj ;
	}

error:
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Failed to open pty.\n") ;
#endif
	free( nativeObj) ;

	return  0 ;
}

JNIEXPORT void JNICALL
Java_mlterm_MLTermPty_nativeClose(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nobj
	)
{
	native_obj_t *  nativeObj ;

	nativeObj = (native_obj_t*)nobj ;

	if( nativeObj)
	{
		if( nativeObj->term)
		{
			ml_destroy_term( nativeObj->term) ;
		}

		if( nativeObj->listener)
		{
			(*env)->DeleteGlobalRef( env , nativeObj->listener) ;
		}

		free( nativeObj) ;
	}
}

JNIEXPORT void JNICALL
Java_mlterm_MLTermPty_nativeSetListener(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nobj ,
	jobject  listener
	)
{
	native_obj_t *  nativeObj ;

	nativeObj = nobj ;

	if( nativeObj->listener)
	{
		(*env)->DeleteGlobalRef( env , nativeObj->listener) ;
	}

	if( listener)
	{
		nativeObj->listener = (*env)->NewGlobalRef( env , listener) ;
	}
	else
	{
		nativeObj->listener = NULL ;
	}
}

JNIEXPORT jboolean JNICALL
Java_mlterm_MLTermPty_waitForReading(
	JNIEnv *  env ,
	jclass  class
	)
{
#if  defined(USE_WIN32API) && ! defined(USE_LIBSSH2)
	return  JNI_FALSE ;
#else
	u_int  count ;
	ml_term_t **  terms ;
	u_int  num_of_terms ;
	int  maxfd ;
	int  ptyfd ;
	fd_set  read_fds ;
#ifdef  USE_LIBSSH2
	struct timeval  tval ;
	int *  xssh_fds ;
	u_int  num_of_xssh_fds ;
#endif

	ml_close_dead_terms() ;
	num_of_terms = ml_get_all_terms( &terms) ;

	if( num_of_terms == 0)
	{
		return  JNI_FALSE ;
	}

#ifdef  USE_LIBSSH2
	num_of_xssh_fds = ml_pty_ssh_get_x11_fds( &xssh_fds) ;
#endif

	while( 1)
	{
	#ifdef  USE_LIBSSH2
		if( ml_pty_ssh_poll( &read_fds) > 0)
		{
			return  JNI_TRUE ;
		}
	#endif

		maxfd = 0 ;
		FD_ZERO( &read_fds) ;

		for( count = 0 ; count < num_of_terms ; count ++)
		{
			ptyfd = ml_term_get_master_fd( terms[count]) ;
			FD_SET( ptyfd , &read_fds) ;

			if( ptyfd > maxfd)
			{
				maxfd = ptyfd ;
			}
		}

	#ifdef  USE_LIBSSH2
		for( count = 0 ; count < num_of_xssh_fds ; count++)
		{
			FD_SET( xssh_fds[count] , &read_fds) ;

			if( xssh_fds[count] > maxfd)
			{
				maxfd = xssh_fds[count] ;
			}
		}

		tval.tv_usec = 0 ;
		tval.tv_sec = keepalive_interval ;

		if( select( maxfd + 1 , &read_fds , NULL , NULL ,
				keepalive_interval > 0 ? &tval : NULL) == 0)
		{
			ml_pty_ssh_keepalive( keepalive_interval * 1000) ;
		}
		else
	#else
		select( maxfd + 1 , &read_fds , NULL , NULL , NULL) ;
	#endif
		{
			break ;
		}
	}

	return  JNI_TRUE ;
#endif
}

JNIEXPORT jboolean JNICALL
Java_mlterm_MLTermPty_nativeIsActive(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj
	)
{
	ml_close_dead_terms() ;

	if( nativeObj && ((native_obj_t*)nativeObj)->term)
	{
		return  JNI_TRUE ;
	}
	else
	{
		return  JNI_FALSE ;
	}
}

JNIEXPORT jboolean JNICALL
Java_mlterm_MLTermPty_nativeRead(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nObj
	)
{
	native_obj_t *  nativeObj ;

	nativeObj = nObj ;

	if( nativeObj && nativeObj->term)
	{
		int  ret ;

	#ifdef  USE_LIBSSH2
		u_int  count ;
		int *  xssh_fds ;

		for( count = ml_pty_ssh_get_x11_fds( &xssh_fds) ; count > 0 ; count--)
		{
			ml_pty_ssh_send_recv_x11( count - 1 , 1) ;
		}
	#endif

		/* For event listeners of ml_term_t. */
		nativeObj->env = env ;
		nativeObj->obj = obj ;

		draw_cursor( nativeObj->term , ml_line_restore_color) ;
		ret = ml_term_parse_vt100_sequence( nativeObj->term) ;
		draw_cursor( nativeObj->term , ml_line_reverse_color) ;

		if( ret)
		{
		#if  0	/* #ifdef  TUNEUP_HACK */
			u_int  row ;
			u_int  num_of_skip ;
			u_int  num_of_rows ;
			u_int  num_of_mod ;
			int  prev_is_modified ;
			ml_line_t *  line ;

			prev_is_modified = 0 ;
			num_of_skip = 0 ;
			num_of_mod = 0 ;
			num_of_rows = ml_term_get_rows( nativeObj->term) ;
			for( row = 0 ; row < num_of_rows ; row++)
			{
				if( ( line = ml_term_get_line( nativeObj->term , row)) &&
				    ml_line_is_modified( line))
				{
					if( ! prev_is_modified)
					{
						num_of_skip ++ ;
					}
					prev_is_modified = 1 ;

					num_of_mod ++ ;
				}
				else if( prev_is_modified)
				{
					prev_is_modified = 0 ;
				}
			}

			/*
			 * If 80% of lines are modified, set modified flag to all lines
			 * to decrease the number of calling replaceTextRange().
			 */
			if( num_of_skip > 2 && num_of_mod * 5 / 4 > num_of_rows)
			{
				for( row = 0 ; row < num_of_rows ; row++)
				{
					if( ( line = ml_term_get_line( nativeObj->term , row)))
					{
						ml_line_set_modified_all( line) ;
					}
				}
			}
		#endif

			return  JNI_TRUE ;
		}
	}

	return  JNI_FALSE ;
}

JNIEXPORT jboolean JNICALL
Java_mlterm_MLTermPty_nativeWrite(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj ,
	jstring  jstr
	)
{
	char *  str ;
	u_char  buf[128] ;
	size_t  len ;

	if( ! nativeObj || ! ((native_obj_t*)nativeObj)->term)
	{
		return  JNI_FALSE ;
	}

	str = (*env)->GetStringUTFChars( env , jstr , NULL) ;
#if  0
	kik_debug_printf( "WRITE TO PTY: %x" , (int)*str) ;
#endif

	/* In case local echo in ml_term_write(). */
	draw_cursor( ((native_obj_t*)nativeObj)->term , ml_line_restore_color) ;

	if( *str == '\0')
	{
		/* Control+space */
		ml_term_write( ((native_obj_t*)nativeObj)->term , str , 1) ;
	}
	else
	{
		(*utf8_parser->init)( utf8_parser) ;
		(*utf8_parser->set_str)( utf8_parser , str , strlen( str)) ;
		while( ! utf8_parser->is_eos &&
		       ( len = ml_term_convert_to( ((native_obj_t*)nativeObj)->term ,
					buf , sizeof(buf) , utf8_parser)) > 0)
		{
			ml_term_write( ((native_obj_t*)nativeObj)->term , buf , len) ;
		}

	#if  0
		kik_debug_printf( " => DONE\n") ;
	#endif
		(*env)->ReleaseStringUTFChars( env , jstr , str) ;
	}

	draw_cursor( ((native_obj_t*)nativeObj)->term , ml_line_reverse_color) ;

	return  JNI_TRUE ;
}

JNIEXPORT jboolean JNICALL
Java_mlterm_MLTermPty_nativeWriteModifiedKey(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj ,
	jint  key ,
	jint  modcode
	)
{
	if( ml_term_write_modified_key( ((native_obj_t*)nativeObj)->term , key , modcode))
	{
		return  JNI_TRUE ;
	}
	else
	{
		return  JNI_FALSE ;
	}
}

JNIEXPORT jboolean JNICALL
Java_mlterm_MLTermPty_nativeWriteSpecialKey(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj ,
	jint  key ,
	jint  modcode
	)
{
	ml_special_key_t  spkey ;
	u_int8_t  keys[] =
	{
		SPKEY_UP ,
		SPKEY_DOWN ,
		SPKEY_LEFT ,
		SPKEY_RIGHT ,
		SPKEY_PRIOR ,
		SPKEY_NEXT ,
		SPKEY_HOME ,
		SPKEY_END ,
		SPKEY_INSERT ,
		SPKEY_F1 ,
		SPKEY_F2 ,
		SPKEY_F3 ,
		SPKEY_F4 ,
		SPKEY_F5 ,
		SPKEY_F6 ,
		SPKEY_F7 ,
		SPKEY_F8 ,
		SPKEY_F9 ,
		SPKEY_F10 ,
		SPKEY_F11 ,
		SPKEY_F12 ,
		SPKEY_F13 ,
		SPKEY_F14 ,
		SPKEY_F15 ,
		SPKEY_F16 ,
		SPKEY_F17 ,
		SPKEY_F18 ,
		SPKEY_F19 ,
		SPKEY_F20 ,
	} ;

	u_int8_t  keypad_keys[] =
	{
		SPKEY_KP_MULTIPLY ,
		SPKEY_KP_ADD ,
		SPKEY_KP_MULTIPLY ,
		SPKEY_KP_ADD ,
		SPKEY_KP_SUBTRACT ,
		SPKEY_KP_DELETE ,
		SPKEY_KP_DIVIDE ,
		SPKEY_KP_INSERT ,	/* 0 */
		SPKEY_KP_END ,		/* 1 */
		SPKEY_KP_DOWN ,		/* 2 */
		SPKEY_KP_NEXT ,		/* 3 */
		SPKEY_KP_LEFT ,		/* 4 */
		SPKEY_KP_BEGIN ,	/* 5 */
		SPKEY_KP_RIGHT ,	/* 6 */
		SPKEY_KP_HOME ,		/* 7 */
		SPKEY_KP_UP ,		/* 8 */
		SPKEY_KP_PRIOR ,	/* 9 */
	} ;

	/* Definitions in SWT.java */
	enum
	{
		KEYCODE_BIT = (1 << 24) ,
		ARROW_UP = KEYCODE_BIT + 1 ,
		ARROW_DOWN = KEYCODE_BIT + 2 ,
		ARROW_LEFT = KEYCODE_BIT + 3 ,
		ARROW_RIGHT = KEYCODE_BIT + 4 ,
		PAGE_UP = KEYCODE_BIT + 5 ,
		PAGE_DOWN = KEYCODE_BIT + 6 ,
		HOME = KEYCODE_BIT + 7 ,
		END = KEYCODE_BIT + 8 ,
		INSERT = KEYCODE_BIT + 9 ,
		F1 = KEYCODE_BIT + 10 ,
		F2 = KEYCODE_BIT + 11 ,
		F3 = KEYCODE_BIT + 12 ,
		F4 = KEYCODE_BIT + 13 ,
		F5 = KEYCODE_BIT + 14 ,
		F6 = KEYCODE_BIT + 15 ,
		F7 = KEYCODE_BIT + 16 ,
		F8 = KEYCODE_BIT + 17 ,
		F9 = KEYCODE_BIT + 18 ,
		F10 = KEYCODE_BIT + 19 ,
		F11 = KEYCODE_BIT + 20 ,
		F12 = KEYCODE_BIT + 21 ,
		F13 = KEYCODE_BIT + 22 ,
		F14 = KEYCODE_BIT + 23 ,
		F15 = KEYCODE_BIT + 24 ,
		F16 = KEYCODE_BIT + 25 ,
		F17 = KEYCODE_BIT + 26 ,
		F18 = KEYCODE_BIT + 27 ,
		F19 = KEYCODE_BIT + 28 ,
		F20 = KEYCODE_BIT + 29 ,

		KEYPAD_MULTIPLY = KEYCODE_BIT + 42 ,
		KEYPAD_ADD = KEYCODE_BIT + 43 ,
		KEYPAD_SUBTRACT = KEYCODE_BIT + 45 ,
		KEYPAD_DECIMAL = KEYCODE_BIT + 46 ,
		KEYPAD_DIVIDE = KEYCODE_BIT + 47 ,
		KEYPAD_0 = KEYCODE_BIT + 48 ,
		KEYPAD_1 = KEYCODE_BIT + 49 ,
		KEYPAD_2 = KEYCODE_BIT + 50 ,
		KEYPAD_3 = KEYCODE_BIT + 51 ,
		KEYPAD_4 = KEYCODE_BIT + 52 ,
		KEYPAD_5 = KEYCODE_BIT + 53 ,
		KEYPAD_6 = KEYCODE_BIT + 54 ,
		KEYPAD_7 = KEYCODE_BIT + 55 ,
		KEYPAD_8 = KEYCODE_BIT + 56 ,
		KEYPAD_9 = KEYCODE_BIT + 57 ,
		KEYPAD_EQUAL = KEYCODE_BIT + 61 ,
		KEYPAD_CR = KEYCODE_BIT + 80 ,
		HELP = KEYCODE_BIT + 81 ,
		CAPS_LOCK = KEYCODE_BIT + 82 ,
		NUM_LOCK = KEYCODE_BIT + 83 ,
		SCROLL_LOCK = KEYCODE_BIT + 84 ,
		PAUSE = KEYCODE_BIT + 85 ,
		BREAK = KEYCODE_BIT + 86 ,
		PRINT_SCREEN = KEYCODE_BIT + 87 ,
	} ;

	if( ARROW_UP <= key && key <= F20)
	{
		spkey = keys[key - ARROW_UP] ;
	}
	else if( KEYPAD_MULTIPLY <= key && key <= KEYPAD_9)
	{
		spkey = keypad_keys[key - KEYPAD_MULTIPLY] ;
	}
	else if( key == '\x1b')
	{
		spkey = SPKEY_ESCAPE ;
	}
	else
	{
		return  JNI_FALSE ;
	}

	if( ml_term_write_special_key( ((native_obj_t*)nativeObj)->term ,
			spkey , modcode , 0))
	{
		return  JNI_TRUE ;
	}
	else
	{
		return  JNI_FALSE ;
	}
}

JNIEXPORT jboolean JNICALL
Java_mlterm_MLTermPty_nativeResize(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj ,
	jint  cols ,
	jint  rows
	)
{
	if( nativeObj && ((native_obj_t*)nativeObj)->term &&
	    ml_term_resize( ((native_obj_t*)nativeObj)->term , cols , rows , 0 , 0))
	{
		return  JNI_TRUE ;
	}
	else
	{
		return  JNI_FALSE ;
	}
}

JNIEXPORT jboolean JNICALL
Java_mlterm_MLTermPty_nativeGetRedrawString(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj ,
	jint  row ,
	jobject  region
	)
{
	ml_line_t *  line ;
	char *  buf ;
	size_t  buf_len ;
	int  mod_beg ;
	u_int  num_of_chars ;
	u_int  count ;
	u_int  start ;
	size_t  redraw_len ;
	jobject *  styles ;
	u_int  num_of_styles ;
	static jfieldID  region_str ;
	static jfieldID  region_start ;
	static jfieldID  region_styles ;
	static jclass  style_class ;
	static jfieldID  style_length ;
	static jfieldID  style_start ;
	static jfieldID  style_fg_color ;
	static jfieldID  style_fg_pixel ;
	static jfieldID  style_bg_color ;
	static jfieldID  style_bg_pixel ;
	static jfieldID  style_underline ;
	static jfieldID  style_strikeout ;
	static jfieldID  style_bold ;
	static jfieldID  style_italic ;
	jobjectArray  array ;

	if( ! nativeObj || ! ((native_obj_t*)nativeObj)->term ||
	    ! ( line = ml_term_get_line( ((native_obj_t*)nativeObj)->term , row)) ||
	    ! ml_line_is_modified( line))
	{
		return  JNI_FALSE ;
	}

	if( ! region_str)
	{
		jclass  class ;

		class = (*env)->FindClass( env , "mlterm/RedrawRegion") ;
		region_str = (*env)->GetFieldID( env , class , "str" , "Ljava/lang/String;") ;
		region_start = (*env)->GetFieldID( env , class , "start" , "I") ;
		region_styles = (*env)->GetFieldID( env , class , "styles" , "[Lmlterm/Style;") ;
	}

	mod_beg = ml_line_get_beg_of_modified( line) ;
#if  0
	num_of_chars = ml_line_get_num_of_redrawn_chars( line , 1 /* to end */) ;
#else
	if( ( num_of_chars = get_num_of_filled_chars_except_spaces( line)) >= mod_beg)
	{
		num_of_chars -= mod_beg ;
	}
	else
	{
		num_of_chars = 0 ;
	}
#endif

	buf_len = (mod_beg + num_of_chars) * sizeof(int16_t) * 2 /* SURROGATE_PAIR */
			+ 2 /* NULL */ ;
	buf = alloca( buf_len) ;

	num_of_styles = 0 ;

	if( mod_beg > 0)
	{
		(*str_parser->init)( str_parser) ;
		ml_str_parser_set_str( str_parser , line->chars , mod_beg) ;
		start = (*utf16_conv->convert)( utf16_conv ,
				buf , buf_len , str_parser) / 2 ;
	}
	else
	{
		start = 0 ;
	}

	if( num_of_chars > 0)
	{
		styles = alloca( sizeof(jobject) * num_of_chars) ;
		redraw_len = 0 ;
		(*str_parser->init)( str_parser) ;
		for( count = 0 ; count < num_of_chars ; count++)
		{
			size_t  len ;
			int  ret ;

		#if  0
			if( ml_char_code_equal( line->chars + mod_beg + count , ml_nl_ch()))
			{
				/* Drawing will collapse, but region.str mustn't contain '\n'. */
				continue ;
			}
		#endif

			ml_str_parser_set_str( str_parser , line->chars + mod_beg + count , 1) ;
			if( ( len = (*utf16_conv->convert)( utf16_conv , buf + redraw_len ,
						buf_len - redraw_len , str_parser)) < 2)
			{
				continue ;
			}

			ret = need_style( line->chars + mod_beg + count ,
					count > 0 ? line->chars + mod_beg + count - 1 : NULL) ;
			if( ret == 1)
			{
				(*env)->SetIntField( env , styles[num_of_styles - 1] ,
						style_length ,
						(*env)->GetIntField( env ,
							styles[num_of_styles - 1] , style_length)
							+ len / 2) ;
			}
			else if( ret == 2)
			{
				ml_color_t  color ;
				u_int8_t  red ;
				u_int8_t  green ;
				u_int8_t  blue ;

				if( ! style_class)
				{
					style_class = (*env)->NewGlobalRef( env ,
							(*env)->FindClass( env , "mlterm/Style")) ;
					style_length = (*env)->GetFieldID( env , style_class ,
								"length" , "I") ;
					style_start = (*env)->GetFieldID( env , style_class ,
								"start" , "I") ;
					style_fg_color = (*env)->GetFieldID( env , style_class ,
								"fg_color" , "I") ;
					style_fg_pixel = (*env)->GetFieldID( env , style_class ,
								"fg_pixel" , "I") ;
					style_bg_color = (*env)->GetFieldID( env , style_class ,
								"bg_color" , "I") ;
					style_bg_pixel = (*env)->GetFieldID( env , style_class ,
								"bg_pixel" , "I") ;
					style_underline = (*env)->GetFieldID( env , style_class ,
								"underline" , "Z") ;
					style_strikeout = (*env)->GetFieldID( env , style_class ,
								"strikeout" , "Z") ;
					style_bold = (*env)->GetFieldID( env , style_class ,
								"bold" , "Z") ;
					style_italic = (*env)->GetFieldID( env , style_class ,
								"italic" , "Z") ;
				}

				styles[num_of_styles++] = (*env)->AllocObject( env , style_class) ;

				(*env)->SetIntField( env , styles[num_of_styles - 1] ,
					style_length , len / 2) ;

				(*env)->SetIntField( env , styles[num_of_styles - 1] ,
					style_start , redraw_len / 2) ;

				color = ml_char_fg_color( line->chars + mod_beg + count) ;
				(*env)->SetIntField( env , styles[num_of_styles - 1] ,
					style_fg_color , color) ;
				(*env)->SetIntField( env , styles[num_of_styles - 1] ,
					style_fg_pixel ,
					/* return -1(white) for invalid color. */
					ml_get_color_rgba( color , &red , &green , &blue , NULL) ?
						((red << 16) | (green << 8) | blue) : -1) ;

				color = ml_char_bg_color( line->chars + mod_beg + count) ;
				(*env)->SetIntField( env , styles[num_of_styles - 1] ,
					style_bg_color , color) ;
				(*env)->SetIntField( env , styles[num_of_styles - 1] ,
					style_bg_pixel ,
					/* return -1(white) for invalid color. */
					ml_get_color_rgba( color , &red , &green , &blue , NULL) ?
						((red << 16) | (green << 8) | blue) : -1) ;

				(*env)->SetBooleanField( env , styles[num_of_styles - 1] ,
					style_underline ,
					ml_char_underline_style( line->chars + mod_beg + count) ?
						JNI_TRUE : JNI_FALSE) ;

				(*env)->SetBooleanField( env , styles[num_of_styles - 1] ,
					style_strikeout ,
					ml_char_is_crossed_out( line->chars + mod_beg + count) ?
						JNI_TRUE : JNI_FALSE) ;

				(*env)->SetBooleanField( env , styles[num_of_styles - 1] ,
					style_bold ,
					ml_char_font( line->chars + mod_beg + count) & FONT_BOLD ?
						JNI_TRUE : JNI_FALSE) ;

				(*env)->SetBooleanField( env , styles[num_of_styles - 1] ,
					style_italic ,
					ml_char_font( line->chars + mod_beg + count) &
						FONT_ITALIC ? JNI_TRUE : JNI_FALSE) ;
			}

			redraw_len += len ;
		}
	}
	else
	{
		redraw_len = 0 ;
	}

#ifdef  WORDS_BIGENDIAN
	buf[redraw_len++] = '\0' ;
	buf[redraw_len++] = '\n' ;
#else
	buf[redraw_len++] = '\n' ;
	buf[redraw_len++] = '\0' ;
#endif

	(*env)->SetObjectField( env , region , region_str ,
			((*env)->NewString)( env , buf , redraw_len / 2)) ;
	(*env)->SetIntField( env , region , region_start , start) ;

	if( num_of_styles > 0)
	{
		u_int  count ;

		array = (*env)->NewObjectArray( env , num_of_styles , style_class , styles[0]) ;
		for( count = 1 ; count < num_of_styles ; count++)
		{
			(*env)->SetObjectArrayElement( env , array , count , styles[count]) ;
		}
	}
	else
	{
		array = NULL ;
	}

	(*env)->SetObjectField( env , region , region_styles , array) ;

	ml_line_set_updated( line) ;

#ifdef  TUNEUP_HACK
	{
		/*
		 * XXX
		 * It is assumed that lines are sequentially checked from 0th row.
		 * If next line is modified, set mod_beg = 0 to enable combining modified
		 * characters in current line and next line.
		 */

		ml_line_t *  next_line ;

		if( ( next_line = ml_term_get_line( ((native_obj_t*)nativeObj)->term , row + 1)) &&
		    ml_line_is_modified( next_line) &&
		    ml_line_get_beg_of_modified( next_line) <
			ml_term_get_cols( ((native_obj_t*)nativeObj)->term) / 2)
		{
			ml_line_set_modified_all( next_line) ;
		}
	}
#endif

	return  JNI_TRUE ;
}

JNIEXPORT jint JNICALL
Java_mlterm_MLTermPty_nativeGetRows(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj
	)
{
	if( ! nativeObj || ! ((native_obj_t*)nativeObj)->term)
	{
		return  0 ;
	}

	return  ml_term_get_rows( ((native_obj_t*)nativeObj)->term) ;
}

JNIEXPORT jint JNICALL
Java_mlterm_MLTermPty_nativeGetCols(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj
	)
{
	if( ! nativeObj || ! ((native_obj_t*)nativeObj)->term)
	{
		return  0 ;
	}

	return  ml_term_get_cols( ((native_obj_t*)nativeObj)->term) ;
}

JNIEXPORT jint JNICALL
Java_mlterm_MLTermPty_nativeGetCaretRow(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj
	)
{
	if( ! nativeObj || ! ((native_obj_t*)nativeObj)->term)
	{
		return  0 ;
	}

	return  ml_term_cursor_row_in_screen( ((native_obj_t*)nativeObj)->term) ;
}

JNIEXPORT jint JNICALL
Java_mlterm_MLTermPty_nativeGetCaretCol(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nativeObj
	)
{
	ml_line_t *  line ;
	int  char_index ;

	if( ! nativeObj || ! ((native_obj_t*)nativeObj)->term ||
	    ! ( line = ml_term_get_cursor_line( ((native_obj_t*)nativeObj)->term)))
	{
		return  0 ;
	}

	char_index = ml_term_cursor_char_index( ((native_obj_t*)nativeObj)->term) ;

	if( char_index > 0)
	{
		char *  buf ;
		size_t  buf_len ;

		buf_len = char_index * sizeof(int16_t) * 2 /* SURROGATE_PAIR */ ;
		buf = alloca( buf_len) ;

		(*str_parser->init)( str_parser) ;
		ml_str_parser_set_str( str_parser , line->chars , char_index) ;

		return  (*utf16_conv->convert)( utf16_conv ,
				buf , buf_len , str_parser) / 2 ;
	}
	else
	{
		return  0 ;
	}
}

JNIEXPORT jboolean JNICALL
Java_mlterm_MLTermPty_nativeIsTrackingMouse(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nobj ,
	jint  button ,
	jboolean  isMotion
	)
{
	native_obj_t *  nativeObj ;

	nativeObj = nobj ;

	if( ! nativeObj || ! nativeObj->term ||
	    ! ml_term_get_mouse_report_mode( nativeObj->term) ||
	    ( isMotion &&
	      ( ml_term_get_mouse_report_mode( nativeObj->term) < BUTTON_EVENT_MOUSE_REPORT ||
	        ( button == 0 && ml_term_get_mouse_report_mode( nativeObj->term) ==
		                   BUTTON_EVENT_MOUSE_REPORT))) )
	{
		return  JNI_FALSE ;
	}
	else
	{
		return  JNI_TRUE ;
	}
}

JNIEXPORT void JNICALL
Java_mlterm_MLTermPty_nativeReportMouseTracking(
	JNIEnv *  env ,
	jobject  obj ,
	jlong  nobj ,
	jint  char_index ,
	jint  row ,
	jint  button ,
	jint  state ,
	jboolean  isMotion ,
	jboolean  isReleased
	)
{
	native_obj_t *  nativeObj ;
	ml_line_t *  line ;
	int  col ;
	int  key_state ;

	nativeObj = nobj ;

#if  0
	if( ! Java_mlterm_MLTermPty_nativeIsTrackingMouse( env , obj , nobj ,
				button , state , isMotion , isReleased))
	{
		return ;
	}
#endif

	if( ml_term_get_mouse_report_mode( nativeObj->term) >= LOCATOR_CHARCELL_REPORT)
	{
		/* Not supported */

		return ;
	}

	/*
	 * XXX
	 * Not considering BiDi etc.
	 */

	if( ! ( line = ml_term_get_line( nativeObj->term , row)))
	{
		col = char_index ;
	}
	else
	{
		int  count ;

		if( ml_line_end_char_index( line) < char_index)
		{
			col = char_index - ml_line_end_char_index( line) ;
			char_index -= col ;
		}
		else
		{
			col = 0 ;
		}

		for( count = 0 ; count < char_index ; count++)
		{
			u_int  size ;

			col += ml_char_cols( line->chars + count) ;

			if( ml_get_combining_chars( line->chars + count , &size))
			{
				char_index -= size ;
			}
		}
	}

	/*
	 * Following is the same as x_screen.c:report_mouse_tracking().
	 */

	if( /* isMotion && */ button == 0)
	{
		/* PointerMotion */
		key_state = 0 ;
	}
	else
	{
		/*
		 * Shift = 4
		 * Meta = 8
		 * Control = 16
		 * Button Motion = 32
		 *
		 * NOTE: with Ctrl/Shift, the click is interpreted as region selection at present.
		 * So Ctrl/Shift will never be catched here.
		 */
		key_state = ((state & ShiftMask) ? 4 : 0) +
				((state & AltMask) ? 8 : 0) +
				((state & ControlMask) ? 16 : 0) +
				(isMotion ? 32 : 0) ;
	}

	/* count starts from 1, not 0 */
	col ++ ;
	row ++ ;

	if( isMotion &&
	    button <= 3 &&	/* not wheel mouse */
	    nativeObj->prev_mouse_report_col == col &&
	    nativeObj->prev_mouse_report_row == row)
	{
		/* Pointer is not moved. */
		return ;
	}

	ml_term_report_mouse_tracking( nativeObj->term , col , row , button ,
		isReleased , key_state , 0) ;

	nativeObj->prev_mouse_report_col = col ;
	nativeObj->prev_mouse_report_row = row ;
}

JNIEXPORT jlong JNICALL
Java_mlterm_MLTermPty_getColorRGB(
	JNIEnv *  env ,
	jclass  class ,
	jstring  jstr_color
	)
{
	const char *  color_name ;
	ml_color_t  color ;
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	int  error ;

	color_name = (*env)->GetStringUTFChars( env , jstr_color , NULL) ;

	error = 0 ;

	/*
	 * The similar processing as that of x_load_named_xcolor()
	 * in fb/x_color.c and win32/x_color.c.
	 */

	if( ml_color_parse_rgb_name( &red , &green , &blue , NULL , color_name))
	{
		/* do nothing */
	}
	else if( ( color = ml_get_color( color_name)) != ML_UNKNOWN_COLOR &&
	         IS_VTSYS_BASE_COLOR(color))
	{
		/*
		 * 0 : 0x00, 0x00, 0x00
		 * 1 : 0xff, 0x00, 0x00
		 * 2 : 0x00, 0xff, 0x00
		 * 3 : 0xff, 0xff, 0x00
		 * 4 : 0x00, 0x00, 0xff
		 * 5 : 0xff, 0x00, 0xff
		 * 6 : 0x00, 0xff, 0xff
		 * 7 : 0xe5, 0xe5, 0xe5
		 */
		red = (color & 0x1) ? 0xff : 0 ;
		green = (color & 0x2) ? 0xff : 0 ;
		blue = (color & 0x4) ? 0xff : 0 ;
	}
	else
	{
		if( strcmp( color_name , "gray") == 0)
		{
			red = green = blue = 190 ;
		}
		else if( strcmp( color_name , "lightgray") == 0)
		{
			red = green = blue = 211 ;
		}
		else
		{
			error = 1 ;
		}
	}

	(*env)->ReleaseStringUTFChars( env , jstr_color , color_name) ;

	if( ! error)
	{
		return  ((red << 16) | (green << 8) | blue) ;
	}
	else
	{
		return  -1 ;
	}
}
