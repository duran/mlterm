/*
 *	$Id$
 */

#ifndef  __MKF_UCS4_BIG5_H__
#define  __MKF_UCS4_BIG5_H__


#include  <kiklib/kik_types.h>	/* u_xxx */

#include  "mkf_char.h"


int  mkf_map_big5_to_ucs4( mkf_char_t *  ucs4 , u_int16_t  big5) ;

int  mkf_map_ucs4_to_big5( mkf_char_t *  non_ucs , u_int32_t  ucs4_code) ;


#endif
