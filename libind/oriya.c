/*
 *	$Id: $
 */

#include  "indian.h"
#include  "table/oriya.table"

struct tabl *
libind_get_table(
	u_int *  table_size
	)
{
	*table_size = sizeof( iscii_oriya_table) / sizeof( struct tabl) ;

	return  iscii_oriya_table ;
}