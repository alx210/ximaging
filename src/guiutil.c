/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * GUI utility functions
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <Xm/Xm.h>
#ifdef ENABLE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include "common.h"
#include "guiutil.h"
#include "debug.h"

/* Set input focus to 'w' */
void raise_and_focus(Widget w)
{
	static Atom XaNET_ACTIVE_WINDOW=None;
	static Atom XaWM_STATE=None;
	static Atom XaWM_CHANGE_STATE=None;
	Atom ret_type;
	int ret_fmt;
	unsigned long ret_items;
	unsigned long ret_bytes;
	uint32_t *state=NULL;
	XClientMessageEvent evt;

	if(XaWM_STATE==None){
		XaWM_STATE=XInternAtom(app_inst.display,"WM_STATE",True);
		XaWM_CHANGE_STATE=XInternAtom(app_inst.display,"WM_CHANGE_STATE",True);
		XaNET_ACTIVE_WINDOW=XInternAtom(app_inst.display,
			"_NET_ACTIVE_WINDOW",True);
	}

	if(XaWM_STATE==None) return;

	if(XGetWindowProperty(app_inst.display,XtWindow(w),XaWM_STATE,0,1,
		False,XaWM_STATE,&ret_type,&ret_fmt,&ret_items,
		&ret_bytes,(unsigned char**)&state)!=Success) return;
	if(ret_type==XaWM_STATE && ret_fmt && *state==IconicState){
		evt.type=ClientMessage;
		evt.send_event=True;
		evt.message_type=XaWM_CHANGE_STATE;
		evt.window=XtWindow(w);
		evt.format=32;
		evt.data.l[0]=NormalState;
		XSendEvent(app_inst.display,XDefaultRootWindow(app_inst.display),True,
			SubstructureNotifyMask|SubstructureRedirectMask,
			(XEvent*)&evt);
	}else{
		if(XaNET_ACTIVE_WINDOW){
			evt.type=ClientMessage,
			evt.send_event=True;
			evt.serial=0;
			evt.display=app_inst.display;
			evt.window=XtWindow(w);
			evt.message_type=XaNET_ACTIVE_WINDOW;
			evt.format=32;

			XSendEvent(app_inst.display,
				XDefaultRootWindow(app_inst.display),False,
				SubstructureNotifyMask|SubstructureRedirectMask,(XEvent*)&evt);
		}else{
			XRaiseWindow(app_inst.display,XtWindow(w));
			XSync(app_inst.display,False);
			XSetInputFocus(app_inst.display,XtWindow(w),
				RevertToParent,CurrentTime);
		}
	}
	XFree((char*)state);
}

/* Remove PPosition hint and map a shell widget.
 * The widget must be realized with mappedWhenManaged set to False */
void map_shell_unpositioned(Widget wshell)
{
	XSizeHints size_hints;
	long supplied_hints;

	if(XGetWMNormalHints(XtDisplay(wshell),XtWindow(wshell),
		&size_hints,&supplied_hints)){
		
		size_hints.flags &= ~PPosition;
		XSetWMNormalHints(XtDisplay(wshell),XtWindow(wshell),&size_hints);
	}
	XtMapWidget(wshell);
}

/* Build a masked icon pixmap from the xbm data */
void load_icon(const void *bits, const void *mask_bits,
	unsigned int width, unsigned int height, Pixmap *icon, Pixmap *mask)
{
	Pixel fg_color=0, bg_color=0;
	Window root;
	int depth, screen;
	Screen *pscreen;
	
	pscreen=XtScreen(app_inst.session_shell);
	screen=XScreenNumberOfScreen(pscreen);
	root=RootWindowOfScreen(pscreen);
	depth=DefaultDepth(app_inst.display,screen);
	fg_color=XBlackPixel(app_inst.display,screen);
	bg_color=XWhitePixel(app_inst.display,screen);
	*icon=XCreatePixmapFromBitmapData(app_inst.display,root,
		(char*)bits,width,height,fg_color,bg_color,depth);
	*mask=XCreatePixmapFromBitmapData(app_inst.display,root,
		(char*)mask_bits,width,height,1,0,1);
}

/*
 * Build and color a pixmap according to specified back/foreground colors
 */
Pixmap load_bitmap(const unsigned char *bits, unsigned int width,
	unsigned int height, Pixel fg, Pixel bg)
{
	Window root;
	Pixmap pm;
	int depth, screen;
	Screen *pscreen;
	
	pscreen=XtScreen(app_inst.session_shell);
	screen=XScreenNumberOfScreen(pscreen);
	root=RootWindowOfScreen(pscreen);
	depth=DefaultDepth(app_inst.display,screen);
	pm=XCreatePixmapFromBitmapData(app_inst.display,root,
		(char*)bits,width,height,fg,bg,depth);
	return pm;
}

/* 
 * Build and color a pixmap according to widget's back/foreground colors
 */
Pixmap load_widget_bitmap(Widget w, const unsigned char *bits,
	unsigned int width, unsigned int height)
{
	Pixel fg=0, bg=0, ns=0;
	Pixmap pm;
	
	XtVaGetValues(w,XmNforeground,&fg,
		XmNbackground,&bg,XmNbottomShadowColor,&ns,NULL);
		
	pm=load_bitmap(bits,width,height,fg,bg);
	return pm;
}

