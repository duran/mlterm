/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_window.h"

#include <X11/Xft/Xft.h>
#include <pobl/bl_mem.h> /* alloca */

#define ui_color_to_xft(xcolor) _ui_color_to_xft(alloca(sizeof(XftColor)), (xcolor))

/* --- static functions --- */

static XftColor* _ui_color_to_xft(XftColor *xftcolor, ui_color_t *xcolor) {
  xftcolor->pixel = xcolor->pixel;
  xftcolor->color.red = (xcolor->red << 8) + xcolor->red;
  xftcolor->color.green = (xcolor->green << 8) + xcolor->green;
  xftcolor->color.blue = (xcolor->blue << 8) + xcolor->blue;
  xftcolor->color.alpha = (xcolor->alpha << 8) + xcolor->alpha;

  return xftcolor;
}

/* --- global functions --- */

int ui_window_set_use_xft(ui_window_t *win, int use_xft) {
  if (use_xft) {
    if ((win->xft_draw = XftDrawCreate(win->disp->display, win->my_window, win->disp->visual,
                                       win->disp->colormap))) {
      return 1;
    }
  } else {
    XftDrawDestroy(win->xft_draw);
    win->xft_draw = NULL;

    return 1;
  }

  return 0;
}

void ui_window_xft_draw_string8(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color, int x,
                                int y, u_char *str, size_t len) {
  XftColor *xftcolor;

  /* Removing trailing spaces. */
  while (1) {
    if (len == 0) {
      return;
    }

    if (*(str + len - 1) == ' ') {
      len--;
    } else {
      break;
    }
  }

  xftcolor = ui_color_to_xft(fg_color);

  XftDrawString8(win->xft_draw, xftcolor, font->xft_font, x + font->x_off + win->hmargin,
                 y + win->vmargin, str, len);

  if (font->double_draw_gap) {
    XftDrawString8(win->xft_draw, xftcolor, font->xft_font,
                   x + font->x_off + win->hmargin + font->double_draw_gap, y + win->vmargin, str,
                   len);
  }
}

void ui_window_xft_draw_string32(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color, int x,
                                 int y, FcChar32* str, u_int len) {
  XftColor *xftcolor;

  xftcolor = ui_color_to_xft(fg_color);

#ifdef USE_OT_LAYOUT
  if (font->use_ot_layout /* && font->ot_font */) {
    XftDrawGlyphs(win->xft_draw, xftcolor, font->xft_font, x + font->x_off + win->hmargin,
                  y + win->vmargin, str, len);
  } else
#endif
  {
    XftDrawString32(win->xft_draw, xftcolor, font->xft_font, x + font->x_off + win->hmargin,
                    y + win->vmargin, str, len);
  }

  if (font->double_draw_gap) {
#ifdef USE_OT_LAYOUT
    if (font->use_ot_layout /* && font->ot_font */) {
      XftDrawGlyphs(win->xft_draw, xftcolor, font->xft_font, x + font->x_off + win->hmargin,
                    y + win->vmargin, str, len);
    } else
#endif
    {
      XftDrawString32(win->xft_draw, xftcolor, font->xft_font,
                      x + font->x_off + win->hmargin + font->double_draw_gap, y + win->vmargin,
                      str, len);
    }
  }
}

void xft_set_clip(ui_window_t *win, int x, int y, u_int width, u_int height) {
  XRectangle rect;

  rect.x = 0;
  rect.y = 0;
  rect.width = width;
  rect.height = height;

  XftDrawSetClipRectangles(win->xft_draw, x, y, &rect, 1);
}

void xft_unset_clip(ui_window_t *win) {
  XRectangle rect;

  rect.x = 0;
  rect.y = 0;
  rect.width = ACTUAL_WIDTH(win);
  rect.height = ACTUAL_HEIGHT(win);

  XftDrawSetClipRectangles(win->xft_draw, 0, 0, &rect, 1);
}
