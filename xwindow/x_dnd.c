/** @file
 * @brief X Drag and Drop protocol support
 * 
 *	$Id$
 */

#include  "x_window.h"
#include  "x_dnd.h"
/*
#include  <ctype.h>
*/
#include  <X11/Xatom.h>
#include  <X11/Xutil.h>
#include  <mkf/mkf_utf8_conv.h>
#include  <mkf/mkf_utf16_parser.h>

/* XXX Should we create x_atom.h ?? */
#define  XA_COMPOUND_TEXT(display)  (XInternAtom(display , "COMPOUND_TEXT" , False))
#define  XA_TEXT(display)  (XInternAtom( display , "TEXT" , False))
#define  XA_UTF8_STRING(display)  (XInternAtom(display , "UTF8_STRING" , False))
#define  XA_INCR(display) (XInternAtom(display, "INCR", False))

#define  XA_DND_MIME_TEXT_PLAIN(display) (XInternAtom(display, "text/plain", False))
#define  XA_DND_MIME_TEXT_UNICODE(display) (XInternAtom(display, "text/unicode", False))
#define  XA_DND_MIME_TEXT_URI_LIST(display) (XInternAtom(display, "text/uri-list", False))

typedef struct dnd_parser {
	char *  atomname ;
	int  (*parser)(x_window_t *, unsigned char *, int);
} dnd_parser_t ;

static int
parse_text_plain(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	if( !(win->utf8_selection_notified))
		return 0 ; /* needs ASCII capable parser*/
	(*win->utf8_selection_notified)( win , src , len) ;

	return 1 ;
}

static int
parse_text_unicode(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	int filled_len ;
	mkf_parser_t * parser ;
	mkf_conv_t * conv ;
	char conv_buf[512] = {0};
	
	if( !(win->utf8_selection_notified))
		return 0 ;
	if( !(conv = mkf_utf8_conv_new()))
		return 0 ;
	if( !(parser = mkf_utf16_parser_new()))
	{
		(conv->delete)(conv) ;
		return 0 ;
	}
	
	(parser->init)( parser) ;
	if ( (src[0] == 0xFF || src[0] == 0xFE)
	     && (src[1] == 0xFF || src[1] == 0xFE)
	     && (src[0] != src[1]))
	{			
		/* src sequence seems to have a valid BOM and *
		 * should be processed correctly */
	}
	else
	{
		/* try to set parser state depending on 
		   your machine's endianess by sending BOM */
		u_int16_t BOM[] =  {0xFEFF};
		
		(parser->set_str)( parser , (char *)BOM , 2) ;
		(parser->next_char)( parser , 0) ;	     
	}
	
	(parser->set_str)( parser , src , len) ;
	/* conversion from utf16 -> utf8. */
	while( ! parser->is_eos)
	{
		filled_len = (conv->convert)( conv,
					      conv_buf,
					      sizeof(conv_buf),
					      parser) ;
		if(filled_len ==0) 
			break ;
		(*win->utf8_selection_notified)( win,
						 conv_buf, 
						 filled_len) ;
	}
	(conv->delete)( conv) ;
	(parser->delete)( parser) ;
	
	return 1 ;
}

static int
parse_text_uri_list(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	int pos ;
	unsigned char *delim ;
	
	if( !(win->utf8_selection_notified))
		return 0 ;
	pos = 0 ;
	delim = src ;
	while( pos < len){
		delim = strchr( &(src[pos]), 13) ;
		if( delim )
		{
			/* output one ' ' as a separator */
			*delim = ' ' ; 
		}
		else
		{
			delim = &(src[len -1]) ;
		}
		if( strncmp( &(src[pos]), "file:",5) == 0)
		{
			/* remove garbage("file:").
			   new length should be (length - "file:" + " ")*/
			(*win->utf8_selection_notified)( win ,
							 &(src[pos+5]),
							 (delim - &(src[pos])) -5 +1 ) ;
		}
		else
		{
			/* original string + " " */
			(*win->utf8_selection_notified)( win ,
							 &(src[pos]),
							 (delim - &(src[pos])) +1) ;
		}
		/* skip trailing 0x0A */
		pos = (delim - src) +2 ; 
	}
	
	return 1;
}

static int
parse_compound_text(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	if( !(win->xct_selection_notified))
		return 0 ;
	(*win->xct_selection_notified)( win , src , len) ;

	return 1 ;
}

