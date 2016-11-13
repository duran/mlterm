/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_SCREEN_MANAGER_H__
#define __UI_SCREEN_MANAGER_H__

#include <stdio.h> /* FILE */
#include "ui_screen.h"
#include "ui_main_config.h"

int ui_screen_manager_init(char* mlterm_version, u_int depth, u_int max_screens_multiple,
                           u_int num_of_startup_screens, ui_main_config_t* main_config);

int ui_screen_manager_final(void);

u_int ui_screen_manager_startup(void);

int ui_close_dead_screens(void);

u_int ui_get_all_screens(ui_screen_t*** _screens);

int ui_mlclient(char* args, FILE* fp);

#if defined(__ANDROID__) && defined(USE_LIBSSH2)
void ui_set_start_with_local_pty(int flag);
#endif

#endif
