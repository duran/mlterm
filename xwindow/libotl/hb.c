/*
 *	$Id$
 */

#include  <hb.h>
#include  <hb-ft.h>
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>


/* --- static functions --- */

static hb_feature_t *
get_hb_features(
	const char *  features ,
	u_int *  num
	)
{
	static const char *  prev_features ;
	static hb_feature_t *  hbfeatures ;
	static u_int  num_of_features ;

	if( features != prev_features)
	{
		char *  str ;
		void *  p ;

		if( ( str = alloca( strlen( features) + 1)) &&
		    ( p = realloc( hbfeatures ,
				sizeof(hb_feature_t) *
				(kik_count_char_in_str( features , ',') + 1))))
		{
			hbfeatures = p ;

			strcpy( str , features) ;
			num_of_features = 0 ;
			while( ( p = kik_str_sep( &str , ",")))
			{
				if( hb_feature_from_string( p , -1 ,
					&hbfeatures[num_of_features]))
				{
					num_of_features++ ;
				}
			}

			if( num_of_features == 0)
			{
				free( hbfeatures) ;
				hbfeatures = NULL ;
			}
		}

		prev_features = features ;
	}

	*num = num_of_features ;

	return  hbfeatures ;
}


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_OTL
static
#endif
void *
otl_open(
	void *  obj ,
	u_int  size
	)
{
	return  hb_ft_font_create( obj , NULL) ;
}

#ifdef  NO_DYNAMIC_LOAD_OTL
static
#endif
void
otl_close(
	void *  hbfont
	)
{
	hb_font_destroy( hbfont) ;
}

#ifdef  NO_DYNAMIC_LOAD_OTL
static
#endif
u_int
otl_convert_text_to_glyphs(
	void *  hbfont ,
	u_int32_t *  shaped ,
	u_int  shaped_len ,
	u_int32_t *  cmapped ,
	u_int32_t *  src ,
	u_int  src_len ,
	const char *  script ,
	const char *  features
	)
{
	if( src && cmapped)
	{
		memcpy( cmapped , src , sizeof(*src) * src_len) ;

		return  src_len ;
	}
	else
	{
		static hb_buffer_t *  buf ;
		hb_glyph_info_t *  info ;
		u_int  count ;
		u_int  num ;
		hb_feature_t *  hbfeatures ;

		if( ! buf)
		{
			buf = hb_buffer_create() ;
		}
		else
		{
			hb_buffer_reset( buf) ;
		}

		if( cmapped)
		{
			src = cmapped ;
		}

		hb_buffer_add_utf32( buf , src , src_len , 0 , src_len) ;

	#if  0
		hb_buffer_guess_segment_properties( buf) ;
	#else
		{
			u_int32_t  code ;
			hb_script_t  hbscript ;

			hbscript = HB_TAG( script[0] & ~0x20 ,	/* Upper case */
			                   script[1] | 0x20 ,	/* Lower case */
					   script[2] | 0x20 ,	/* Lower case */
					   script[3] | 0x20) ;	/* Lower case */

			for( count = 0 ; count < src_len ; count++)
			{
				code = src[count] ;

				if( 0x900 <= code && code <= 0xd7f)
				{
					if( code <= 0x97f)
					{
						hbscript = HB_SCRIPT_DEVANAGARI ;
					}
					else if( /* 0x980 <= code && */ code <= 0x9ff)
					{
						hbscript = HB_SCRIPT_BENGALI ;
					}
					else if( /* 0xa00 <= code && */ code <= 0xa7f)
					{
						/* PUNJABI */
						hbscript = HB_SCRIPT_GURMUKHI ;
					}
					else if( /* 0xa80 <= code && */ code <= 0xaff)
					{
						hbscript = HB_SCRIPT_GUJARATI ;
					}
					else if( /* 0xb00 <= code && */ code <= 0xb7f)
					{
						hbscript = HB_SCRIPT_ORIYA ;
					}
					else if( /* 0xb80 <= code && */ code <= 0xbff)
					{
						hbscript = HB_SCRIPT_TAMIL ;
					}
					else if( /* 0xc00 <= code && */ code <= 0xc7f)
					{
						hbscript = HB_SCRIPT_TELUGU ;
					}
					else if( /* 0xc80 <= code && */ code <= 0xcff)
					{
						hbscript = HB_SCRIPT_KANNADA ;
					}
					else if( 0xd00 <= code)
					{
						hbscript = HB_SCRIPT_MALAYALAM ;
					}
				}
			}

			hb_buffer_set_script( buf , hbscript) ;
			hb_buffer_set_direction( buf ,
				hb_script_get_horizontal_direction( hbscript)) ;
			hb_buffer_set_language( buf , hb_language_get_default()) ;
		}
	#endif

		hbfeatures = get_hb_features( features , &num) ;
		hb_shape( hbfont , buf , hbfeatures , num) ;

		info = hb_buffer_get_glyph_infos( buf , &num) ;

		for( count = 0 ; count < num ; count++)
		{
			shaped[count] = info[count].codepoint ;
		}

	#if  0
		{
			hb_glyph_position_t *  pos = hb_buffer_get_glyph_positions( buf , &num) ;
			for( count = 0 ; count < num ; count++)
			{
				kik_msg_printf( "%d+%d " ,
					pos[count].x_offset , pos[count].x_advance) ;
			}
			kik_msg_printf( "\n") ;
		}
	#endif

	#if  0
		hb_buffer_destroy( buf) ;
	#endif

		return  num ;
	}
}
