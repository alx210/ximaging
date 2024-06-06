/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#ifndef GUIUTIL_H
#define GUIUTIL_H

/* Set input focus to 'w' */
void raise_and_focus(Widget w);

/*
 * Build and color a pixmap according to specified back/foreground colors
 */
Pixmap load_bitmap(const unsigned char *bits, unsigned int width,
	unsigned int height, Pixel fg, Pixel bg);

/* 
 * Remove PPosition hint and map a shell widget.
 * The widget must be realized with mappedWhenManaged set to False 
 */
void map_shell_unpositioned(Widget wshell);

/* Build a masked icon pixmap from xbm data */
void load_icon(const void *bits, const void *mask_bits,
	unsigned int width, unsigned int height, Pixmap *icon, Pixmap *mask);

/* 
 * Build and color a pixmap according to widget's back/foreground colors
 */
Pixmap load_widget_bitmap(Widget w, const unsigned char *bits,
	unsigned int width, unsigned int height);

/*
 * Create a pixmap with the masked area colored according to w's background
 */
Pixmap load_masked_bitmap(Widget w, const unsigned char *bits,
	const unsigned char *mask_bits, unsigned int width, unsigned int height);

/* Returns size and x/y offsets of the screen the widget is located on */
void get_screen_size(Widget w, int *pwidth,
	int *pheight, int *px, int *py);

/* Convert string to double (locale independent) */
double str_to_double(const char *str);

/* Shortens a multibyte string to max_chrs */
char* shorten_mb_string(const char *sz, size_t max_chrs, Boolean ltor);

/* Returns number of characters in a multibyte string */
size_t mb_strlen(const char *sz);

#define SIZE_CS_MAX 32
char* get_size_string(unsigned long size, char buffer[SIZE_CS_MAX]);

#endif /* GUIUTIL_H */
