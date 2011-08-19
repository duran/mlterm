/*
 *	$Id$
 */

#include  "kik_path.h"

#include  <stdio.h>	/* NULL */
#include  <string.h>
#include  <ctype.h>	/* isdigit */
#ifdef  USE_WIN32API
#include  <sys/stat.h>
#include  <windows.h>	/* IsDBCSLeadByte */
#endif

#include  "kik_str.h"	/* kik_str_alloca_dup */


#if  0
#define  __DEBUG
#endif


/* --- global functions --- */

#if  ! defined(HAVE_BASENAME) || defined(USE_WIN32API)

char *
__kik_basename(
	char *  path
	)
{
	char *  p ;

	if( path == NULL || *path == '\0')
	{
		return "." ;
	}

	p = path + strlen(path) - 1 ;
	while( 1)
	{
		if( p == path)
		{
			return  p ;
		}
		else if( *p == '/'
		#ifdef  USE_WIN32API
			|| (*p == '\\' && (p - 1 == path || ! IsDBCSLeadByte(*(p - 1))))
		#endif
			)
		{
			*(p--) = '\0' ;
		}
		else
		{
			break ;
		}
	}

	while( 1)
	{
		if( *p == '/'
		#ifdef  USE_WIN32API
			|| (*p == '\\' && (p - 1 == path || ! IsDBCSLeadByte(*(p - 1))))
		#endif
			)
		{
			return  p + 1 ;
		}
		else
		{
			if( p == path)
			{
				return  p ;
			}
		}

		p -- ;
	}
}

#endif


#ifndef  REMOVE_FUNCS_MLTERM_UNUSE

int
kik_path_cleanname(
	char *  cleaned_path ,
	size_t  size ,
	const char *  path
	)
{
	char *  src ;
	char *  dst ;
	size_t  left ;
	char *  p ;

	if( size == 0)
	{
		return  0 ;
	}

	if( ( src = kik_str_alloca_dup( path)) == NULL)
	{
		return  0 ;
	}

	dst = cleaned_path ;
	left = size ;

	if( *src == '/')
	{
		*(dst ++) = '\0' ;
		left -- ;
		src ++ ;
	}
	
	while( ( p = strchr( src , '/')))
	{
		*p = '\0' ;
		
		if( strcmp( src , ".") == 0)
		{
			goto  end ;
		}
		else if( strcmp( src , "..") == 0 && left < size)
		{
			char *  last ;

			if( ( last = strrchr( cleaned_path , '/')))
			{
				last ++ ;
			}
			else
			{
				last = cleaned_path ;
			}

			if( *last != '\0' && strcmp( last , "..") != 0)
			{
				dst -= (strlen( last) + 1) ;
				left += (strlen( last) + 1) ;

				*dst = '\0' ;

				goto  end ;
			}
		}
		
		if( *src)
		{
			if( left < strlen( src) + 1)
			{
				return  1 ;
			}

			if( left < size)
			{
				*(dst - 1) = '/' ;
			}
			
			strcpy( dst , src) ;
			
			dst += (strlen( src) + 1) ;
			left -= (strlen( src) + 1) ;
		}

	end:
		src = p + 1 ;
	}

	if( src && *src)
	{
		if( left < strlen( src) + 1)
		{
			return  1 ;
		}

		if( left < size)
		{
			*(dst - 1) = '/' ;
		}

		strcpy( dst , src) ;
		
		dst += (strlen( src) + 1) ;
		left -= (strlen( src) + 1) ;
	}

	return  1 ;
}

#endif /* REMOVE_FUNCS_MLTERM_UNUSE */

/*
 * Parsing "<user>@<proto>:<host>:<port>:<aux>".
 */
