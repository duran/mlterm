/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/*
 * im_m17nlib.c - input method plugin using m17n library
 *
 * Copyright (C) 2004 Seiichi SATO <ssato@sh.rim.or.jp>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of any author may not be used to endorse or promote
 *    products derived from this software without their specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 *	$Id$
 */

#include <pobl/bl_mem.h>    /* malloc/alloca/free */
#include <pobl/bl_str.h>    /* bl_str_alloca_dup bl_snprintf bl_str_sep*/
#include <pobl/bl_locale.h> /* bl_get_lang */

#include <m17n.h>
#include <m17n-misc.h> /* merror_code */

#include <ui_im.h>
#include "../im_common.h"
#include "../im_info.h"

#if 0
#define IM_M17NLIB_DEBUG 1
#endif

#define MAX_BYTES_PER_CHAR 4     /* FIXME */
#define MAX_BYTES_ESC_SEQUEACE 5 /* FIXME */

#define MAX_BYTES(n) (((n)*MAX_BYTES_PER_CHAR) + MAX_BYTES_ESC_SEQUEACE + 1)

typedef struct im_m17nlib {
  /* input method common object */
  ui_im_t im;

  MInputMethod *input_method;
  MInputContext *input_context;

  MConverter *mconverter;    /* MText -> u_char    */
  ef_parser_t *parser_term; /* for term encoding  */
  ef_conv_t *conv;

} im_m17nlib_t;

/* --- static variables --- */

static int ref_count = 0;
static int initialized = 0;
static ef_parser_t *parser_ascii = NULL;
/* mlterm internal symbols */
static ui_im_export_syms_t *syms = NULL;

/* --- static functions --- */

#ifdef IM_M17NLIB_DEBUG
static void show_available_ims(void) {
  MPlist *im_list;
  MSymbol sym_im;
  int num_ims;
  int i;

  sym_im = msymbol("input-method");

  im_list = mdatabase_list(sym_im, Mnil, Mnil, Mnil);

  num_ims = mplist_length(im_list);

  for (i = 0; i < num_ims; i++, im_list = mplist_next(im_list)) {
    MDatabase *db;
    MSymbol *tag;

    db = mplist_value(im_list);
    tag = mdatabase_tag(db);

    printf("%d: %s(%s)\n", i, msymbol_name(tag[2]), msymbol_name(tag[1]));
  }

  m17n_object_unref(im_list);
}
#endif

