/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * GUI utility functions
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <Xm/Xm.h>
#ifdef ENABLE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include "common.h"
#include "debug.h"

/* Set input focus to 'w' */
void raise_and_focus(Widget w)
{
	static Atom XaWM_STATE=None;
	static Atom XaWM_CHANGE_STATE=None;
	Atom ret_type;
	int ret_fmt;
	unsigned long ret_items;
	unsigned long ret_bytes;
	uint32_t *state=NULL;
	
	if(XaWM_STATE==None){
		XaWM_STATE=XInternAtom(app_inst.display,"WM_STATE",False);
		XaWM_CHANGE_STATE=XInternAtom(app_inst.display,"WM_CHANGE_STATE",False);
	}
	if(XGetWindowProperty(app_inst.display,XtWindow(w),XaWM_STATE,0,1,
		False,XaWM_STATE,&ret_type,&ret_fmt,&ret_items,
		&ret_bytes,(unsigned char**)&state)!=Success) return;
	if(ret_type==XaWM_STATE && ret_fmt && *state==IconicState){
		XClientMessageEvent evt;
		evt.type=ClientMessage;
		evt.message_type=XaWM_CHANGE_STATE;
		evt.window=XtWindow(w);
		evt.format=32;
		evt.data.l[0]=NormalState;
		XSendEvent(app_inst.display,XDefaultRootWindow(app_inst.display),True,
			SubstructureNotifyMask|SubstructureRedirectMask,
			(XEvent*)&evt);
	}else{
		XRaiseWindow(app_inst.display,XtWindow(w));
		XSync(app_inst.display,False);
		XSetInputFocus(app_inst.display,XtWindow(w),RevertToParent,CurrentTime);
	}
	XFree((char*)state);
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
