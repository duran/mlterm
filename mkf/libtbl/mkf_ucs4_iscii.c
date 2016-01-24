/*
 *	$Id$
 */

#include  "../lib/mkf_ucs4_iscii.h"


#define  ISCIICS_TO_INDEX(cs)  \
		((cs) == ISCII_ASSAMESE ? 0 : \
		 ((cs) == ISCII_TAMIL   ? 7 : \
		  ((cs) == ISCII_TELUGU ? 8 : (cs) - ISCII_BENGALI)))


/* --- static variables --- */

static u_int16_t  iscii_to_ucs_table[90][9] =
{
	/*
	 * XXX ROMAN is not supported.
	 *
	 * ASSAMESE
	 * BENGALI ,GUJARATI,HINDI,KANNADA,MALAYALAM,ORIYA,PUNJABI, TAMIL,  TELUGU ,
	 */
/* 0xa1 */ { 0x981 , 0xa81 , 0x901 , 0x000 , 0x000 , 0xb01 , 0x000 , 0x000 , 0xc01 } ,
/* 0xa2 */ { 0x982 , 0xa82 , 0x902 , 0xc82 , 0xd02 , 0xb02 , 0xa02 , 0xb82 , 0xc02 } ,
/* 0xa3 */ { 0x983 , 0xa83 , 0x903 , 0xc83 , 0xd03 , 0xb03 , 0x000 , 0xb83 , 0xc03 } ,
/* 0xa4 */ { 0x985 , 0xa85 , 0x905 , 0xc85 , 0xd05 , 0xb05 , 0xa05 , 0xb85 , 0xc05 } ,
/* 0xa5 */ { 0x986 , 0xa86 , 0x906 , 0xc86 , 0xd06 , 0xb06 , 0xa06 , 0xb86 , 0xc06 } ,
/* 0xa6 */ { 0x987 , 0xa87 , 0x907 , 0xc87 , 0xd07 , 0xb07 , 0xa07 , 0xb87 , 0xc07 } ,
/* 0xa7 */ { 0x988 , 0xa88 , 0x908 , 0xc88 , 0xd08 , 0xb08 , 0xa08 , 0xb88 , 0xc08 } ,
/* 0xa8 */ { 0x989 , 0xa89 , 0x909 , 0xc89 , 0xd09 , 0xb09 , 0xa09 , 0xb89 , 0xc09 } ,
/* 0xa9 */ { 0x98a , 0xa8a , 0x90a , 0xc8a , 0xd0a , 0xb0a , 0xa0a , 0xb8a , 0xc0a } ,
/* 0xaa */ { 0x98b , 0xa8b , 0x90b , 0xc8b , 0xd0b , 0xb0b , 0x000 , 0x000 , 0xc0b } ,
/* 0xab */ { 0x000 , 0x000 , 0x90e , 0xc8e , 0xd0e , 0x000 , 0x000 , 0xb8e , 0xc0e } ,
/* 0xac */ { 0x98f , 0xa8f , 0x90f , 0xc8f , 0xd0f , 0xb0f , 0xa0f , 0xb8f , 0xc0f } ,
/* 0xad */ { 0x990 , 0xa90 , 0x910 , 0xc90 , 0xd10 , 0xb10 , 0xa10 , 0xb90 , 0xc10 } ,
/* 0xae */ { 0x000 , 0xa8d , 0x90d , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 } ,
/* 0xaf */ { 0x000 , 0x000 , 0x912 , 0xc92 , 0xd12 , 0x000 , 0x000 , 0xb92 , 0xc12 } ,
/* 0xb0 */ { 0x993 , 0xa93 , 0x913 , 0xc93 , 0xd13 , 0xb13 , 0xa13 , 0xb93 , 0xc13 } ,
/* 0xb1 */ { 0x994 , 0xa94 , 0x914 , 0xc94 , 0xd14 , 0xb14 , 0xa14 , 0xb94 , 0xc14 } ,
/* 0xb2 */ { 0x000 , 0xa91 , 0x911 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 } ,
/* 0xb3 */ { 0x995 , 0xa95 , 0x915 , 0xc95 , 0xd15 , 0xb15 , 0xa15 , 0xb95 , 0xc15 } ,
/* 0xb4 */ { 0x996 , 0xa96 , 0x916 , 0xc96 , 0xd16 , 0xb16 , 0xa16 , 0x000 , 0xc16 } ,
/* 0xb5 */ { 0x997 , 0xa97 , 0x917 , 0xc97 , 0xd17 , 0xb17 , 0xa17 , 0x000 , 0xc17 } ,
/* 0xb6 */ { 0x998 , 0xa98 , 0x918 , 0xc98 , 0xd18 , 0xb18 , 0xa18 , 0x000 , 0xc18 } ,
/* 0xb7 */ { 0x999 , 0xa99 , 0x919 , 0xc99 , 0xd19 , 0xb19 , 0xa19 , 0xb99 , 0xc19 } ,
/* 0xb8 */ { 0x99a , 0xa9a , 0x91a , 0xc9a , 0xd1a , 0xb1a , 0xa1a , 0xb9a , 0xc1a } ,
/* 0xb9 */ { 0x99b , 0xa9b , 0x91b , 0xc9b , 0xd1b , 0xb1b , 0xa1b , 0x000 , 0xc1b } ,
/* 0xba */ { 0x99c , 0xa9c , 0x91c , 0xc9c , 0xd1c , 0xb1c , 0xa1c , 0xb9c , 0xc1c } ,
/* 0xbb */ { 0x99d , 0xa9d , 0x91d , 0xc9d , 0xd1d , 0xb1d , 0xa1d , 0x000 , 0xc1d } ,
/* 0xbc */ { 0x99e , 0xa9e , 0x91e , 0xc9e , 0xd1e , 0xb1e , 0xa1e , 0xb9e , 0xc1e } ,
/* 0xbd */ { 0x99f , 0xa9f , 0x91f , 0xc9f , 0xd1f , 0xb1f , 0xa1f , 0xb9f , 0xc1f } ,
/* 0xbe */ { 0x9a0 , 0xaa0 , 0x920 , 0xca0 , 0xd20 , 0xb20 , 0xa20 , 0x000 , 0xc20 } ,
/* 0xbf */ { 0x9a1 , 0xaa1 , 0x921 , 0xca1 , 0xd21 , 0xb21 , 0xa21 , 0x000 , 0xc21 } ,
/* 0xc0 */ { 0x9a2 , 0xaa2 , 0x922 , 0xca2 , 0xd22 , 0xb22 , 0xa22 , 0x000 , 0xc22 } ,
/* 0xc1 */ { 0x9a3 , 0xaa3 , 0x923 , 0xca3 , 0xd23 , 0xb23 , 0xa23 , 0xba3 , 0xc23 } ,
/* 0xc2 */ { 0x9a4 , 0xaa4 , 0x924 , 0xca4 , 0xd24 , 0xb24 , 0xa24 , 0xba4 , 0xc24 } ,
/* 0xc3 */ { 0x9a5 , 0xaa5 , 0x925 , 0xca5 , 0xd25 , 0xb25 , 0xa25 , 0x000 , 0xc25 } ,
/* 0xc4 */ { 0x9a6 , 0xaa6 , 0x926 , 0xca6 , 0xd26 , 0xb26 , 0xa26 , 0x000 , 0xc26 } ,
/* 0xc5 */ { 0x9a7 , 0xaa7 , 0x927 , 0xca7 , 0xd27 , 0xb27 , 0xa27 , 0x000 , 0xc27 } ,
/* 0xc6 */ { 0x9a8 , 0xaa8 , 0x928 , 0xca8 , 0xd28 , 0xb28 , 0xa28 , 0xba8 , 0xc28 } ,
/* 0xc7 */ { 0x000 , 0x000 , 0x929 , 0x000 , 0x000 , 0x000 , 0x000 , 0xba9 , 0x000 } ,
/* 0xc8 */ { 0x9aa , 0xaaa , 0x92a , 0xcaa , 0xd2a , 0xb2a , 0xa2a , 0xbaa , 0xc2a } ,
/* 0xc9 */ { 0x9ab , 0xaab , 0x92b , 0xcab , 0xd2b , 0xb2b , 0xa2b , 0x000 , 0xc2b } ,
/* 0xca */ { 0x9ac , 0xaac , 0x92c , 0xcac , 0xd2c , 0xb2c , 0xa2c , 0x000 , 0xc2c } ,
/* 0xcb */ { 0x9ad , 0xaad , 0x92d , 0xcad , 0xd2d , 0xb2d , 0xa2d , 0x000 , 0xc2d } ,
/* 0xcc */ { 0x9ae , 0xaae , 0x92e , 0xcae , 0xd2e , 0xb2e , 0xa2e , 0xbae , 0xc2e } ,
/* 0xcd */ { 0x9af , 0xaaf , 0x92f , 0xcaf , 0xd2f , 0xb2f , 0xa2f , 0xbaf , 0xc2f } ,
/* 0xce */ { 0x9df , 0x000 , 0x95f , 0x000 , 0x000 , 0xb5f , 0x000 , 0x000 , 0x000 } ,
/* 0xcf */ { 0x9b0 , 0xab0 , 0x930 , 0xcb0 , 0xd30 , 0xb30 , 0xa30 , 0x9f0 , 0xc30 } ,
/* 0xd0 */ { 0x000 , 0x000 , 0x931 , 0xcb1 , 0xd31 , 0x000 , 0x000 , 0xbb1 , 0xc31 } ,
/* 0xd1 */ { 0x9b2 , 0xab2 , 0x932 , 0xcb2 , 0xd32 , 0xb32 , 0xa32 , 0xbb2 , 0xc32 } ,
/* 0xd2 */ { 0x000 , 0xab3 , 0x933 , 0xcb3 , 0xd33 , 0xb33 , 0xa33 , 0xbb3 , 0xc33 } ,
/* 0xd3 */ { 0x000 , 0x000 , 0x934 , 0x000 , 0xd34 , 0x000 , 0x000 , 0xbb4 , 0x000 } ,
/* 0xd4 */ { 0x000 , 0xab5 , 0x935 , 0xcb5 , 0xd35 , 0x000 , 0xa35 , 0x9f1 , 0xc35 } ,
/* 0xd5 */ { 0x9b6 , 0xab6 , 0x936 , 0xcb6 , 0xd36 , 0xb36 , 0xa36 , 0x000 , 0xc36 } ,
/* 0xd6 */ { 0x9b7 , 0xab7 , 0x937 , 0xcb7 , 0xd37 , 0xb37 , 0x000 , 0xbb7 , 0xc37 } ,
/* 0xd7 */ { 0x9b8 , 0xab8 , 0x938 , 0xcb8 , 0xd38 , 0xb38 , 0xa38 , 0xbb8 , 0xc38 } ,
/* 0xd8 */ { 0x9b9 , 0xab9 , 0x939 , 0xcb9 , 0xd39 , 0xb39 , 0xa39 , 0xbb9 , 0xc39 } ,
/* 0xd9 */ { 0x000 , 0x218d , 0x200d , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 } ,
/* 0xda */ { 0x9be , 0xabe , 0x93e , 0xcbe , 0xd3e , 0xb3e , 0xa3e , 0xbbe , 0xc3e } ,
/* 0xdb */ { 0x9bf , 0xabf , 0x93f , 0xcbf , 0xd3f , 0xb3f , 0xa3f , 0xbbf , 0xc3f } ,
/* 0xdc */ { 0x9c0 , 0xac0 , 0x940 , 0xcc0 , 0xd40 , 0xb40 , 0xa40 , 0xbc0 , 0xc40 } ,
/* 0xdd */ { 0x9c1 , 0xac1 , 0x941 , 0xcc1 , 0xd41 , 0xb41 , 0xa41 , 0xbc1 , 0xc41 } ,
/* 0xde */ { 0x9c2 , 0xac2 , 0x942 , 0xcc2 , 0xd42 , 0xb42 , 0xa42 , 0xbc2 , 0xc42 } ,
/* 0xdf */ { 0x9c3 , 0xac3 , 0x943 , 0xcc3 , 0xd43 , 0xb43 , 0x000 , 0x000 , 0xc43 } ,
/* 0xe0 */ { 0x000 , 0x000 , 0x946 , 0xcc6 , 0xd46 , 0x000 , 0x000 , 0xbc6 , 0xc46 } ,
/* 0xe1 */ { 0x9c7 , 0xac7 , 0x947 , 0xcc7 , 0xd47 , 0xb47 , 0xa47 , 0xbc7 , 0xc47 } ,
/* 0xe2 */ { 0x9c8 , 0xac8 , 0x948 , 0xcc8 , 0xd48 , 0xb48 , 0xa48 , 0xbc8 , 0xc48 } ,
/* 0xe3 */ { 0x000 , 0xac5 , 0x945 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 } ,
/* 0xe4 */ { 0x000 , 0x000 , 0x94a , 0xcca , 0xd4a , 0x000 , 0x000 , 0xbca , 0xc4a } ,
/* 0xe5 */ { 0x9cb , 0xacb , 0x94b , 0xccb , 0xd4b , 0xb4b , 0xa4b , 0xbcb , 0xc4b } ,
/* 0xe6 */ { 0x9cc , 0xacc , 0x94c , 0xccc , 0xd4c , 0xb4c , 0xa4c , 0xbcc , 0xc4c } ,
/* 0xe7 */ { 0x000 , 0xac9 , 0x949 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 } ,
/* 0xe8 */ { 0x9cd , 0xacd , 0x94d , 0xccd , 0xd4d , 0xb4d , 0xa4d , 0xbcd , 0xc4d } ,
/* 0xe9 */ { 0x9bc , 0xabc , 0x93c , 0x000 , 0x000 , 0xb3c , 0xa3c , 0x000 , 0x000 } ,
/* 0xea */ { 0x000 , 0x000 , 0x964 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 } ,
/* 0xeb */ { 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 } ,
/* 0xec */ { 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 } ,
/* 0xed */ { 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 } ,
/* 0xee */ { 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 } ,
/* 0xef */ { 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 } ,
/* 0xf0 */ { 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 , 0x000 } ,
/* 0xf1 */ { 0x9e6 , 0xae6 , 0x966 , 0xce6 , 0xd66 , 0xb66 , 0xa66 , 0x000 , 0xc66 } ,
/* 0xf2 */ { 0x9e7 , 0xae7 , 0x967 , 0xce7 , 0xd67 , 0xb67 , 0xa67 , 0xbe7 , 0xc67 } ,
/* 0xf3 */ { 0x9e8 , 0xae8 , 0x968 , 0xce8 , 0xd68 , 0xb68 , 0xa68 , 0xbe8 , 0xc68 } ,
/* 0xf4 */ { 0x9e9 , 0xae9 , 0x969 , 0xce9 , 0xd69 , 0xb69 , 0xa69 , 0xbe9 , 0xc69 } ,
/* 0xf5 */ { 0x9ea , 0xaea , 0x96a , 0xcea , 0xd6a , 0xb6a , 0xa6a , 0xbea , 0xc6a } ,
/* 0xf6 */ { 0x9eb , 0xaeb , 0x96b , 0xceb , 0xd6b , 0xb6b , 0xa6b , 0xbeb , 0xc6b } ,
/* 0xf7 */ { 0x9ec , 0xaec , 0x96c , 0xcec , 0xd6c , 0xb6c , 0xa6c , 0xbec , 0xc6c } ,
/* 0xf8 */ { 0x9ed , 0xaed , 0x96d , 0xced , 0xd6d , 0xb6d , 0xa6d , 0xbed , 0xc6d } ,
/* 0xf9 */ { 0x9ee , 0xaee , 0x96e , 0xcee , 0xd6e , 0xb6e , 0xa6e , 0xbee , 0xc6e } ,
/* 0xfa */ { 0x9ef , 0xaef , 0x96f , 0xcef , 0xd6f , 0xb6f , 0xa6f , 0xbef , 0xc6f } ,
} ;