#ifndef USE_XLIB
#undef XKeysymToString
static char *XKeysymToString(KeySym ksym) {
  /* Latin 1 */
  if (0x20 <= ksym && ksym <= 0x7e) {
    static char str[2];
    str[0] = ksym;
    str[1] = '\0';

    return str;
  }

  switch (ksym) {
    /* TTY Functions */
    case XK_BackSpace:
      return "Backspace";
    case XK_Tab:
      return "Tab";
    case XK_Return:
      return "Return";
    case XK_Escape:
      return "Escape";
    case XK_Delete:
      return "Delete";
#ifdef XK_Multi_key
    /* International & multi-key character composition */
    case XK_Multi_key:
      return "Multi_key";
#endif
    /* Japanese keyboard support */
    case XK_Muhenkan:
      return "Muhenkan";
    case XK_Henkan_Mode:
      return "Henkan_Mode";
    case XK_Zenkaku_Hankaku:
      return "Zenkaku_Hankaku";
    case XK_Hiragana_Katakana:
      return "Hiragana_Katakana";
    /* Cursor control & motion */
    case XK_Home:
      return "Home";
    case XK_Left:
      return "Left";
    case XK_Up:
      return "Up";
    case XK_Right:
      return "Right";
    case XK_Down:
      return "Down";
    case XK_Prior:
      return "Prior";
    case XK_Next:
      return "Next";
    case XK_End:
      return "End";
    case XK_F1:
      return "F1";
    case XK_F2:
      return "F2";
    case XK_F3:
      return "F3";
    case XK_F4:
      return "F4";
    case XK_F5:
      return "F5";
    case XK_F6:
      return "F6";
    case XK_F7:
      return "F7";
    case XK_F8:
      return "F8";
    case XK_F9:
      return "F9";
    case XK_F10:
      return "F10";
    case XK_F11:
      return "F11";
    case XK_F12:
      return "F12";
    case XK_F13:
      return "F13";
    case XK_F14:
      return "F14";
    case XK_F15:
      return "F15";
    case XK_F16:
      return "F16";
    case XK_F17:
      return "F17";
    case XK_F18:
      return "F18";
    case XK_F19:
      return "F19";
    case XK_F20:
      return "F20";
    case XK_F21:
      return "F21";
    case XK_F22:
      return "F22";
    case XK_F23:
      return "F23";
    case XK_F24:
      return "F24";
#ifdef XK_F25
    case XK_F25:
      return "F25";
    case XK_F26:
      return "F26";
    case XK_F27:
      return "F27";
    case XK_F28:
      return "F28";
    case XK_F29:
      return "F29";
    case XK_F30:
      return "F30";
    case XK_F31:
      return "F31";
    case XK_F32:
      return "F32";
    case XK_F33:
      return "F33";
    case XK_F34:
      return "F34";
    case XK_F35:
      return "F35";
#endif
#ifdef XK_KP_Space
    case XK_KP_Space:
      return ' ';
#endif
#ifdef XK_KP_Tab
    case XK_KP_Tab:
      return "Tab";
#endif
#ifdef XK_KP_Enter
    case XK_KP_Enter:
      return "Return";
#endif
    case XK_KP_F1:
      return "F1";
    case XK_KP_F2:
      return "F2";
    case XK_KP_F3:
      return "F3";
    case XK_KP_F4:
      return "F4";
    case XK_KP_Home:
      return "Home";
    case XK_KP_Left:
      return "Left";
    case XK_KP_Up:
      return "Up";
    case XK_KP_Right:
      return "Right";
    case XK_KP_Down:
      return "Down";
    case XK_KP_Prior:
      return "Prior";
    case XK_KP_Next:
      return "Next";
    case XK_KP_End:
      return "End";
    case XK_KP_Delete:
      return "Delete";
#ifdef XK_KP_Equal
    case XK_KP_Equal:
      return "=";
#endif
    case XK_KP_Multiply:
      return "*";
    case XK_KP_Add:
      return "+";
    case XK_KP_Separator:
      return ",";
    case XK_KP_Subtract:
      return "-";
    case XK_KP_Decimal:
      return ".";
    case XK_KP_Divide:
      return "/";
    /* keypad numbers */
    case XK_KP_0:
      return "0";
    case XK_KP_1:
      return "1";
    case XK_KP_2:
      return "2";
    case XK_KP_3:
      return "3";
    case XK_KP_4:
      return "4";
    case XK_KP_5:
      return "5";
    case XK_KP_6:
      return "6";
    case XK_KP_7:
      return "7";
    case XK_KP_8:
      return "8";
    case XK_KP_9:
      return "9";
    default:
      return NULL;
  }
}
#endif

static MSymbol xksym_to_msymbol(im_m17nlib_t *m17nlib, KeySym ksym, u_int state) {
  char mod[13] = "";
  char *key;
  char *str;
  int filled_len = 0;
  size_t len;

  int is_shift;
  int is_lock;
  int is_ctl;
  int is_alt;
  int is_meta;
  int is_super;
  int is_hyper;

  if (IsModifierKey(ksym)) {
    return Mnil;
  }

  (*m17nlib->im.listener->compare_key_state_with_modmap)(m17nlib->im.listener->self, state,
                                                         &is_shift, &is_lock, &is_ctl, &is_alt,
                                                         &is_meta, NULL, &is_super, &is_hyper);

  /* Latin 1 */
  if (0x20 <= ksym && ksym <= 0x7e) {
    char buf[2] = " ";
    buf[0] = ksym;

    if (is_shift && ('a' <= buf[0] && buf[0] <= 'z')) {
      buf[0] += ('A' - 'a');
      is_shift = 0;
    }

    return msymbol(buf);
  }

  if (is_shift) filled_len += bl_snprintf(&mod[filled_len], sizeof(mod) - filled_len, "S-");
  if (is_ctl) filled_len += bl_snprintf(&mod[filled_len], sizeof(mod) - filled_len, "C-");
  if (is_alt) filled_len += bl_snprintf(&mod[filled_len], sizeof(mod) - filled_len, "A-");
  if (is_meta) filled_len += bl_snprintf(&mod[filled_len], sizeof(mod) - filled_len, "M-");
  if (is_super) filled_len += bl_snprintf(&mod[filled_len], sizeof(mod) - filled_len, "s-");
  if (is_hyper) filled_len += bl_snprintf(&mod[filled_len], sizeof(mod) - filled_len, "H-");

  if (!(key = XKeysymToString(ksym))) {
    return Mnil;
  }

  len = strlen(mod) + strlen(key) + 1;

  if (!(str = alloca(len))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " alloca failed\n");
#endif

    return Mnil;
  }

  bl_snprintf(str, len, "%s%s", mod, key);

  return msymbol(str);
}

