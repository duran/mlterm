/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __OTL_H__
#define __OTL_H__

#include <pobl/bl_types.h>

#ifdef NO_DYNAMIC_LOAD_OTL

#ifdef USE_HARFBUZZ
#include "hb.c"
#else
#include "otf.c"
#endif

#else /* NO_DYNAMIC_LOAD_OTL */

#include <pobl/bl_dlfcn.h>
#include <pobl/bl_debug.h>
#include <stdio.h>

#ifndef LIBDIR
#define OTL_DIR "/usr/local/lib/mlterm/"
#else
#define OTL_DIR LIBDIR "/mlterm/"
#endif

#if defined(USE_WIN32GUI)
#define OTL_LIB "otl-win32"
#elif defined(USE_QUARTZ)
#define OTL_LIB "otl-quartz"
#else
#define OTL_LIB "otl"
#endif

/* --- static variables --- */

static void *(*open_sym)(void *, u_int);
static void (*close_sym)(void *);
static u_int (*convert_sym)(void *, u_int32_t *, u_int, int8_t *, u_int8_t *, u_int32_t *,
                            u_int32_t *, u_int, const char *, const char *, u_int);

/* --- static functions --- */

static void *otl_open(void *obj, u_int size) {
  static int is_tried;

  if (!is_tried) {
    bl_dl_handle_t handle;

    is_tried = 1;

    if ((!(handle = bl_dl_open(OTL_DIR, OTL_LIB)) && !(handle = bl_dl_open("", OTL_LIB))) ||
        !(open_sym = bl_dl_func_symbol(handle, "otl_open")) ||
        !(close_sym = bl_dl_func_symbol(handle, "otl_close")) ||
        !(convert_sym = bl_dl_func_symbol(handle, "otl_convert_text_to_glyphs"))) {
      bl_error_printf("libotl: Could not load.\n");

      if (handle) {
        bl_dl_close(handle);
      }

      return NULL;
    }
  } else if (!open_sym) {
    return NULL;
  }

  return (*open_sym)(obj, size);
}

static void otl_close(void *otf) { (*close_sym)(otf); }

static u_int otl_convert_text_to_glyphs(void *otf, u_int32_t *shaped, u_int shaped_len,
                                        int8_t *offsets, u_int8_t *widths, u_int32_t *cmapped,
                                        u_int32_t *src, u_int src_len, const char *script,
                                        const char *features, u_int fontsize) {
  return (*convert_sym)(otf, shaped, shaped_len, offsets, widths, cmapped, src, src_len, script,
                        features, fontsize);
}

#endif

#endif /* __OTL_H__ */
