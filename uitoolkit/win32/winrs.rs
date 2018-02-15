/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

MLTERM_ICON ICON "mlterm-icon-win32.ico"

#include  <pobl/bl_def.h>		/* USE_WIN32API */

#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)

#ifdef USE_SDL2
#define USE_WIN32GUI
#undef USE_SDL2
#include  "../win32/ui_connect_dialog.h"
#else
#include  "ui_connect_dialog.h"
#endif

ConnectDialog DIALOG 20, 20, 138, 139
	STYLE WS_POPUP | WS_DLGFRAME | DS_CENTER
	{
		LTEXT		"List",		-1,		4,  4,  20,  8
		COMBOBOX			IDD_LIST,	24, 4,  106, 48, CBS_SORT | CBS_DROPDOWNLIST | WS_GROUP | WS_TABSTOP
		GROUPBOX	"Protocol",	-1,		4,  20, 126, 24
		RADIOBUTTON	"&SSH",		IDD_SSH,	8,  30, 40,  12
		RADIOBUTTON	"&TELNET",	IDD_TELNET,	48, 30, 40,  12
		RADIOBUTTON	"&RLOGIN",	IDD_RLOGIN,	88, 30, 40,  12
		LTEXT		"Server",	-1,		4,  46, 30,  8
		EDITTEXT			IDD_SERVER,	38, 46, 96, 10, ES_AUTOHSCROLL
		LTEXT		"Port",		-1,		4,  57, 30,  8
		EDITTEXT			IDD_PORT,	38, 57, 96, 10, ES_AUTOHSCROLL
		LTEXT		"User",		-1,		4,  68, 30,  8
		EDITTEXT			IDD_USER,	38, 68, 96, 10, ES_AUTOHSCROLL
		LTEXT		"Pass",		-1,		4,  79, 30,  8
		EDITTEXT			IDD_PASS,	38, 79, 96, 10, ES_PASSWORD | ES_AUTOHSCROLL
		LTEXT		"Encoding",	-1,		4,  90, 30,  8
		EDITTEXT			IDD_ENCODING,	38, 90, 96, 10, ES_AUTOHSCROLL
		LTEXT		"ExecCmd",	-1,		4, 101, 30,  8
		EDITTEXT			IDD_EXEC_CMD,	38,101, 96, 10, ES_AUTOHSCROLL
		CONTROL		"X11 forwarding", IDD_X11,	"Button", BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP, 4, 112, 126, 8
		DEFPUSHBUTTON	"OK",		IDOK,		20,124, 40, 12
		PUSHBUTTON	"Cancel",	IDCANCEL,	80,124, 40, 12
	}

#endif	/* USE_WIN32API */
