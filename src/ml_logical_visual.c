/*
 *	$Id$
 */

#include  "ml_logical_visual.h"

#include  <kiklib/kik_mem.h>		/* realloc/free */

#include  "ml_image_intern.h"


typedef struct  iscii_logical_visual
{
	ml_logical_visual_t  logvis ;
	
	ml_iscii_state_t  iscii_state ;

	ml_image_line_t *  logical_lines ;
	ml_image_line_t *  visual_lines ;
	u_int  num_of_cols ;
	u_int  num_of_rows ;

} iscii_logical_visual_t ;


/* --- static functions --- */

/*
 * Bidi logical <=> visual methods
 */
 
static int
bidi_delete(
	ml_logical_visual_t *  logvis
	)
{
	ml_image_t *  image ;
	int  row ;

	image = logvis->image ;
	
	for( row = 0 ; row < image->num_of_rows ; row ++)
	{
		ml_imgline_unuse_bidi( &image->lines[row]) ;
	}

	free( logvis) ;
	
	return  1 ;
}

static int
bidi_change_image(
	ml_logical_visual_t *  logvis ,
	ml_image_t *  new_image
	)
{
	ml_image_t *  image ;
	int  row ;

	image = logvis->image ;
	
	for( row = 0 ; row < image->num_of_rows ; row ++)
	{
		ml_imgline_unuse_bidi( &image->lines[row]) ;
	}

	logvis->image = new_image ;
	
	return  1 ;
}

static int
bidi_render(
	ml_logical_visual_t *  logvis
	)
{
	ml_image_t *  image ;
	ml_image_line_t *  line ;
	int  row ;

	image = logvis->image ;

	/*
	 * all lines(not only filled lines) should be rendered.
	 */
	for( row = 0 ; row < image->num_of_rows ; row ++)
	{
		line = &IMAGE_LINE(image,row) ;
		
		if( line->is_modified)
		{
			if( ! ml_imgline_is_using_bidi( line))
			{
				ml_imgline_use_bidi( line) ;
			}

			ml_imgline_bidi_render( line) ;
		}
	}

	return  1 ;
}

static int
bidi_visual(
	ml_logical_visual_t *  logvis
	)
{
	ml_image_t *  image ;
	int  row ;
	u_int  cols_rest ;

	if( logvis->is_visual)
	{
		kik_error_printf( KIK_DEBUG_TAG " is called continuously.\n") ;
		
		return  0 ;
	}
	
	image = logvis->image ;

#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest , image->cursor.col , 0) ;

	for( row = 0 ; row < image->num_of_filled_rows ; row ++)
	{
		if( ! ml_imgline_bidi_visual( &IMAGE_LINE(image,row)))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " visualize row %d failed.\n" , row) ;
		#endif
		}
	}

	logvis->cursor_logical_char_index = image->cursor.char_index ;
	logvis->cursor_logical_col = image->cursor.col ;

	image->cursor.char_index = ml_bidi_convert_logical_char_index_to_visual(
					&CURSOR_LINE(image) , image->cursor.char_index) ;
	image->cursor.col = ml_convert_char_index_to_col( &CURSOR_LINE(image) ,
					image->cursor.char_index , 0) + cols_rest ;

#ifdef  CURSOR_DEBUG
	fprintf( stderr , "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	logvis->is_visual = 1 ;

	return  1 ;
}

static int
bidi_logical(
	ml_logical_visual_t *  logvis
	)
{
	ml_image_t *  image ;
	int  row ;

	if( ! logvis->is_visual)
	{
		kik_error_printf( KIK_DEBUG_TAG " is called continuously.\n") ;
		
		return  0 ;
	}
	
	image = logvis->image ;
	
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif

	for( row = 0 ; row < image->num_of_filled_rows ; row ++)
	{
		if( ! ml_imgline_bidi_logical( &IMAGE_LINE(image,row)))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " visualize row %d failed.\n" , row) ;
		#endif
		}
	}

	image->cursor.char_index = logvis->cursor_logical_char_index ;
	image->cursor.col = logvis->cursor_logical_col ;

#ifdef  CURSOR_DEBUG
	fprintf( stderr , "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		image->cursor.char_index , image->cursor.col , image->cursor.row) ;
#endif
	
	logvis->is_visual = 0 ;
	
	return  1 ;
}

static int
bidi_visual_line(
	ml_logical_visual_t *  logvis ,
	ml_image_line_t *  line
	)
{
	/* just to be sure */
	ml_imgline_bidi_render( line) ;
	
	ml_imgline_bidi_visual( line) ;

	return  1 ;
}


/*
 * ISCII logical <=> visual methods
 */
 
