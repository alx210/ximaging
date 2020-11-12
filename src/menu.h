/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Constants and prototypes for menu construction routines.
 */

#ifndef MENU_H
#define MENU_H

/* Menu item types */
enum item_type { IT_PUSH, IT_TOGGLE, IT_RADIO, IT_SEP };

struct menu_item {
	enum item_type type;
	char *name;			/* widget name */
	char *label;		/* item text (prefix mnemonic character with _ ) */
	int label_sid;		/* label string ID */
	
	void (*callback)(Widget,XtPointer,XtPointer); /* activation callback */
};

/*
 * Create a pulldown menu and attach it to the given menubar widget.
 * First item in 'items' array specifies the cascade button. A message set
 * for menu item label string IDs must be specified in msg_set. If is_help
 * is True, the pulldown will be right-aligned. User data for callbacks may
 * be specified in cbc_data.
 */
Widget create_pulldown(Widget mbar, const struct menu_item *items,
	int nitems, const void *cbc_data, int msg_set, Bool is_help);

/*
 * Create a popup menu. A message set for menu item label string IDs must
 * be specified in msg_set. User data for callbacks may be specified
 * in cbc_data.
 */
Widget create_popup(Widget wparent, const char *name,
	const struct menu_item *items, int nitems,
	const void *cbc_data, int msg_set);

#endif /* MENU_H */