/*
 * Returns size and x/y offsets of the screen the widget is located on.
 */
void get_screen_size(Widget w, int *pwidth, int *pheight, int *px, int *py)
{
	Screen *scr;
	
	#ifdef ENABLE_XINERAMA
	if(XineramaIsActive(app_inst.display)){
		Position wx,wy;
		int i,nxis;
		XineramaScreenInfo *xis;
		
		xis=XineramaQueryScreens(app_inst.display,&nxis);
		while(w && !XtIsShell(w)) w=XtParent(w);
		XtVaGetValues(w,XmNx,&wx,XmNy,&wy,NULL);
		
		for(i=0; i<nxis; i++){
			if((wx>=xis[i].x_org && wx<=(xis[i].x_org+xis[i].width)) &&
				(wy>=xis[i].y_org && wy<=(xis[i].y_org+xis[i].height)))
				break;
		}
		
		if(i<nxis){
			*pwidth=xis[i].width;
			*pheight=xis[i].height;
			*px=xis[i].x_org;
			*py=xis[i].y_org;
			XFree(xis);
			return;
		}
		XFree(xis);
	}
	#endif /* ENABLE_XINERAMA */
	
	scr=XtScreen(w);
	*pwidth=XWidthOfScreen(scr);
	*pheight=XHeightOfScreen(scr);
	*px=0;
	*py=0;
}

/* Convert string to double (locale independent) */
double str_to_double(const char *str)
{
	double n;
	double sum=0;
	double expt;
	size_t i, len;
	char *ptr;
	int sign=0;

	while(isspace(*str)) str++;

	if(str[0]=='-'){
		sign=1;
		str++;
	}else if(str[0]=='+') str++;
	
	len=strlen(str);
	expt=(double)len-1;
	ptr=strchr(str,'.');
	if(!ptr) ptr=strchr(str,',');
	if(ptr) expt-=(double)strlen(ptr+1)+1;

	for(i=0; str[i] != 0 ; i++, expt--){
		if(str[i] < '0' || str[i] > '9'){
			if(str[i]=='.' || str[i]==','){
				expt++;
				continue;
			}
			return 0;
		}
		n=(double)(str[i]-'0')*pow(10,expt);
		sum+=n;
	}
	if(sign) sum= -sum;
	
	return sum;
}

/* Shortens a multibyte string to max_chrs */
char* shorten_mb_string(const char *sz, size_t max_chrs, Boolean ltor)
{
	size_t nbytes = strlen(sz);
	size_t nchrs = 0;
	size_t i = 0;
	char *result;
	
	mblen(NULL, 0);
	
	while(sz[i]) {
		int n = mblen(sz + i, nbytes - i);
		if(n <= 0) break;
		nchrs++;
		i += n;
	}

	if(nchrs <= max_chrs)
		return strdup(sz);
	
	mblen(NULL, 0);
	
	if(ltor) {
		int i;
		size_t n, nskip = nchrs - (max_chrs - 3);
		
		for(n = 0, i = 0; n < nskip; n++) { 
			i += mblen(sz + i, nbytes - i);
		}
		nbytes = strlen(sz + i);
		result = malloc(nbytes + 4);
		if(!result) return NULL;
		sprintf(result, "...%s", sz + i);
	} else {
		int i;
		size_t n, nskip = nchrs - (nchrs - (max_chrs - 3));
		
		for(n = 0, i = 0; n < nskip; n++) { 
			i += mblen(sz + i, nbytes - i);
		}
		result = malloc(i + 4);
		if(!result) return NULL;
		strncpy(result, sz, i);
		result[i] = '\0';
		strcat(result, "...");	
	}
	return result;
}

/* Returns number of characters in a multibyte string */
size_t mb_strlen(const char *sz)
{
	size_t nbytes = strlen(sz);
	size_t nchrs = 0;
	size_t i = 0;
	
	if(!nbytes) return 0;
	
	mblen(NULL, 0);
	
	while(sz[i]) {
		int n = mblen(sz + i, nbytes - i);
		if(n <= 0) break;
		nchrs++;
		i += n;
	}
	return nchrs;
}

char* get_size_string(unsigned long size, char buffer[SIZE_CS_MAX])
{
	char CS_BYTES[] = "B";
	char CS_KILO[] = "K";
	char CS_MEGA[] = "M";
	char CS_GIGA[] = "G";

	const double kilo = 1024;
	double fsize = size;
	char *sz_units = CS_BYTES;
	double dp;
	char *fmt;

	
	if(size >= pow(kilo, 3)) {
		fsize /=  pow(kilo, 3);
		sz_units = CS_GIGA;
	}else if(size >=  pow(kilo, 2)) {
		fsize /= pow(kilo, 2);
		sz_units = CS_MEGA;
	} else if(size >= kilo) {
		fsize /= kilo;
		sz_units = CS_KILO;
	}

	/* don't show decimal part if it's near .0 */
	dp = fsize - trunc(fsize);
	if(dp > 0.1 && dp < 0.9)
		fmt = "%.1f%s";
	else
		fmt = "%.0f%s";

	snprintf(buffer, SIZE_CS_MAX, fmt, fsize, sz_units);
	return buffer;
}