static MInputMethod *find_input_method(char *param) {
  char *lang = NULL;
  char *im_name = NULL;
  MPlist *im_list;
  MInputMethod *result = NULL;
  int found = 0;
  int num_ims;
  int i;

  if (param) {
    lang = bl_str_alloca_dup(param);
    if (strstr(lang, ":")) {
      im_name = lang;
      lang = bl_str_sep(&im_name, ":");
    }

    if (lang && strcmp(lang, "") == 0) {
      lang = NULL;
    }

    if (im_name && strcmp(im_name, "") == 0) {
      im_name = NULL;
    }
  }

  if (lang == NULL && im_name == NULL) {
    lang = bl_get_lang();
  }

  if (!(im_list = mdatabase_list(msymbol("input-method"), Mnil, Mnil, Mnil))) {
    bl_error_printf("There are no available input methods.\n");
    return 0;
  }

  num_ims = mplist_length(im_list);

  for (i = 0; i < num_ims; i++, im_list = mplist_next(im_list)) {
    MDatabase *db;
    MSymbol *tag;

    db = mplist_value(im_list);
    tag = mdatabase_tag(db);

    if (tag[1] == Mnil) {
      continue;
    }

    if (lang && im_name) {
      if (strcmp(lang, msymbol_name(tag[1])) == 0 && strcmp(im_name, msymbol_name(tag[2])) == 0) {
        found = 1;
      }
    } else if (lang) {
      if (strcmp(lang, msymbol_name(tag[1])) == 0) {
        found = 1;
      }
    } else if (im_name) {
      if (strcmp(im_name, msymbol_name(tag[2])) == 0) {
        found = 1;
      }
    }

    if (found) {
#ifdef IM_M17NLIB_DEBUG
      bl_debug_printf(BL_DEBUG_TAG " found. language: %s, im_name: %s\n", msymbol_name(tag[1]),
                      msymbol_name(tag[2]));
#endif
      result = minput_open_im(tag[1], tag[2], NULL);
      break;
    }
  }

  m17n_object_unref(im_list);

  return result;
}

static void commit(im_m17nlib_t *m17nlib, MText *text) {
  u_char *buf = NULL;
  u_int num_chars;
  int filled_len;

  if ((num_chars = mtext_len(text))) {
    if (!(buf = alloca(MAX_BYTES(num_chars)))) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG, " alloca failed\n");
#endif
    }
  }

  if (buf) {
    mconv_reset_converter(m17nlib->mconverter);
    mconv_rebind_buffer(m17nlib->mconverter, buf, MAX_BYTES(num_chars));
    filled_len = mconv_encode(m17nlib->mconverter, text);

    if (filled_len == -1) {
      bl_error_printf(
          "Could not convert the encoding of committed characters. [error "
          "code: %d]\n",
          merror_code);
    } else {
      (*m17nlib->im.listener->write_to_term)(m17nlib->im.listener->self, buf, filled_len);
    }
  }
}

static void set_candidate(im_m17nlib_t *m17nlib, MText *candidate, int idx) {
  u_char *buf;
  u_int num_chars;
  u_int filled_len;

  if (!(num_chars = mtext_len(candidate))) {
    return;
  }

  if (!(buf = alloca(MAX_BYTES(num_chars)))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG, " alloca failed\n");
#endif
    return;
  }

  mconv_reset_converter(m17nlib->mconverter);
  mconv_rebind_buffer(m17nlib->mconverter, buf, MAX_BYTES(num_chars));
  filled_len = mconv_encode(m17nlib->mconverter, candidate);

  if (filled_len == -1) {
    bl_error_printf(
        "Could not convert the encoding of characters in candidates. [error "
        "code: %d]\n",
        merror_code);
    return;
  }

  buf[filled_len] = '\0';
  (*m17nlib->im.cand_screen->set)(m17nlib->im.cand_screen, m17nlib->parser_term, buf, idx);
}