/* --- static functions --- */

static u_int8_t
convert_ucs_to_iscii(
	u_int16_t  ucs ,
	mkf_charset_t  cs
	)
{
	u_int8_t  iscii ;

	for( iscii = 0xa1 ; iscii <= 0xfa ; iscii++)
	{
		if( iscii_to_ucs_table[iscii - 0xa1][ISCIICS_TO_INDEX(cs)] == ucs)
		{
			return  iscii ;
		}
	}

	return  0 ;
}

static int
convert_iscii_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  iscii_code ,
	mkf_charset_t  cs
	)
{
	if( iscii_code <= 0x9f)
	{
		ucs4->ch[0] = 0x0 ;
		ucs4->ch[1] = 0x0 ;
		ucs4->ch[2] = 0x0 ;
		ucs4->ch[3] = iscii_code ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}
	else if( 0xa1 <= iscii_code && iscii_code <= 0xfa)
	{
		u_int32_t  ucs_code ;

		if( (ucs_code = iscii_to_ucs_table[iscii_code - 0xa1][ISCIICS_TO_INDEX(cs)]) == 0)
		{
			return  0 ;
		}

		mkf_int_to_bytes( ucs4->ch , 4 , ucs_code) ;
		ucs4->size = 4 ;
		ucs4->cs = ISO10646_UCS4_1 ;
		ucs4->property = 0 ;

		return  1 ;
	}

	return  0 ;
}


