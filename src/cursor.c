/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Cursor pixmap management
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>
#include "common.h"
#include "cursor.h"
#include "debug.h"

/* Cursor pixmaps */
#include "bitmaps/dragptr.bm"
#include "bitmaps/dragptr_m.bm"
#include "bitmaps/grabptr.bm"
#include "bitmaps/grabptr_m.bm"
#include "bitmaps/hgptr.bm"
#include "bitmaps/hgptr_m.bm"
#include "bitmaps/dragptr.l.bm"
#include "bitmaps/dragptr_m.l.bm"
#include "bitmaps/grabptr.l.bm"
#include "bitmaps/grabptr_m.l.bm"
#include "bitmaps/hgptr.l.bm"
#include "bitmaps/hgptr_m.l.bm"

static Cursor get_cursor(Display *dpy, int id);
static Cursor build_cursor(Display *dpy, unsigned char *bits,
	unsigned char *mask, short width, short height, short hspx, short hspy);

static Cursor cache[(_NUM_CURSORS-1)]={0};

/*
 * Set cursor for the given widget
 */
void set_widget_cursor(Widget w, enum cursors id)
{
	if(id==CUR_POINTER)
		XUndefineCursor(XtDisplay(w),XtWindow(w));
	else
		XDefineCursor(XtDisplay(w),XtWindow(w),get_cursor(XtDisplay(w),id));
}

/*
 * Retrieve a cursor from the cache, create if it doesn't yet exist.
 */
static Cursor get_cursor(Display *dpy, int id)
{
	if(cache[id]==0){
		switch(id){
			case CUR_GRAB:
			if(init_app_res.large_cursors){
				cache[CUR_GRAB]=build_cursor(dpy,grabptr_l_bits,
					grabptr_l_m_bits,grabptr_l_width,grabptr_l_height,
					grabptr_l_x_hot,grabptr_l_y_hot);
			} else {
				cache[CUR_GRAB]=build_cursor(dpy,grabptr_bits,grabptr_m_bits,
					grabptr_width,grabptr_height,grabptr_x_hot,grabptr_y_hot);
			}
			break;
			case CUR_DRAG:
			if(init_app_res.large_cursors){
				cache[CUR_DRAG]=build_cursor(dpy,dragptr_l_bits,
					dragptr_l_m_bits, dragptr_l_width,dragptr_l_height,
					dragptr_l_x_hot,dragptr_l_y_hot);
			} else {
				cache[CUR_DRAG]=build_cursor(dpy,dragptr_bits,dragptr_m_bits,
					dragptr_width,dragptr_height,dragptr_x_hot,dragptr_y_hot);
			}
			break;
			case CUR_XARROWS:
			cache[CUR_XARROWS]=XCreateFontCursor(dpy,XC_fleur);
			break;
			case CUR_HOURGLAS:
			if(init_app_res.large_cursors){
				cache[CUR_HOURGLAS]=build_cursor(dpy,hgptr_l_bits,
					hgptr_l_m_bits,	hgptr_l_width,hgptr_l_height,
					hgptr_l_x_hot,hgptr_l_y_hot);
			} else {
				cache[CUR_HOURGLAS]=build_cursor(dpy,hgptr_bits,hgptr_m_bits,
					hgptr_width,hgptr_height,hgptr_x_hot,hgptr_y_hot);
			}
			break;
		}
	}
	return cache[id];	
}

/*
 * Build an X cursor from the bits/mask data.
 */
static Cursor build_cursor(Display *dpy, unsigned char *bits,
	unsigned char *mask, short width, short height, short hspx, short hspy)
{
	Cursor cursor;
	XColor fore, back;
	Pixmap pm_bits, pm_mask;
	Colormap cmap;
	Window drawable;
	int scr;
	
	scr=XDefaultScreen(dpy);
	drawable=XDefaultRootWindow(dpy);
	back.pixel=WhitePixel(dpy,scr);
	fore.pixel=BlackPixel(dpy,scr);
	cmap=XDefaultColormap(dpy,scr);
	XQueryColor(dpy,cmap,&fore);
	XQueryColor(dpy,cmap,&back);
	
	pm_bits=XCreateBitmapFromData(dpy,drawable,(char*)bits,width,height);
	pm_mask=XCreateBitmapFromData(dpy,drawable,(char*)mask,width,height);
	
	cursor=XCreatePixmapCursor(dpy,pm_bits,pm_mask,&fore,&back,hspx,hspy);
	
	XFreePixmap(dpy,pm_bits);
	XFreePixmap(dpy,pm_mask);
	return cursor;
}