static void preedit_changed(im_m17nlib_t *m17nlib) {
  int filled_len;
  u_char *buf;
  u_int num_chars;
  vt_char_t *p;
  ef_char_t ch;
  u_int pos = 0;

#ifdef IM_M17NLIB_DEBUG
  bl_debug_printf(BL_DEBUG_TAG "\n");
#endif

  /*
   * clear saved preedit
   */

  if (m17nlib->im.preedit.chars) {
    (*syms->vt_str_delete)(m17nlib->im.preedit.chars, m17nlib->im.preedit.num_chars);
    m17nlib->im.preedit.chars = NULL;
  }

  m17nlib->im.preedit.num_chars = 0;
  m17nlib->im.preedit.filled_len = 0;
  m17nlib->im.preedit.segment_offset = 0;
  m17nlib->im.preedit.cursor_offset = UI_IM_PREEDIT_NOCURSOR;

  /*
   * MText -> u_char
   */

  num_chars = mtext_len(m17nlib->input_context->preedit);

  if (!num_chars) {
    goto draw;
  }

  if (!(buf = alloca(MAX_BYTES(num_chars)))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG, " alloca failed\n");
#endif
    return;
  }

  mconv_reset_converter(m17nlib->mconverter);
  mconv_rebind_buffer(m17nlib->mconverter, buf, MAX_BYTES(num_chars));
  filled_len = mconv_encode(m17nlib->mconverter, m17nlib->input_context->preedit);

  if (filled_len == -1) {
    bl_error_printf("Could not convert the preedit string to terminal encoding. [%d]\n",
                    merror_code);
    return;
  }

  /*
   * allocate im.preedit.chars
   */

  if (!(m17nlib->im.preedit.chars = calloc(num_chars, sizeof(vt_char_t)))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG, " calloc failed\n");
#endif
    return;
  }
  m17nlib->im.preedit.num_chars = num_chars;

  /*
   * u_char -> vt_char_t
   */

  p = m17nlib->im.preedit.chars;
  (*syms->vt_str_init)(p, m17nlib->im.preedit.num_chars);

  (*m17nlib->parser_term->init)(m17nlib->parser_term);
  (*m17nlib->parser_term->set_str)(m17nlib->parser_term, (u_char*)buf, filled_len);

  m17nlib->im.preedit.segment_offset = m17nlib->input_context->candidate_from;
  m17nlib->im.preedit.cursor_offset = m17nlib->input_context->cursor_pos;

  while ((*m17nlib->parser_term->next_char)(m17nlib->parser_term, &ch)) {
    vt_color_t fg_color = VT_FG_COLOR;
    vt_color_t bg_color = VT_BG_COLOR;
    int is_underline = 1;
    int is_fullwidth = 0;
    int is_comb = 0;

    if ((*syms->vt_convert_to_internal_ch)(m17nlib->im.vtparser, &ch) <= 0) {
      continue;
    }

    if (ch.property & EF_FULLWIDTH) {
      is_fullwidth = 1;
    } else if (ch.property & EF_AWIDTH) {
      /* TODO: check col_size_of_width_a */
      is_fullwidth = 1;
    }

    if (m17nlib->input_context->candidate_list && m17nlib->input_context->candidate_from <= pos &&
        m17nlib->input_context->candidate_to > pos) {
      fg_color = VT_BG_COLOR;
      bg_color = VT_FG_COLOR;
      is_underline = 0;
    }

    if (ch.property & EF_COMBINING) {
      is_comb = 1;

      if ((*syms->vt_char_combine)(p - 1, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb,
                                   fg_color, bg_color, 0, 0, is_underline, 0, 0, 0)) {
        pos++;
        continue;
      }

      /*
       * if combining failed , char is normally appended.
       */
    }

    (*syms->vt_char_set)(p, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_comb, fg_color, bg_color,
                         0, 0, is_underline, 0, 0, 0);

    pos++;
    p++;
    m17nlib->im.preedit.filled_len++;
  }

draw:

  (*m17nlib->im.listener->draw_preedit_str)(m17nlib->im.listener->self, m17nlib->im.preedit.chars,
                                            m17nlib->im.preedit.filled_len,
                                            m17nlib->im.preedit.cursor_offset);
}