/* --- global functions --- */

int
mkf_map_iscii_assamese_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  iscii_code
	)
{
	return  convert_iscii_to_ucs4( ucs4 , iscii_code , ISCII_ASSAMESE) ;
}

int
mkf_map_iscii_bengali_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  iscii_code
	)
{
	return  convert_iscii_to_ucs4( ucs4 , iscii_code , ISCII_BENGALI) ;
}

int
mkf_map_iscii_gujarati_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  iscii_code
	)
{
	return  convert_iscii_to_ucs4( ucs4 , iscii_code , ISCII_GUJARATI) ;
}

int
mkf_map_iscii_hindi_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  iscii_code
	)
{
	return  convert_iscii_to_ucs4( ucs4 , iscii_code , ISCII_HINDI) ;
}

int
mkf_map_iscii_kannada_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  iscii_code
	)
{
	return  convert_iscii_to_ucs4( ucs4 , iscii_code , ISCII_KANNADA) ;
}

int
mkf_map_iscii_malayalam_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  iscii_code
	)
{
	return  convert_iscii_to_ucs4( ucs4 , iscii_code , ISCII_MALAYALAM) ;
}

int
mkf_map_iscii_oriya_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  iscii_code
	)
{
	return  convert_iscii_to_ucs4( ucs4 , iscii_code , ISCII_ORIYA) ;
}

