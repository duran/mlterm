/*
 *	$Id$
 */

#include  "ml_pty.h"

#include  <unistd.h>		/* close */
#include  <sys/ioctl.h>
#include  <termios.h>
#include  <signal.h>		/* signal/SIGWINCH */
#include  <stdio.h>		/* fprintf */
#include  <string.h>		/* strchr/memcpy */
#include  <stdlib.h>		/* putenv */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* realloc/alloca */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_pty.h>


/* --- global functions --- */

ml_pty_t *
ml_pty_new(
	char *  cmd_path ,
	char **  cmd_argv ,
	char **  env ,
	u_int  cols ,
	u_int  rows
	)
{
	pid_t  pid ;
	ml_pty_t *  pty ;

	if( ( pty = malloc( sizeof( ml_pty_t))) == NULL)
	{
		return  NULL ;
	}
	
	pid = kik_pty_fork( &pty->fd) ;

	if( pid == -1)
	{
		goto  error ;
	}
	else if( pid == 0)
	{
		/* child process */

		/*
		 * setting environmental variables.
		 */
		while( *env)
		{
			/* an argument string of putenv() must be allocated memory.(see SUSV2) */
			putenv( strdup( *env)) ;

			env ++ ;
		}
		
		/* reset signals and spin off the command interpreter */
		signal(SIGINT, SIG_DFL) ;
		signal(SIGQUIT, SIG_DFL) ;
		signal(SIGCHLD, SIG_DFL) ;

	#if  0
		/*
		 * XXX is this necessary ?
		 *
		 * mimick login's behavior by disabling the job control signals.
		 * a shell that wants them can turn them back on
		 */
		signal(SIGTSTP , SIG_IGN) ;
		signal(SIGTTIN , SIG_IGN) ;
		signal(SIGTTOU , SIG_IGN) ;
	#endif

		if( strchr( cmd_path , '/') == NULL)
		{
			if( execvp( cmd_path , cmd_argv) < 0)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " execve(%s) failed.\n" , cmd_path) ;
			#endif

				exit(1) ;
			}
		}
		else
		{
			if( execv( cmd_path , cmd_argv) < 0)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " execve(%s) failed.\n" , cmd_path) ;
			#endif

				exit(1) ;
			}
		}
	}

	/* parent process */

	pty->child_pid = pid ;
	pty->buf = NULL ;
	pty->left = 0 ;
	
	if( ml_set_pty_winsize( pty , cols , rows) == 0)
	{
		goto  error ;
	}

	return  pty ;

error:
	if( pty)
	{
		free( pty) ;
	}

	return  NULL ;
}

int
ml_pty_delete(
	ml_pty_t *  pty
	)
{
#ifdef  __DEBUG
	kik_debug_printf( "fd: %d is closed\n" , pty->fd) ;
#endif

	if( close( pty->fd) == 0)
	{
		free( pty) ;
	
		return  1 ;
	}
	else
	{
		free( pty) ;
		
		return  0 ;
	}
}

int
ml_set_pty_winsize(
	ml_pty_t *  pty ,
	u_int  cols ,
	u_int  rows
	)
{
	struct winsize  ws ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " win size cols %d rows %d.\n" , cols , rows) ;
#endif

	ws.ws_col = cols ;
	ws.ws_row = rows ;
	
	if( ioctl( pty->fd , TIOCSWINSZ , &ws) < 0)
	{
	#ifdef  DBEUG
		kik_warn_printf( KIK_DEBUG_TAG " ioctl(TIOCSWINSZ) failed.\n") ;
	#endif
	
		return  0 ;
	}

	kill( pty->child_pid , SIGWINCH) ;

	return  1 ;
}

size_t
ml_write_to_pty(
	ml_pty_t *  pty ,
	u_char *  buf ,
	size_t  len
	)
{
	u_char *  w_buf ;
	size_t  w_buf_size ;
	int  written_size ;
	void *  p ;

	if( ( w_buf_size = pty->left + len) == 0)
	{
		return  0 ;
	}

	if( ( w_buf = alloca( w_buf_size)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" alloca() failed and , bytes to be written to pty are lost.\n") ;
	#endif
	
		return  pty->left ;
	}

	if( pty->buf && pty->left > 0)
	{
		memcpy( w_buf , pty->buf , pty->left) ;
	}

	if( len > 0)
	{
		memcpy( &w_buf[pty->left] , buf , len) ;
	}

#ifdef  __DEBUG
	{
		int  i ;
		for( i = 0 ; i < w_buf_size ; i++)
		{
			fprintf( stderr , "%.2x" , w_buf[i]) ;
		}
		fprintf( stderr , "\n") ;
	}
#endif

	written_size = write( pty->fd , w_buf , w_buf_size) ;
	if( written_size == w_buf_size)
	{
		if( pty->buf)
		{
			/* reset */
			free( pty->buf) ;
			pty->buf = NULL ;
			pty->left = 0 ;
		}
	
		return  0 ;
	}
	else if( written_size < 0)
	{
		written_size = 0 ;
	}

	pty->left = w_buf_size - written_size ;

	if( ( p = realloc( pty->buf , pty->left)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " realloc failed.\n") ;
	#endif
	
		return  0 ;
	}

	pty->buf = p ;
	
	memcpy( pty->buf , &w_buf[written_size] , pty->left) ;

	return  pty->left ;
}

int
ml_flush_pty(
	ml_pty_t *  pty
	)
{
	return  ml_write_to_pty( pty , NULL , 0) ;
}

size_t
ml_read_pty(
	ml_pty_t *  pty ,
	u_char *  bytes ,
	size_t  left
	)
{
	size_t  read_size ;

	read_size = 0 ;
	while( 1)
	{
		int  ret ;

		ret = read( pty->fd , &bytes[read_size] , left) ;
		if( ret <= 0)
		{
			return  read_size ;
		}
		else
		{
			read_size += ret ;
			left -= ret ;
		}
	}
}