static void candidates_changed(im_m17nlib_t *m17nlib) {
  MPlist *group;
  MPlist *candidate;
  u_int num_candidates = 0;
  int idx;
  int x;
  int y;

#ifdef IM_M17NLIB_DEBUG
  bl_debug_printf(BL_DEBUG_TAG "\n");
#endif

  if (m17nlib->input_context->candidate_list == NULL ||
      m17nlib->input_context->candidate_show == 0) {
    if (m17nlib->im.cand_screen) {
      (*m17nlib->im.cand_screen->delete)(m17nlib->im.cand_screen);
      m17nlib->im.cand_screen = NULL;
    }

    if (m17nlib->im.stat_screen) {
      (*m17nlib->im.stat_screen->show)(m17nlib->im.stat_screen);
    }

    return;
  }

  group = m17nlib->input_context->candidate_list;
  while (mplist_value(group) != Mnil) {
    if (mplist_key(group) == Mtext) {
      num_candidates += mtext_len(mplist_value(group));
    } else {
      num_candidates += mplist_length(mplist_value(group));
    }
    group = mplist_next(group);
  }

#ifdef IM_M17NLIB_DEBUG
  bl_debug_printf(BL_DEBUG_TAG " number of candidates: %d\n", num_candidates);
#endif

  (*m17nlib->im.listener->get_spot)(m17nlib->im.listener->self, m17nlib->im.preedit.chars,
                                    m17nlib->im.preedit.segment_offset, &x, &y);

  if (m17nlib->im.cand_screen == NULL) {
    int is_vertical_direction = 0;

    if (strcmp(msymbol_name(m17nlib->input_method->name), "anthy") == 0) {
      is_vertical_direction = 1;
    }

    if (!(m17nlib->im.cand_screen = (*syms->ui_im_candidate_screen_new)(
              m17nlib->im.disp, m17nlib->im.font_man, m17nlib->im.color_man, m17nlib->im.vtparser,
              (*m17nlib->im.listener->is_vertical)(m17nlib->im.listener->self),
              is_vertical_direction,
              (*m17nlib->im.listener->get_line_height)(m17nlib->im.listener->self), x, y))) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " ui_im_candidate_screen_new() failed.\n");
#endif

      return;
    }

    m17nlib->im.cand_screen->listener.self = m17nlib;
    m17nlib->im.cand_screen->listener.selected = NULL; /* XXX */
  }

  if (!(*m17nlib->im.cand_screen->init)(m17nlib->im.cand_screen, num_candidates, 10)) {
    (*m17nlib->im.cand_screen->delete)(m17nlib->im.cand_screen);
    m17nlib->im.cand_screen = NULL;
    return;
  }

  group = m17nlib->input_context->candidate_list;

  if (mplist_key(group) == Mtext) {
    for (idx = 0; mplist_key(group) != Mnil; group = mplist_next(group)) {
      int i;
      for (i = 0; i < mtext_len(mplist_value(group)); i++) {
        MText *text;
        text = mtext();
        mtext_cat_char(text, mtext_ref_char(mplist_value(group), i));
        set_candidate(m17nlib, text, idx);
        m17n_object_unref(text);
        idx++;
      }
    }
  } else {
    for (idx = 0; mplist_key(group) != Mnil; group = mplist_next(group)) {
      for (candidate = mplist_value(group); mplist_key(candidate) != Mnil;
           candidate = mplist_next(candidate), idx++) {
        set_candidate(m17nlib, mplist_value(candidate), idx);
      }
    }
  }

  (*m17nlib->im.cand_screen->select)(m17nlib->im.cand_screen,
                                     m17nlib->input_context->candidate_index);

  (*m17nlib->im.cand_screen->set_spot)(m17nlib->im.cand_screen, x, y);

  if (m17nlib->im.stat_screen) {
    (*m17nlib->im.stat_screen->hide)(m17nlib->im.stat_screen);
  }
}

/*
 * methods of ui_im_t
 */

