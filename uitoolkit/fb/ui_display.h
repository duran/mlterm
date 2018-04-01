/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef ___UI_DISPLAY_H__
#define ___UI_DISPLAY_H__

#include "../ui_display.h"

#ifdef __FreeBSD__
#include <sys/kbio.h> /* NLKED */
#else
#define CLKED 1
#define NLKED 2
#define SLKED 4
#define ALKED 8
#endif

#define KeyPress 2      /* Private in fb/ */
#define ButtonPress 4   /* Private in fb/ */
#define ButtonRelease 5 /* Private in fb/ */
#define MotionNotify 6  /* Private in fb/ */

#define IM_WINDOW_IS_ACTIVATED(disp) ((disp)->num_roots > 1 && (disp)->roots[1]->is_mapped)

#ifdef USE_GRF
#define TP_COLOR 12
#endif

/* common functions for ui_window.c */

#define ui_display_get_pixel(disp, x, y) ui_display_get_pixel2(x, y)

u_long ui_display_get_pixel2(int x, int y);

#define ui_display_put_image(disp, x, y, image, size, need_fb_pixel) \
        ui_display_put_image2(x, y, image, size, need_fb_pixel)

void ui_display_put_image2(int x, int y, u_char *image, size_t size, int need_fb_pixel);

#define ui_display_copy_lines(disp, src_x, src_y, dst_x, dst_y, width, height) \
        ui_display_copy_lines2(src_x, src_y, dst_x, dst_y, width, height)

void ui_display_copy_lines2(int src_x, int src_y, int dst_x, int dst_y, u_int width, u_int height);

/* common functions for ui_window.c (pseudo color) */

void ui_display_fill_with(int x, int y, u_int width, u_int height, u_int8_t pixel);

int ui_cmap_get_closest_color(u_long *closest, int red, int green, int blue);

int ui_cmap_get_pixel_rgb(u_int8_t *red, u_int8_t *green, u_int8_t *blue, u_long pixel);

/* platform specific functions for ui_window.c */

#ifdef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
void ui_display_enable_to_change_cmap(int flag);

int ui_display_is_changeable_cmap(void);

void ui_display_set_cmap(u_int32_t *cmap, u_int cmap_size);
#endif

#ifdef USE_GRF
int x68k_tvram_is_enabled(void);

int x68k_tvram_set_wall_picture(u_short *image, u_int width, u_int height);
#endif

#endif
