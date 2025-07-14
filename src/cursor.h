/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#ifndef CURSOR_H
#define CURSOR_H

enum cursors {
	CUR_DRAG,
	CUR_GRAB,
	CUR_XARROWS,
	CUR_HOURGLAS,
	CUR_POINTER,
	_NUM_CURSORS
};

/*
 * Set cursor for the given widget
 */
void set_widget_cursor(Widget w, enum cursors id);

#endif /* CURSOR_H */
