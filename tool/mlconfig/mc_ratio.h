/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __MC_RATIO_H__
#define __MC_RATIO_H__

#include <gtk/gtk.h>

#define MC_RATIOS 5

#define MC_RATIO_CONTRAST 0
#define MC_RATIO_GAMMA 1
#define MC_RATIO_BRIGHTNESS 2
#define MC_RATIO_FADE 3
#define MC_RATIO_SCREEN_WIDTH 4

GtkWidget *mc_ratio_config_widget_new(int id);

void mc_update_ratio(int id);

#endif