int
mkf_map_iscii_punjabi_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  iscii_code
	)
{
	return  convert_iscii_to_ucs4( ucs4 , iscii_code , ISCII_PUNJABI) ;
}

int
mkf_map_iscii_roman_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  iscii_code
	)
{
	return  convert_iscii_to_ucs4( ucs4 , iscii_code , ISCII_ROMAN) ;
}

int
mkf_map_iscii_tamil_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  iscii_code
	)
{
	return  convert_iscii_to_ucs4( ucs4 , iscii_code , ISCII_TAMIL) ;
}

int
mkf_map_iscii_telugu_to_ucs4(
	mkf_char_t *  ucs4 ,
	u_int16_t  iscii_code
	)
{
	return  convert_iscii_to_ucs4( ucs4 , iscii_code , ISCII_TELUGU) ;
}

int
mkf_map_ucs4_to_iscii(
	mkf_char_t *  non_ucs ,
	u_int32_t  ucs4_code
	)
{
	if( ucs4_code < 0x900 || 0xd7f < ucs4_code)
	{
		return  0 ;
	}
	
	if( ucs4_code <= 0x97f)
	{
		non_ucs->cs = ISCII_HINDI ;
	}
	else if( /* 0x980 <= ucs4_code && */ ucs4_code <= 0x9ff)
	{
		non_ucs->cs = ISCII_BENGALI ;
	}
	else if( /* 0xa00 <= ucs4_code && */ ucs4_code <= 0xa7f)
	{
		non_ucs->cs = ISCII_PUNJABI ;
	}
	else if( /* 0xa80 <= ucs4_code && */ ucs4_code <= 0xaff)
	{
		non_ucs->cs = ISCII_GUJARATI ;
	}
	else if( /* 0xb00 <= ucs4_code && */ ucs4_code <= 0xb7f)
	{
		non_ucs->cs = ISCII_ORIYA ;
	}
	else if( /* 0xb80 <= ucs4_code && */ ucs4_code <= 0xbff)
	{
		non_ucs->cs = ISCII_TAMIL ;
	}
	else if( /* 0xc00 <= ucs4_code && */ ucs4_code <= 0xc7f)
	{
		non_ucs->cs = ISCII_TELUGU ;
	}
	else if( /* 0xc80 <= ucs4_code && */ ucs4_code <= 0xcff)
	{
		non_ucs->cs = ISCII_KANNADA ;
	}
	else if( 0xd00 <= ucs4_code)
	{
		non_ucs->cs = ISCII_MALAYALAM ;
	}
	else
	{
		return  0 ;
	}

	if( ( non_ucs->ch[0] = convert_ucs_to_iscii( ucs4_code , non_ucs->cs)) == 0)
	{
		return  0 ;
	}
	
	non_ucs->size = 1 ;
	non_ucs->property = 0 ;

	return  1 ;
}