static void delete(ui_im_t *im) {
  im_m17nlib_t *m17nlib;

  m17nlib = (im_m17nlib_t*)im;

  ref_count--;

#ifdef IM_M17NLIB_DEBUG
  bl_debug_printf(BL_DEBUG_TAG " An object was deleted. ref_count: %d\n", ref_count);
#endif

  if (m17nlib->input_context) {
    minput_destroy_ic(m17nlib->input_context);
  }

  if (m17nlib->input_method) {
    minput_close_im(m17nlib->input_method);
  }

  if (m17nlib->mconverter) {
    mconv_free_converter(m17nlib->mconverter);
  }

  if (m17nlib->parser_term) {
    (*m17nlib->parser_term->delete)(m17nlib->parser_term);
  }

  if (m17nlib->conv) {
    (*m17nlib->conv->delete)(m17nlib->conv);
  }

  free(m17nlib);

  if (ref_count == 0 && initialized) {
    M17N_FINI();

    initialized = 0;

    if (parser_ascii) {
      (*parser_ascii->delete)(parser_ascii);
      parser_ascii = NULL;
    }
  }
}

static int key_event(ui_im_t *im, u_char key_char, KeySym ksym, XKeyEvent *event) {
  im_m17nlib_t *m17nlib;
  MSymbol mkey;
  MText *text;
  int ret = 1;

  m17nlib = (im_m17nlib_t*)im;

  if (!m17nlib->input_context->active) {
    return 1;
  }

  if ((mkey = xksym_to_msymbol(m17nlib, ksym, event->state)) == Mnil) {
    return 1;
  }

  if (minput_filter(m17nlib->input_context, mkey, Mnil)) {
    ret = 0;
  }

  if (m17nlib->input_context->preedit_changed) {
    preedit_changed(m17nlib);
  }

  if (m17nlib->input_context->candidates_changed) {
    candidates_changed(m17nlib);
  }

  text = mtext();

  if (minput_lookup(m17nlib->input_context, Mnil, NULL, text) == 0) {
    if (mtext_len(text)) {
      commit(m17nlib, text);
      ret = 0;
    }
  }

  m17n_object_unref(text);

  return ret;
}

static int switch_mode(ui_im_t *im) {
  im_m17nlib_t *m17nlib;
  int x;
  int y;

  m17nlib = (im_m17nlib_t*)im;

  (*m17nlib->im.listener->get_spot)(m17nlib->im.listener->self, m17nlib->im.preedit.chars,
                                    m17nlib->im.preedit.segment_offset, &x, &y);

  if (!m17nlib->input_context->active) {
    u_char buf[50];
    u_int filled_len;

    if (m17nlib->im.stat_screen == NULL) {
      if (!(m17nlib->im.stat_screen = (*syms->ui_im_status_screen_new)(
                m17nlib->im.disp, m17nlib->im.font_man, m17nlib->im.color_man,
                m17nlib->im.vtparser,
                (*m17nlib->im.listener->is_vertical)(m17nlib->im.listener->self),
                (*m17nlib->im.listener->get_line_height)(m17nlib->im.listener->self), x, y))) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " ui_im_satus_screen_new() failed.\n");
#endif

        return 1;
      }
    }

    mconv_reset_converter(m17nlib->mconverter);
    mconv_rebind_buffer(m17nlib->mconverter, buf, sizeof(buf));
    filled_len = mconv_encode(m17nlib->mconverter, m17nlib->input_context->status);

    if (filled_len == -1) {
      bl_error_printf("Could not convert the encoding of characters for status. [%d]\n",
                      merror_code);
    } else {
      buf[filled_len] = 0;
      (*m17nlib->im.stat_screen->set)(m17nlib->im.stat_screen, m17nlib->parser_term, buf);
    }
  } else {
    /*
     * commit the last preedit before deactivating the input
     * method.
     */
    if (mtext_len(m17nlib->input_context->preedit)) {
      commit(m17nlib, m17nlib->input_context->preedit);
    }

    /*
     * initialize the state of MinputContext.
     * <http://sf.net/mailarchive/message.php?msg_id=9958816>
     */
    minput_filter(m17nlib->input_context, Mnil, Mnil);

    /*
     * clear saved preedit
     */

    if (m17nlib->im.preedit.chars) {
      (*syms->vt_str_delete)(m17nlib->im.preedit.chars, m17nlib->im.preedit.num_chars);
      m17nlib->im.preedit.chars = NULL;
    }

    m17nlib->im.preedit.num_chars = 0;
    m17nlib->im.preedit.filled_len = 0;
    m17nlib->im.preedit.segment_offset = 0;
    m17nlib->im.preedit.cursor_offset = UI_IM_PREEDIT_NOCURSOR;

    if (m17nlib->im.stat_screen) {
      (*m17nlib->im.stat_screen->delete)(m17nlib->im.stat_screen);
      m17nlib->im.stat_screen = NULL;
    }

    if (m17nlib->im.cand_screen) {
      (*m17nlib->im.cand_screen->delete)(m17nlib->im.cand_screen);
      m17nlib->im.cand_screen = NULL;
    }
  }

  minput_toggle(m17nlib->input_context);

  return 1;
}

