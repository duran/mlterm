/*
 *	$Id$
 */

#include  "mc_mod_meta.h"

#include  <kiklib/kik_debug.h>
#include  <glib.h>
#include  <ml_intl.h>


#if  0
#define  __DEBUG
#endif


/* --- static variables --- */

static char *  new_mod_meta_mode ;
static int  is_changed ;


/* --- static functions --- */

static gint
button_none_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( GTK_TOGGLE_BUTTON(widget)->active)
	{
		new_mod_meta_mode = "none" ;
		is_changed = 1 ;
	}
	
	return  1 ;
}

static gint
button_esc_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( GTK_TOGGLE_BUTTON(widget)->active)
	{
		new_mod_meta_mode = "esc" ;
		is_changed = 1 ;
	}
		
	return  1 ;
}

static gint
button_8bit_checked(
	GtkWidget *  widget ,
	gpointer  data
	)
{
	if( GTK_TOGGLE_BUTTON(widget)->active)
	{
		new_mod_meta_mode = "8bit" ;
		is_changed = 1 ;
	}
	
	return  1 ;
}


/* --- global functions --- */

GtkWidget *
mc_mod_meta_config_widget_new(
	char *  mod_meta_mode
	)
{
	GtkWidget *  label ;
	GtkWidget *  hbox ;
	GtkWidget *  radio ;
	GSList *  group ;

	hbox = gtk_hbox_new(FALSE , 0) ;

	label = gtk_label_new( _("Meta key outputs:")) ;
	gtk_widget_show(label) ;
	gtk_box_pack_start(GTK_BOX(hbox) , label , TRUE , TRUE , 0) ;
	
	group = NULL ;

	radio = gtk_radio_button_new_with_label( group , _("None")) ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_none_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;

	if( strcmp( mod_meta_mode , "none") == 0)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}

	radio = gtk_radio_button_new_with_label( group , _("Esc")) ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_esc_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;
	
	if( strcmp( mod_meta_mode , "esc") == 0)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}
	
	radio = gtk_radio_button_new_with_label( group , _("8bit")) ;
	group = gtk_radio_button_group( GTK_RADIO_BUTTON(radio)) ;
	gtk_signal_connect(GTK_OBJECT(radio) , "toggled" , GTK_SIGNAL_FUNC(button_8bit_checked) , NULL) ;
	gtk_widget_show(GTK_WIDGET(radio)) ;
	gtk_box_pack_start(GTK_BOX(hbox) , radio , TRUE , TRUE , 0) ;
	
	if( strcmp( mod_meta_mode , "8bit") == 0)
	{
		gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(radio) , TRUE) ;
	}

	new_mod_meta_mode = mod_meta_mode ;
	is_changed = 0 ;
	
	return  hbox ;
}

char *
mc_get_mod_meta_mode(void)
{
	if( ! is_changed)
	{
		return  NULL ;
	}
	
	is_changed = 0 ;
	
	return  new_mod_meta_mode ;
}