static int
parse_text(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
		if( !(win->utf8_selection_notified))
			return 0 ;
		(*win->utf8_selection_notified)( win , src , len) ;
		return 1 ;
}

static int
parse_utf8_string(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	if( !(win->utf8_selection_notified))
		return 0 ;
	(*win->utf8_selection_notified)( win , src , len) ;
	return 1 ;
}

static int
parse_mlterm_config(
	x_window_t *  win,
	unsigned char *  src,
	int  len)
{
	char *  value ;	
	
	if( !(win->config_listener))
		return 0 ;
	value = strchr( src, '=') ;
	if( !value)
		return 0 ;
	*value = 0 ;
	(*win->config_listener)( win , 
				 NULL, /* dev */
				 src, /* key */
				 value +1 /* value */) ;

	return 1 ;
}

dnd_parser_t dnd_parsers[] ={
	{"text/plain"   , parse_text_plain } ,
	{"text/unicode"   , parse_text_unicode } ,
	{"text/uri-list", parse_text_uri_list } ,
	{"COMPOUND_TEXT", parse_compound_text } ,
	{"TEXT"         , parse_text } ,
	{"UTF8_STRING"  , parse_utf8_string } ,
	{"text/x-mlterm.config"  , parse_mlterm_config } ,
	{NULL, NULL}
};

/* --- static variables --- */



/* --- static functions --- */
static int
ignore_badwin(
	Display *  display,
	XErrorEvent *  event
	)
{
	char  buffer[1024] ;

	XGetErrorText( display , event->error_code , buffer , 1024) ;

	kik_error_printf( "%s\n" , buffer) ;
	
	switch( event->error_code)
	{
	case BadWindow:
		return 1;
	default:
		abort() ;
	}
}

static void
set_badwin_handler(
	int  flag
	)
{
	static XErrorHandler  old;

	if( flag)
	{
		if( !old)
			old = XSetErrorHandler( ignore_badwin) ;
#ifdef  DEBUG
		else
			kik_debug_printf("handler aready istalled\n") ;
#endif
	}
	else
	{
		if( old)
		{
			XSetErrorHandler( old) ;
			old = NULL ;
		}
#ifdef  DEBUG
		else
		{
			kik_debug_printf("no handler had been stored\n") ;
		}
#endif
	}
}

/* seek atom array and return the index */
static int
is_pref(
	Atom type,
	Atom * atom,
	int num
	)
{
	int i ;
	for( i = 0 ; i < num ; i++)
		if( atom[i] == type)
			return i ;

	return  -1 ;
}

/**send a accept/reject message to the dnd sender
 *\param win mlterm window
 */
static void
reply(
	x_window_t * win
	)
{
	XClientMessageEvent  msg ;
	
	msg.type = ClientMessage ;
	msg.display = win->display ;
	msg.format = 32 ;
	msg.window = win->dnd->source ;
	msg.message_type = XInternAtom( win->display, "XdndStatus", False) ;
	msg.data.l[0] = win->my_window ;
	if( win->dnd->waiting_atom)
	{
		msg.data.l[1] = 0x1 | 0x2 ; /* accept the drop | use [2][3] */
		msg.data.l[2] = 0 ;
		msg.data.l[3] = 0 ;
		msg.data.l[4] = XInternAtom( win->display, "XdndActionCopy", False) ;
	}
	else
	{
		msg.data.l[1] = 0 ;
		msg.data.l[2] = 0 ;
		msg.data.l[3] = 0 ;
		msg.data.l[4] = 0 ;
	}

	set_badwin_handler(1) ;	
	XSendEvent(win->display, msg.window, False, 0, (XEvent*)&msg);
	set_badwin_handler(0) ;
}

/**send finish message to dnd sender
 *\param win mlterm window 
 */
static void
finish(
	x_window_t *  win 
	)
{
	XClientMessageEvent msg ;
	
	if( !(win->dnd->source))
		return ;

	msg.message_type = XInternAtom( win->display, "XdndFinished", False) ;
	msg.data.l[0] = win->my_window ;
	/* setting bit 0 means success */
	msg.data.l[1] = 1 ; 
	msg.data.l[2] = XInternAtom( win->display, "XdndActionCopy", False) ;
	msg.type = ClientMessage ;
	msg.format = 32 ;
	msg.window = win->dnd->source ;
	msg.display = win->display ;

	set_badwin_handler(1) ;
	XSendEvent(win->display, win->dnd->source, False, 0, (XEvent*)&msg) ;
	set_badwin_handler(0) ;

	win->dnd->source = 0 ;
}