int
kik_parse_uri(
	char **  proto ,	/* proto can be NULL. If seq doesn't have proto, NULL is set. */
	char **  user ,		/* user can be NULL. If seq doesn't have user, NULL is set. */
	char **  host ,		/* host can be NULL. */
	char **  port ,		/* port can be NULL. If seq doesn't have port, NULL is set. */
	char **  path ,		/* path can be NULL. If seq doesn't have path, NULL is set. */
	char **  aux ,		/* aux can be NULL. If seq doesn't have aux string, NULL is set. */
	char *  seq		/* broken in this function. If NULL, return 0. */
	)
{
	char *  p ;
	size_t  len ;

	/*
	 * This hack enables the way of calling this function like
	 * 'kik_parse_uri( ... , kik_str_alloca_dup( "string"))'
	 */
	if( ! seq)
	{
		return  0 ;
	}

	len = strlen( seq) ;
	if( len > 6 &&
		( strncmp( seq , "ssh://" , 6) == 0 || strncmp( seq , "ftp://" , 6) == 0))
	{
		seq = (p = seq) + 6 ;
		*(seq - 3) = '\0' ;
	}
	else if( len > 9 &&
		( strncmp( seq , "telnet://" , 9) == 0 || strncmp( seq , "rlogin://" , 9) == 0))
	{
		seq = (p = seq) + 9 ;
		*(seq - 3) = '\0' ;
	}
	else
	{
		p = NULL ;
	}

	if( proto)
	{
		*proto = p ;
	}

	if( ( p = strchr( seq , '/')))
	{
		*(p ++) = '\0' ;
		if( *p == '\0')
		{
			p = NULL ;
		}
	}

	if( path)
	{
		*path = p ;
	}

	if( ( p = strchr( seq , '@')))
	{
		*p = '\0' ;
		if( user)
		{
			*user = seq ;
		}
		seq = p + 1 ;
	}
	else if( user)
	{
		*user = NULL ;
	}

	if( host)
	{
		*host = seq ;
	}

	if( ( p = strchr( seq , ':')))
	{
		*(p ++) = '\0' ;

		if( isdigit( (int)*p))
		{
			seq = p ;
			while( isdigit( (int)(*(++p))) ) ;
			if( *p == '\0')
			{
				p = NULL ;
			}
			else
			{
				*(p ++) = '\0' ;
			}
		}
		else
		{
			seq = NULL ;
		}
	}
	else
	{
		seq = NULL ;
	}

	if( port)
	{
		*port = seq ;
	}

	if( aux)
	{
		*aux = p ;
	}
	
	return  1 ;
}

char *
kik_get_home_dir(void)
{
	char *  dir ;

#ifdef  USE_WIN32API
	if( ( dir = getenv( "HOMEPATH")) && *dir)
	{
		return  dir ;
	}
#endif

	if( ( dir = getenv( "HOME")) && *dir)
	{
		return  dir ;
	}

	return  NULL ;
}


#ifdef  __DEBUG
int
main(void)
{
	char  uri1[] = "ssh://ken@localhost.localdomain:22" ;
	char  uri2[] = "ken@localhost.localdomain:22" ;
	char  uri3[] = "ken@localhost.localdomain:22" ;
	char  uri4[] = "ken@localhost.localdomain" ;
	char  uri5[] = "ssh://localhost.localdomain" ;
	char  uri6[] = "telnet://ken@localhost.localdomain:22:eucjp/usr/local/" ;
	char  uri7[] = "ssh://ken@localhost.localdomain:22:eucjp/" ;
	char  uri8[] = "ssh://localhost:eucjp/usr/local" ;
	char *  user ;
	char *  proto ;
	char *  host ;
	char *  port ;
	char *  encoding ;
	char *  path ;

	kik_parse_uri( &proto , &user , &host , &port , &path , &encoding , uri1) ;
	printf( "%s %s %s %s %s %s\n" , proto , user , host , port , path , encoding) ;
	kik_parse_uri( &proto , &user , &host , &port , &path , &encoding , uri2) ;
	printf( "%s %s %s %s %s %s\n" , proto , user , host , port , path , encoding) ;
	kik_parse_uri( &proto , &user , &host , &port , &path , &encoding , uri3) ;
	printf( "%s %s %s %s %s %s\n" , proto , user , host , port , path , encoding) ;
	kik_parse_uri( &proto , &user , &host , &port , &path , &encoding , uri4) ;
	printf( "%s %s %s %s %s %s\n" , proto , user , host , port , path , encoding) ;
	kik_parse_uri( &proto , &user , &host , &port , &path , &encoding , uri5) ;
	printf( "%s %s %s %s %s %s\n" , proto , user , host , port , path , encoding) ;
	kik_parse_uri( &proto , &user , &host , &port , &path , &encoding , uri6) ;
	printf( "%s %s %s %s %s %s\n" , proto , user , host , port , path , encoding) ;
	kik_parse_uri( &proto , &user , &host , &port , &path , &encoding , uri7) ;
	printf( "%s %s %s %s %s %s\n" , proto , user , host , port , path , encoding) ;
	kik_parse_uri( &proto , &user , &host , &port , &path , &encoding , uri8) ;
	printf( "%s %s %s %s %s %s\n" , proto , user , host , port , path , encoding) ;

	return  0 ;
}
#endif

#ifdef  __DEBUG
int
main(
	int  argc ,
	char **  argv
	)
{
	printf( "%s\n" , __kik_basename( argv[1])) ;

	return  1 ;
}
#endif