static int
iscii_delete(
	ml_logical_visual_t *  logvis
	)
{
	iscii_logical_visual_t *  iscii_logvis ;
	int  row ;

	iscii_logvis = (iscii_logical_visual_t*) logvis ;
	
	for( row = 0 ; row < iscii_logvis->num_of_rows ; row ++)
	{
		ml_imgline_final( &iscii_logvis->visual_lines[row]) ;
		ml_imgline_final( &iscii_logvis->logical_lines[row]) ;
	}

	free( iscii_logvis->visual_lines) ;
	free( iscii_logvis->logical_lines) ;
	
	free( logvis) ;
	
	return  1 ;
}

static int
iscii_change_image(
	ml_logical_visual_t *  logvis ,
	ml_image_t *  image
	)
{
	logvis->image = image ;

	return  1 ;
}

static int
iscii_render(
	ml_logical_visual_t *  logvis
	)
{
	return  1 ;
}

static int
search_same_line(
	ml_image_line_t *  lines ,
	u_int  num_of_lines ,
	ml_image_line_t *  line
	)
{
	int  row ;

	for( row = 0 ; row < num_of_lines ; row ++)
	{
		if( lines[row].num_of_filled_chars == line->num_of_filled_chars &&
			ml_str_bytes_equal( lines[row].chars , line->chars , line->num_of_filled_chars))
		{
			return  row ;
		}
	}

	/* not found */
	return  -1 ;
}

static int
iscii_visual(
	ml_logical_visual_t *  logvis
	)
{
	iscii_logical_visual_t *  iscii_logvis ;
	ml_image_t *  image ;
	int  row ;
	int  cols_rest ;

	if( logvis->is_visual)
	{
		kik_error_printf( KIK_DEBUG_TAG " is called continuously.\n") ;
		
		return  0 ;
	}
	
	image = logvis->image ;

	iscii_logvis = (iscii_logical_visual_t*) logvis ;

	if( iscii_logvis->num_of_rows != image->num_of_rows ||
		iscii_logvis->num_of_cols != image->num_of_cols)
	{
		/* ml_image_t is resized */
		
		if( iscii_logvis->num_of_rows != image->num_of_rows)
		{
			if( iscii_logvis->visual_lines)
			{
				for( row = 0 ; row < iscii_logvis->num_of_rows ; row ++)
				{
					ml_imgline_final( &iscii_logvis->visual_lines[row]) ;
				}
			}

			if( iscii_logvis->logical_lines)
			{
				for( row = 0 ; row < iscii_logvis->num_of_rows ; row ++)
				{
					ml_imgline_final( &iscii_logvis->logical_lines[row]) ;
				}
			}
			
			if( ( iscii_logvis->visual_lines = realloc( iscii_logvis->visual_lines ,
					sizeof( ml_image_line_t) * image->num_of_rows)) == NULL)
			{
				return  0 ;
			}

			if( ( iscii_logvis->logical_lines = realloc( iscii_logvis->logical_lines ,
					sizeof( ml_image_line_t) * image->num_of_rows)) == NULL)
			{
				free( iscii_logvis->visual_lines) ;
				iscii_logvis->visual_lines = NULL ;
				
				return  0 ;
			}
			
			iscii_logvis->num_of_rows = image->num_of_rows ;
		}
		
		for( row = 0 ; row < iscii_logvis->num_of_rows ; row ++)
		{
			ml_imgline_init( &iscii_logvis->visual_lines[row] , image->num_of_cols) ;
			ml_imgline_init( &iscii_logvis->logical_lines[row] , image->num_of_cols) ;
		}

		iscii_logvis->num_of_cols = image->num_of_cols ;
	}
	
	ml_convert_col_to_char_index( &CURSOR_LINE(image) , &cols_rest , image->cursor.col , 0) ;
	
	for( row = 0 ; row < image->num_of_filled_rows ; row ++)
	{
		int  is_cache_active ;
		
		if( IMAGE_LINE(image,row).is_modified)
		{
			is_cache_active = 0 ;
		}
		else if( iscii_logvis->logical_lines[row].num_of_filled_chars !=
				IMAGE_LINE(image,row).num_of_filled_chars ||
			! ml_str_bytes_equal( iscii_logvis->logical_lines[row].chars ,
				IMAGE_LINE(image,row).chars ,
				iscii_logvis->logical_lines[row].num_of_filled_chars) )
		{
			int  hit_row ;
			
			if( ( hit_row = search_same_line( iscii_logvis->logical_lines ,
				iscii_logvis->num_of_rows , &IMAGE_LINE(image,row))) == -1)
			{
				is_cache_active = 0 ;
			}
			else
			{
				/*
				 * XXX
				 * this may break active cache in "row" line.
				 */
				ml_imgline_copy_line( &iscii_logvis->logical_lines[row] ,
					&iscii_logvis->logical_lines[hit_row]) ;
				ml_imgline_copy_line( &iscii_logvis->visual_lines[row] ,
					&iscii_logvis->visual_lines[hit_row]) ;
				
				is_cache_active = 1 ;
			}
		}
		else
		{
			is_cache_active = 1 ;
		}

		if( is_cache_active)
		{
			/* using cached line */
			ml_imgline_copy_line( &IMAGE_LINE(image,row) , &iscii_logvis->visual_lines[row]) ;
		}
		else
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " iscii rendering.\n") ;
		#endif
		
			ml_imgline_copy_line( &iscii_logvis->logical_lines[row] , &IMAGE_LINE(image,row)) ;
			ml_imgline_is_updated( &iscii_logvis->logical_lines[row]) ;

			ml_imgline_iscii_visual( &IMAGE_LINE(image,row) , iscii_logvis->iscii_state) ;

			/* caching */
			ml_imgline_copy_line( &iscii_logvis->visual_lines[row] , &IMAGE_LINE(image,row)) ;
			ml_imgline_is_updated( &iscii_logvis->visual_lines[row]) ;
		}
	}

	logvis->cursor_logical_char_index = image->cursor.char_index ;
	logvis->cursor_logical_col = image->cursor.col ;
	
	image->cursor.char_index = ml_iscii_convert_logical_char_index_to_visual(
					&CURSOR_LINE(image) , image->cursor.char_index) ;
	image->cursor.col = ml_convert_char_index_to_col( &CURSOR_LINE(image) ,
				image->cursor.char_index , 0) + cols_rest ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d]\n" ,
		image->cursor.col , image->cursor.char_index) ;
