/*
 *	$Id$
 */

#include  "x_color.h"

#include  <string.h>		/* memcpy,strcmp */
#include  <stdio.h>		/* sscanf */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <ml_color.h>


/* --- static functions --- */

static void
native_color_to_xcolor(
	x_color_t *  xcolor ,
#ifdef  USE_TYPE_XFT
	XftColor *  ncolor
#else
	XColor *  ncolor
#endif
	)
{
	xcolor->pixel = ncolor->pixel ;
#ifdef  USE_TYPE_XFT
	xcolor->red = (ncolor->color.red >> 8) & 0xff ;
	xcolor->green = (ncolor->color.green >> 8) & 0xff ;
	xcolor->blue = (ncolor->color.blue >> 8) & 0xff ;
#else
	xcolor->red = (ncolor->red >> 8) & 0xff ;
	xcolor->green = (ncolor->green >> 8) & 0xff ;
	xcolor->blue = (ncolor->blue >> 8) & 0xff ;
#endif
	xcolor->is_loaded = 1 ;
}

static int
alloc_closest_xcolor_pseudo(
	Display *  display ,
	int  screen ,
	unsigned short  red ,		/* 0 to 0xffff */
	unsigned short  green ,		/* 0 to 0xffff */
	unsigned short  blue ,		/* 0 to 0xffff */
	x_color_t *  ret_xcolor
	)
{
	Visual *  visual = DefaultVisual( display , screen) ;
	XColor *  all_colors ;	/* colors exist in the shared color map */
	XColor  closest_color ;

	Colormap  cmap = DefaultColormap( display , screen) ;

	int  closest_index = -1 ;
	unsigned long  min_diff = 0xffffffff ;
	unsigned long  diff ;
	unsigned long  diff_r = 0 , diff_g = 0 , diff_b = 0 ;
	int  ncells = DisplayCells( display , screen) ;
	int  i ;

	/* FIXME: When visual class is StaticColor, should not be return? */
	if ( ! visual->class == PseudoColor && ! visual->class == GrayScale)
	{
		return  0 ;
	}

	if( ( all_colors = malloc( ncells * sizeof( XColor))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  0 ;
	}

	/* get all colors from default colormap */
	for( i = 0 ; i < ncells ; i ++)
	{
		all_colors[i].pixel = i ;
	}
	XQueryColors( display , cmap , all_colors, ncells) ;

	/* find the closest color */
	for( i = 0 ; i < ncells ; i ++)
	{
		diff_r = (red - all_colors[i].red) >> 8 ;
		diff_g = (green - all_colors[i].green) >> 8 ;
		diff_b = (blue - all_colors[i].blue) >> 8 ;

		diff = diff_r * diff_r + diff_g * diff_g + diff_b * diff_b ;

		if ( diff < min_diff) /* closest ? */
		{
			min_diff = diff ;
			closest_index = i ;
		}
	}

	if( closest_index == -1)	/* unable to find closest color */
	{
		closest_color.red = 0 ;
		closest_color.green = 0 ;
		closest_color.blue = 0 ;
	}
	else
	{
		closest_color.red = all_colors[closest_index].red ;
		closest_color.green = all_colors[closest_index].green ;
		closest_color.blue = all_colors[closest_index].blue ;
	}

	closest_color.flags = DoRed | DoGreen | DoBlue;
	free( all_colors) ;

	if ( ! XAllocColor( display , cmap , &closest_color))
	{
		return 0 ;
	}

	ret_xcolor->pixel = closest_color.pixel ;
	ret_xcolor->red = (closest_color.red >> 8) & 0xff ;
	ret_xcolor->green = (closest_color.green >> 8) & 0xff ;
	ret_xcolor->blue = (closest_color.blue >> 8) & 0xff ;
	ret_xcolor->is_loaded = 1 ;

	return 1 ;
}


/* --- global functions --- */

#ifdef  USE_TYPE_XFT

int
x_load_named_xcolor(
	Display *  display ,
	int  screen ,
	x_color_t *  xcolor ,
	char *  name
	)
{
	XftColor  ncolor ;
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	
	if( ml_color_parse_rgb_name( &red , &green , &blue , name))
	{
		return  x_load_rgb_xcolor( display , screen , xcolor , red , green , blue) ;
	}
	
	if( ! XftColorAllocName( display , DefaultVisual( display , screen) ,
		DefaultColormap( display , screen) , name , &ncolor))
	{
		XColor  exact ;

		/* try to find closest color */
		if( XParseColor( display , DefaultColormap( display , screen) ,
				name , &exact))
		{
			return  alloc_closest_xcolor_pseudo( display , screen ,
					exact.red , exact.green , exact.blue ,
					xcolor) ;
		}
		else
		{
			/* fallback to sane defaults */
			if( strcmp( name, "white") == 0)
			{
				return  x_load_rgb_xcolor( display, screen, xcolor,
						0xFF, 0xFF, 0xFF) ;
			}
			else if ( strcmp( name, "black") == 0)
			{
				return  x_load_rgb_xcolor( display, screen, xcolor, 0,0,0) ;
			}
		}

		return 0 ;
	}

	native_color_to_xcolor( xcolor , &ncolor) ;

	return  1 ;
}

int
x_load_rgb_xcolor(
	Display *  display ,
	int  screen ,
	x_color_t *  xcolor ,
	u_int8_t  red ,
	u_int8_t  green ,
	u_int8_t  blue
	)
{
	XftColor  ncolor ;
	XRenderColor  rend_color ;
	
	rend_color.red = (red << 8) + red ;
	rend_color.green = (green << 8) + green ;
	rend_color.blue = (blue << 8) + blue ;
	rend_color.alpha = 0xffff ;

	if( ! XftColorAllocValue( display , DefaultVisual( display , screen) ,
		DefaultColormap( display , screen) , &rend_color , &ncolor))
	{
		/* try to find closest color */
		return  alloc_closest_xcolor_pseudo( display , screen ,
				red , green , blue , xcolor) ;
	}

	native_color_to_xcolor( xcolor , &ncolor) ;
	
	return  1 ;
}

int
x_unload_xcolor(
	Display *  display ,
	int  screen ,
	x_color_t *  xcolor
	)
{
#if 0
	XftColorFree( display , DefaultVisual( display , screen) ,
		DefaultColormap( display , screen) , xcolor) ;
#endif

	xcolor->is_loaded = 0 ;

	return  1 ;
}

void
x_color_to_xft(
	XftColor *  xftcolor ,
	x_color_t *  xcolor
	)
{
	xftcolor->pixel = xcolor->pixel ;
	xftcolor->color.red = (xcolor->red << 8) + xcolor->red ;
	xftcolor->color.green = (xcolor->green << 8) + xcolor->green ;
	xftcolor->color.blue = (xcolor->blue << 8) + xcolor->blue ;
	xftcolor->color.alpha = 0xffff ;
}

#else	/* USE_TYPE_XFT */

int
x_load_named_xcolor(
	Display *  display ,
	int  screen ,
	x_color_t *  xcolor ,
	char *  name
	)
{
	XColor  near ;
	XColor  exact ;
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	
	if( ml_color_parse_rgb_name( &red , &green , &blue , name))
	{
		return  x_load_rgb_xcolor( display , screen , xcolor , red , green , blue) ;
	}

	if( ! XAllocNamedColor( display , DefaultColormap( display , screen) , name , &near , &exact))
	{
		/* try to find closest color */
		if( XParseColor( display , DefaultColormap( display , screen) ,
				name , &exact))
		{
			return  alloc_closest_xcolor_pseudo( display , screen ,
					exact.red , exact.green , exact.blue , xcolor) ;
		}
		else
		{
			return  0 ;
		}
	}

	native_color_to_xcolor( xcolor , &near) ;
	
	return  1 ;

}

int
x_load_rgb_xcolor(
	Display *  display ,
	int  screen ,
	x_color_t *  xcolor ,
	u_int8_t  red ,
	u_int8_t  green ,
	u_int8_t  blue
	)
{
	XColor  ncolor ;
	
	ncolor.red = (red << 8) + red ;
	ncolor.green = (green << 8) + green ;
	ncolor.blue = (blue << 8) + blue ;
	ncolor.flags = 0 ;

	if( ! XAllocColor( display , DefaultColormap( display , screen) , &ncolor))
	{
		/* try to find closest color */
		return  alloc_closest_xcolor_pseudo( display , screen ,
				red , green , blue , xcolor) ;
	}

	native_color_to_xcolor( xcolor , &ncolor) ;
	
	return  1 ;
}

int
x_unload_xcolor(
	Display *  display ,
	int  screen ,
	x_color_t *  xcolor
	)
{
#if  0
	u_long  pixel[1] ;

	pixel[0] = xcolor->pixel ;
	
	XFreeColors( display , DefaultColormap( display , screen) , pixel , 1 , 0) ;
#endif

	xcolor->is_loaded = 0 ;

	return  1 ;
}

#endif

int
x_get_xcolor_rgb(
	u_int8_t *  red ,
	u_int8_t *  green ,
	u_int8_t *  blue ,
	x_color_t *  xcolor
	)
{
	*red = xcolor->red ;
	*blue = xcolor->blue ;
	*green = xcolor->green ;

	return  1 ;
}

int
x_xcolor_fade(
	Display *  display ,
	int  screen ,
	x_color_t *  xcolor ,
	u_int  fade_ratio
	)
{
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;

	x_get_xcolor_rgb( &red , &green , &blue , xcolor) ;

#if  0
	kik_msg_printf( "Fading R%d G%d B%d => " , red , green , blue) ;
#endif

	red = (red * fade_ratio) / 100 ;
	green = (green * fade_ratio) / 100 ;
	blue = (blue * fade_ratio) / 100 ;

	x_unload_xcolor( display , screen , xcolor) ;

#if  0
	kik_msg_printf( "R%d G%d B%d\n" , red , green , blue) ;
#endif

	return  x_load_rgb_xcolor( display , screen , xcolor , red, green , blue) ;
}