/**parse dnd data and send them to the pty
 *\param win mlterm window
 *\param atom type of data
 *\param src data from dnd
 *\param len size of data in bytes
 */
static int
parse(
	x_window_t * win,
	unsigned char *src,
	int len)
{
	dnd_parser_t *  proc_entry ;
	Atom atom ;

	atom = win->dnd->waiting_atom ;
	if( !src)
		return 1 ;
	if( !(win->dnd))
		return 1;
	    
	for( proc_entry = dnd_parsers ; proc_entry->atomname ;proc_entry++)
	{
		if( atom == XInternAtom( win->display, proc_entry->atomname, False) )
			break ;		    
	}
	
	if( proc_entry->parser)
		return  (proc_entry->parser)( win, src, len) ;

	return 0 ; 
}


/* i is used as an index for an array of atom.
 * i = -1 means "nothing"
 *
 * returned value is the atom found and NOT THE INDEX
 * and 0 means "nothing" 
 */

static Atom
choose_atom(
	x_window_t *  win ,
	Atom *  atom,
	int  num
	)
{
	dnd_parser_t *  proc_entry ;
	int  i = -1 ;
	
	for( proc_entry = dnd_parsers ; proc_entry->atomname ;proc_entry++)
	{
		if( (i = is_pref( XInternAtom( win->display, proc_entry->atomname, False), atom, num) >= 0))
			break ;	
	}
		
	if( i < 0)
	{
#ifdef  DEBUG
		char *  p ;
		for( i = 0 ; i < num ; i++)
			if( atom[i])
			{
				p = XGetAtomName( win->display, atom[i]);
				kik_debug_printf("dropped atoms: %s\n",
						 XGetAtomName( win->display,
							       atom[i])) ;
				XFree( p) ;
			}
#endif
		return (Atom)0  ;/* 0 would never be used for Atom */
	}
	else
	{
#ifdef  DEBUG
		char *  p ;
		p = XGetAtomName( win->display, atom[i]);
		if( p)
		{
			kik_debug_printf( "accepted as atom: %s(%d)\n",
					  p, atom[i]) ;
			XFree( p) ;
		}
#endif

		return atom[i] ;
	}
}

/* --- global functions --- */

/**set/reset the window's dnd awareness
 *\param win mlterm window
 *\param flag awareness is set when true
 */
void
x_dnd_set_awareness(
	x_window_t * win,
	int version
	)
{
	set_badwin_handler(1);
	XChangeProperty( win->display, win->my_window,
			 XInternAtom( win->display, "XdndAware", False),
			 XA_ATOM, 32, PropModeReplace, 
			 (unsigned char *)(&version), 1) ;
	set_badwin_handler(0);
}

int 
x_dnd_process_enter(
	x_window_t *  win,
	XEvent *  event
	)
{
	Atom  to_wait ;		

	/* more than 3 type is available? */
	if (event->xclient.data.l[1] & 0x01)
	{
		Atom  act_type;
		int  act_format;
		unsigned long  nitems, left ;
		Atom *  dat ;
		int  result ;
		
		set_badwin_handler(1) ;
		result = XGetWindowProperty( win->display, event->xclient.data.l[0],
					     XInternAtom( win->display, "XdndTypeList", False), 0L, 1024L,
					     False, XA_ATOM,
					     &act_type, &act_format, &nitems, &left,
					     (unsigned char **)&dat);
		set_badwin_handler(0) ;

		if( ( result == Success) &&
		    ( act_type != None))
		{
			to_wait = choose_atom( win , dat, nitems) ;
			XFree( dat);
		}
		else
		{
			to_wait = 0 ;
		}
	}
	else
	{
		/* less than 3 candiates */
		to_wait = choose_atom( win , (event->xclient.data.l)+2 , 3);
	}
	
	if( to_wait)
	{
		if( !(win->dnd))
		{
			win->dnd = malloc( sizeof( x_dnd_context_t)) ;
		}
		if( !(win->dnd))
		{
			return 1 ;
		}
		win->dnd->source = event->xclient.data.l[0];
		win->dnd->waiting_atom = to_wait;
	}
	else
	{
		if( win->dnd)
		{
			free( win->dnd);
			win->dnd = NULL ;
		}
		return 1 ;
	}

	return 0 ;
}

