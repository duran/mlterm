/*
 *	$Id$
 */
/* XXX XDND stuff should be moved into separete module */

#include  "x_window.h"
#include  "x_dnd.h"
#include  <X11/Xatom.h>

#define DEBUG
/* --- static variables --- */

static const u_char  DND_VERSION = 4 ;

/* --- global functuions --- */


void
x_dnd_set_awareness(
	x_window_t * win,
	int flag
	)
{
	if( flag)
	{
		XChangeProperty(win->display, win->my_window, XA_DND_AWARE(win->display),
				XA_ATOM, 32, PropModeReplace, &DND_VERSION, 1) ;
	}
	else
	{
		XDeleteProperty(win->display, win->my_window, XA_DND_AWARE(win->display)) ;
	}
}

void
x_dnd_finish(
	x_window_t * win
	)
{
	XClientMessageEvent reply_msg;

	reply_msg.message_type = XA_DND_FINISH(win->display);
	reply_msg.data.l[0] = win->my_window;
	reply_msg.data.l[1] = 0;
	reply_msg.type = ClientMessage;
	reply_msg.format = 32;
	reply_msg.window = win->dnd_source;
	reply_msg.display = win->display;
	
	XSendEvent(win->display, win->dnd_source, False, 0,
		   (XEvent*)&reply_msg);
}

int
x_dnd_parse(
	x_window_t * win,
	Atom atom,
	char *src,
	int len)
{
	if(!src)
		return 1 ;

	/* COMPOUND_TEXT */
	if( atom == XA_COMPOUND_TEXT(win->display))
	{
		if( !(win->xct_selection_notified))
			return 0 ; /* needs ASCII capable parser*/
		(*win->xct_selection_notified)( win , src , len) ;
		return 1 ;
	}
	/* UTF8_STRING */
	if( atom == XA_UTF8_STRING(win->display) 
	    || atom == XA_DND_MIME_TEXT_UNICODE(win->display))
	{
		if( !(win->utf8_selection_notified))
			return 0 ; /* needs ASCII capable parser*/
		(*win->utf8_selection_notified)( win , src , len) ;
		return 1 ;
	}
	/* XXX ASCII should be safely processed because it's subset of utf8. ISO8859-1 may be problematic...*/
	/* text/plain */
	if( atom == XA_DND_MIME_TEXT_PLAIN(win->display))
	{
		if( !(win->utf8_selection_notified))
			return 0 ; /* needs ASCII capable parser*/
		(*win->utf8_selection_notified)( win , src , len) ;
		return 1 ;
	}
	/* text/url-list */
	if( atom == XA_DND_MIME_TEXT_URL_LIST(win->display))
	{
		int pos ;
		char *delim ;

		if( !(win->utf8_selection_notified))
			return 0 ; /* needs ASCII capable parser*/

		pos = 0 ;
		delim = src ;

		while( pos < len){
			delim = strchr( &(src[pos]), 13) ;
			if( !delim)
				return 0 ; /* parse error */
			while( delim[1] != 10)
			{
				delim[0] = ' ' ; /* eliminate illegal 0x0Ds (they should not appear) */
				delim = strchr( delim, 13) ;
				if ( !delim)
					return 0 ; /* parse error */
			}
			delim[0] = ' '; /* always output ' ' as separator */
			if( strncmp( &(src[pos]), "file:",5) == 0)
			{/* remove "file:". new length is (length - "file:" + " ")*/
				(*win->utf8_selection_notified)( win , &(src[pos+5]) , (delim - src) - pos -4) ;
			}
			else
			{/* as-is + " " */
				(*win->utf8_selection_notified)( win , &(src[pos]) , (delim - src) - pos +1) ;
			}
			pos = (delim - src) +2 ; /* skip 0x0A */
		}
	}
	return 0 ;
}

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
	return 0 ;
}

Atom
x_dnd_preferable_atom(
	x_window_t *  win ,
	Atom *atom,
	int num
	)
{
	int i = 0 ;

	i = is_pref( XA_COMPOUND_TEXT( win->display), atom, num);
	if(!i)
		i = is_pref( XA_UTF8_STRING( win->display), atom, num);
	if(!i)
		i = is_pref( XA_TEXT( win->display), atom, num);
	if(!i)
		i = is_pref( XA_DND_MIME_TEXT_PLAIN( win->display), atom, num);
	if(!i)
		i = is_pref( XA_DND_MIME_TEXT_UNICODE( win->display), atom, num);
	if(!i)
		i = is_pref( XA_DND_MIME_TEXT_URL_LIST( win->display), atom, num);
		
#ifdef  DEBUG
	if(i)
		kik_debug_printf("accepted as atom: %s(%d)\n", XGetAtomName( win->display, atom[i]), atom[i]);
	else
		for( i = 0 ; i < num ; i++)
			if( atom[i])
				kik_debug_printf("dropped unrecognized atom: %d\n", XGetAtomName( win->display, atom[i]));
#endif
	if( i)
		return atom[i];
	else
		return (Atom)0 ;/* it's illegal value for Atom */
}