#endif

	logvis->is_visual = 1 ;
	
	return  1 ;
}

static int
iscii_logical(
	ml_logical_visual_t *  logvis
	)
{
	ml_image_t *  image ;
	int  row ;

	if( ! logvis->is_visual)
	{
		kik_error_printf( KIK_DEBUG_TAG " is called continuously.\n") ;
		
		return  0 ;
	}
	
	image = logvis->image ;
	
	for( row = 0 ; row < image->num_of_filled_rows ; row ++)
	{
		ml_imgline_copy_line( &IMAGE_LINE(image,row) ,
			&((iscii_logical_visual_t*)logvis)->logical_lines[row]) ;
	}

	image->cursor.char_index = logvis->cursor_logical_char_index ;
	image->cursor.col = logvis->cursor_logical_col ;
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [col %d index %d]\n" ,
		image->cursor.col , image->cursor.char_index) ;
#endif

	logvis->is_visual = 0 ;
	
	return  1 ;
}

static int
iscii_visual_line(
	ml_logical_visual_t *  logvis ,
	ml_image_line_t *  line
	)
{
	ml_iscii_state_t  iscii_state ;

	iscii_state = ((iscii_logical_visual_t*) logvis)->iscii_state ;

	ml_imgline_iscii_visual( line , iscii_state) ;
	
	return  1 ;
}


/* --- global functions --- */

ml_logical_visual_t *
ml_logvis_bidi_new(
	ml_image_t *  image
	)
{
	ml_logical_visual_t *  logvis ;

	if( ( logvis = malloc( sizeof( ml_image_t))) == NULL)
	{
		return  NULL ;
	}
	
	logvis->image = image ;

	logvis->cursor_logical_char_index = 0 ;
	logvis->cursor_logical_col = 0 ;

	logvis->is_visual = 0 ;
	
	logvis->delete = bidi_delete ;
	logvis->change_image = bidi_change_image ;
	logvis->render = bidi_render ;
	logvis->visual = bidi_visual ;
	logvis->logical = bidi_logical ;
	logvis->visual_line = bidi_visual_line ;

	return  logvis ;
}

ml_logical_visual_t *
ml_logvis_iscii_new(
	ml_image_t *  image ,
	ml_iscii_state_t  iscii_state
	)
{
	iscii_logical_visual_t *  iscii_logvis ;

	if( ( iscii_logvis = malloc( sizeof( iscii_logical_visual_t))) == NULL)
	{
		return  NULL ;
	}

	iscii_logvis->iscii_state = iscii_state ;
	iscii_logvis->visual_lines = NULL ;
	iscii_logvis->logical_lines = NULL ;
	iscii_logvis->num_of_cols = 0 ;
	iscii_logvis->num_of_rows = 0 ;
	
	iscii_logvis->logvis.image = image ;
	
	iscii_logvis->logvis.cursor_logical_char_index = 0 ;
	iscii_logvis->logvis.cursor_logical_col = 0 ;

	iscii_logvis->logvis.is_visual = 0 ;
	
	iscii_logvis->logvis.delete = iscii_delete ;
	iscii_logvis->logvis.change_image = iscii_change_image ;
	iscii_logvis->logvis.render = iscii_render ;
	iscii_logvis->logvis.visual = iscii_visual ;
	iscii_logvis->logvis.logical = iscii_logical ;
	iscii_logvis->logvis.visual_line = iscii_visual_line ;

	return  (ml_logical_visual_t*) iscii_logvis ;
}
