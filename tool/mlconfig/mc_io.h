/*
 *	$Id$
 */

#ifndef  __MC_IO_H__
#define  __MC_IO_H__


int  mc_set_str_value( char *  key , char *  value , int  save) ;

int  mc_set_flag_value( char *  key , int  flag_val , int  save) ;

int  mc_flush( void) ;

char *  mc_get_str_value( char *  key) ;

int  mc_get_flag_value( char *  key) ;

char *  mc_get_bgtype(void) ;


#endif
