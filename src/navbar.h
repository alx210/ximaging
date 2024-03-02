/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#ifndef NAVBAR_H
#define NAVBAR_H

#include <Xm/Xm.h>

typedef void (*navbar_cb)(const char*,void*);

/*
 * Create navigation bar widgets. Return their container widget
 * handle, which isn't managed yet.
 */
Widget create_navbar(Widget parent, navbar_cb callback, void *cb_data);

/*
 * Set the path to be displayed in the navigation bar.
 * Returns zero on success, errno otherwise.
 */
int set_navbar_path(Widget navbar, const char *path);

#endif /* NAVBAR_H */