static int is_active(ui_im_t *im) {
  im_m17nlib_t *m17nlib;

  m17nlib = (im_m17nlib_t*)im;

  return m17nlib->input_context->active;
}

static void focused(ui_im_t *im) {
  im_m17nlib_t *m17nlib;

  m17nlib = (im_m17nlib_t*)im;

  if (m17nlib->im.cand_screen) {
    (*m17nlib->im.cand_screen->show)(m17nlib->im.cand_screen);
  } else if (m17nlib->im.stat_screen) {
    (*m17nlib->im.stat_screen->show)(m17nlib->im.stat_screen);
  }
}

static void unfocused(ui_im_t *im) {
  im_m17nlib_t *m17nlib;

  m17nlib = (im_m17nlib_t*)im;

  if (m17nlib->im.stat_screen) {
    (*m17nlib->im.stat_screen->hide)(m17nlib->im.stat_screen);
  }

  if (m17nlib->im.cand_screen) {
    (*m17nlib->im.cand_screen->hide)(m17nlib->im.cand_screen);
  }
}

/* --- global functions --- */

ui_im_t *im_m17nlib_new(u_int64_t magic, vt_char_encoding_t term_encoding,
                        ui_im_export_syms_t *export_syms,
                        char *param, /* <language>:<input method> */
                        u_int mod_ignore_mask) {
  im_m17nlib_t *m17nlib;
  char *encoding_name;
  MSymbol encoding_sym;

  if (magic != (u_int64_t)IM_API_COMPAT_CHECK_MAGIC) {
    bl_error_printf("Incompatible input method API.\n");

    return NULL;
  }

  m17nlib = NULL;

#if 1
#define RESTORE_LOCALE
#endif

  if (!initialized) {
#ifdef RESTORE_LOCALE
    /*
     * Workaround against make_locale() of m17nlib.
     */
    char *cur_locale;
    cur_locale = bl_str_alloca_dup(bl_get_locale());
#endif

    M17N_INIT();

#ifdef RESTORE_LOCALE
    /* restoring */
    /*
     * TODO: remove valgrind warning.
     * The memory space pointed to by sys_locale in bl_locale.c
     * was freed by setlocale() in m17nlib.
     */
    bl_locale_init(cur_locale);
#endif

    if (merror_code != MERROR_NONE) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG "failed to initialize m17n library\n");
#endif
      goto error;
    }

    syms = export_syms;

    initialized = 1;

    if (!(parser_ascii = (*syms->vt_char_encoding_parser_new)(VT_ISO8859_1))) {
      goto error;
    }
  }

#ifdef IM_M17NLIB_DEBUG
  show_available_ims();
#endif

  if (!(m17nlib = malloc(sizeof(im_m17nlib_t)))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif
    goto error;
  }

  m17nlib->input_method = NULL;
  m17nlib->input_context = NULL;
  m17nlib->mconverter = NULL;
  m17nlib->parser_term = NULL;
  m17nlib->conv = NULL;

  if (!(m17nlib->input_method = find_input_method(param))) {
    bl_error_printf("Could not find %s\n", param);
    goto error;
  }

  if (!(m17nlib->input_context = minput_create_ic(m17nlib->input_method, NULL))) {
    bl_error_printf("Could not crate context for %s\n", param);
    goto error;
  }

  if (term_encoding == VT_EUCJISX0213) {
    encoding_name = (*syms->vt_get_char_encoding_name)(VT_EUCJP);
  } else {
    encoding_name = (*syms->vt_get_char_encoding_name)(term_encoding);
  }

  if ((encoding_sym = mconv_resolve_coding(msymbol(encoding_name))) == Mnil) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " Could not resolve encoding name [%s]\n", encoding_name);
