/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#ifndef TOOLBAR_H
#define TOOLBAR_H

enum tb_type {
	TB_PUSH,
	TB_TOGGLE,
	TB_SEP
};

struct toolbar_item {
	enum tb_type type;
	char *name;
	unsigned char *bitmap;
	short bm_width;
	short bm_height;
	XtCallbackProc callback;
};

/*
 * Builds a toolbar and returns its container widget, which is left unmanaged.
 * All buttons pass cb_data to their respective callbacks.
 */
Widget create_toolbar(Widget parent,
	struct toolbar_item *items, size_t nitems, void *cb_data);

Widget get_toolbar_item(Widget toolbar, const char *name);

#endif /* TOOLBAR_H */