int 
x_dnd_process_position(
	x_window_t *  win,
	XEvent *  event
	)
{
	if( !(win->dnd))
	{
		return 1 ;
	}

	if( win->dnd->source != event->xclient.data.l[0])
	{
		/* reject malicious drop */
		free( win->dnd);
		win->dnd = NULL ;

		return 1 ;
	}

	reply( win);
	
	return 0 ;
}

int 
x_dnd_process_drop(
	x_window_t *  win,
	XEvent *  event
	)
{

	if( !(win->dnd))
		return 1 ;

	if( win->dnd->source != event->xclient.data.l[0])
	{
		/* reject malicious drop */
		free( win->dnd);
		win->dnd = NULL ;

		return 1 ;
	}
	
	/* data request */
	set_badwin_handler(1) ;
	XConvertSelection( win->display, XInternAtom( win->display, "XdndSelection", False),
			   win->dnd->waiting_atom, /* mime type */
			   XA_DND_STORE(win->display),
			   win->my_window,
			   event->xclient.data.l[2]);
	set_badwin_handler(0) ;

	return 0 ;
}

int 
x_dnd_process_incr(
	x_window_t *  win,
	XEvent *  event
	)
{
	u_long  bytes_after ;
	XTextProperty  ct ;
	int result ;

	if( !(win->dnd))
		return 1 ;

	/* dummy read to determine data length */
	set_badwin_handler(1) ;
	result = XGetWindowProperty( win->display , event->xproperty.window ,
				     event->xproperty.atom , 0 , 0 , False ,
				     AnyPropertyType , &ct.encoding , &ct.format ,
				     &ct.nitems , &bytes_after , &ct.value) ;
	set_badwin_handler(0) ;
	if( result != Success)
		return  1 ; 	

	/* the event should be ignored */
	if( ct.encoding == None)
		return  1 ;

	/* when ct.encoding != XA_INCR, 
	   delete will be read when SelectionNotify would arrive  */
	if( ct.encoding != XA_INCR(win->display))
		return  1 ;

	set_badwin_handler(1) ;
	result = XGetWindowProperty( win->display , event->xproperty.window ,
			    event->xproperty.atom , 0 , bytes_after , False ,
			    AnyPropertyType , &ct.encoding , &ct.format ,
			    &ct.nitems , &bytes_after , &ct.value) ; 
	set_badwin_handler(0) ;

	if( result != Success)
		return  1 ; 	

	if( ct.nitems > 0)
	{
		parse( win, ct.value, ct.nitems) ;
	}
	else
	{       /* all data have been received */
		finish( win) ;
		free( win->dnd);
		win->dnd = NULL ;
	}
	
	XFree( ct.value) ; 

	/* This delete will trigger the next update*/
	set_badwin_handler(1) ;
	XDeleteProperty( win->display, event->xproperty.window,
			 event->xproperty.atom) ;					
	set_badwin_handler(0) ;

	return 0 ;
}

int 
x_dnd_process_selection(
	x_window_t *  win,
	XEvent *  event
	)
{
	u_long  bytes_after ;
	XTextProperty  ct ;
	int  seg = 0 ;
	int  result ;

	if( !(win->dnd))
		return 1 ;

	/* dummy read to determine data length */	
	set_badwin_handler(1) ;
	result = XGetWindowProperty( win->display , event->xselection.requestor ,
				     event->xselection.property , 0 , 0 , False ,
				     AnyPropertyType , &ct.encoding , &ct.format ,
				     &ct.nitems , &bytes_after , &ct.value) ;
	set_badwin_handler(0) ;

	if( result != Success)
	{
		/* reject malicious drop */
		free( win->dnd);
		win->dnd = NULL ;

		return 1 ;		
	}
	if( ct.encoding == XA_INCR(win->display))
		return 0 ;

	while( bytes_after > 0)
	{
		set_badwin_handler(1) ;
		result = XGetWindowProperty( win->display , event->xselection.requestor ,
					     event->xselection.property , seg / 4 , 4096 , False ,
					     AnyPropertyType , &ct.encoding , &ct.format ,
					     &ct.nitems , &bytes_after , &ct.value) ;
		set_badwin_handler(0) ;

		if(result != Success)
			break ;
				
		parse( win, ct.value, ct.nitems) ;		
		XFree( ct.value) ;
		
		seg += ct.nitems ;
	}
	
	finish( win) ;

	free( win->dnd);
	win->dnd = NULL ;

	return  0 ;
}