#endif
    goto error;
  }

  if (!(m17nlib->mconverter = mconv_buffer_converter(encoding_sym, NULL, 0))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " Could not create MConverter\n");
#endif
    goto error;
  }

  if (!(m17nlib->conv = (*syms->vt_char_encoding_conv_new)(term_encoding))) {
    goto error;
  }

  if (!(m17nlib->parser_term = (*syms->vt_char_encoding_parser_new)(term_encoding))) {
    goto error;
  }

  minput_toggle(m17nlib->input_context);

  /*
   * set methods of ui_im_t
   */
  m17nlib->im.delete = delete;
  m17nlib->im.key_event = key_event;
  m17nlib->im.switch_mode = switch_mode;
  m17nlib->im.is_active = is_active;
  m17nlib->im.focused = focused;
  m17nlib->im.unfocused = unfocused;

  ref_count++;

#ifdef IM_M17NLIB_DEBUG
  bl_debug_printf("New object was created. ref_count is %d.\n", ref_count);
#endif

  return (ui_im_t*)m17nlib;

error:
  if (m17nlib) {
    if (m17nlib->input_context) {
      minput_destroy_ic(m17nlib->input_context);
    }

    if (m17nlib->mconverter) {
      mconv_free_converter(m17nlib->mconverter);
    }

    if (m17nlib->input_method) {
      minput_close_im(m17nlib->input_method);
    }

    if (m17nlib->parser_term) {
      (*m17nlib->parser_term->delete)(m17nlib->parser_term);
    }

    if (m17nlib->conv) {
      (*m17nlib->conv->delete)(m17nlib->conv);
    }

    free(m17nlib);
  }

  if (initialized && ref_count == 0) {
    M17N_FINI();

    if (parser_ascii) {
      (*parser_ascii->delete)(parser_ascii);
      parser_ascii = NULL;
    }

    initialized = 0;
  }

  return NULL;
}

/* --- API for external tools --- */

im_info_t *im_m17nlib_get_info(char *locale, char *encoding) {
  im_info_t *result = NULL;
  MPlist *im_list;
  MSymbol sym_im;
  int i;
  int num_ims;
  int auto_idx = 0;

  M17N_INIT();

  sym_im = msymbol("input-method");
  im_list = mdatabase_list(sym_im, Mnil, Mnil, Mnil);
  num_ims = mplist_length(im_list);

  if (num_ims == 0) {
    goto error;
  }

  if (!(result = malloc(sizeof(im_info_t)))) {
    goto error;
  }

  result->num_args = num_ims + 1;
  if (!(result->args = calloc(result->num_args, sizeof(char*)))) {
    goto error;
  }

  if (!(result->readable_args = calloc(result->num_args, sizeof(char*)))) {
    free(result->args);
    goto error;
  }

  for (i = 1; i < result->num_args; i++, im_list = mplist_next(im_list)) {
    MDatabase *db;
    MSymbol *tag;
    size_t len;
    char *lang;
    char *im;

    db = mplist_value(im_list);
    tag = mdatabase_tag(db);

    lang = msymbol_name(tag[1]);
    im = msymbol_name(tag[2]);

    len = strlen(im) + strlen(lang) + 4;

    if ((result->args[i] = malloc(len))) {
      bl_snprintf(result->args[i], len, "%s:%s", lang, im);
    } else {
      result->args[i] = strdup("error");
    }

    if ((result->readable_args[i] = malloc(len))) {
      bl_snprintf(result->readable_args[i], len, "%s (%s)", lang, im);
    } else {
      result->readable_args[i] = strdup("error");
    }

    if (strncmp(lang, locale, 2) == 0 && auto_idx == 0) {
      auto_idx = i;
    }
  }

  result->args[0] = strdup("");
  if (auto_idx) {
    result->readable_args[0] = strdup(result->readable_args[auto_idx]);
  } else {
    result->readable_args[0] = strdup("unknown");
  }

  M17N_FINI();

  result->id = strdup("m17nlib");
  result->name = strdup("m17n library");

  return result;

error:
  M17N_FINI();

  if (result) {
    free(result);
  }

  if (parser_ascii) {
    (*parser_ascii->delete)(parser_ascii);
    parser_ascii = NULL;
  }

  return NULL;
}
