/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#include <stdlib.h>
#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/Label.h>
#include <Xm/Frame.h>
#include "const.h"
#include "common.h"
#include "toolbar.h"
#include "debug.h"

struct toolbar_data {
	Widget wparent;
	Widget wframe;
	Widget wrowcol;
};

static void make_button_pixmaps(Widget btn, const unsigned char *bits,
	unsigned int width, unsigned int height, Pixmap *normal,
	Pixmap *armed, Pixmap *grayed);

/*
 * Builds a toolbar and returns its container widget, which is left unmanaged.
 * All buttons pass cb_data to their respective callbacks.
 */
Widget create_toolbar(Widget parent,
	struct toolbar_item *items, size_t nitems, void *cbdata)
{
	Cardinal n=0;
	Arg args[10];
	struct toolbar_data *tbd;
	Dimension border_width=0;
	size_t i;
	
	tbd=calloc(1,sizeof(struct toolbar_data));
	if(!tbd) return None;
	
	tbd->wparent=parent;

	XtVaGetValues(XmGetXmDisplay(app_inst.display),
		XmNenableThinThickness,&border_width,NULL);
	border_width=border_width?1:2;
		
	n=0;
	XtSetArg(args[n],XmNuserData,(XtPointer)tbd); n++;
	XtSetArg(args[n],XmNshadowThickness,(border_width?1:2)); n++;
	XtSetArg(args[n],XmNshadowType,XmSHADOW_OUT); n++;
	XtSetArg(args[n],XmNresizeHeight,True); n++;
	tbd->wframe=XmCreateFrame(parent,"toolbar",args,n);
	n=0;
	XtSetArg(args[n],XmNorientation,XmHORIZONTAL); n++;
	XtSetArg(args[n],XmNpacking,XmPACK_TIGHT); n++;
	XtSetArg(args[n],XmNspacing,1); n++;
	XtSetArg(args[n],XmNmarginWidth,1); n++;
	XtSetArg(args[n],XmNmarginHeight,1); n++;
	XtSetArg(args[n],XmNtraversalOn,False); n++;
	tbd->wrowcol=XmCreateRowColumn(tbd->wframe,"buttons",args,n);
	
	for(i=0; i<nitems; i++){
		Widget w;
		Pixmap normal,armed,grayed;
		
		if(items[i].type==TB_SEP){
			n=0;
			XtSetArg(args[n],XmNmargin,border_width*2); n++;
			XtSetArg(args[n],XmNseparatorType,XmNO_LINE); n++;
			XtSetArg(args[n],XmNorientation,XmVERTICAL); n++;
			XtSetArg(args[n],XmNwidth,3*border_width); n++;
			w=XmCreateSeparatorGadget(tbd->wrowcol,"sep",args,n);
			XtManageChild(w);
			continue;
		}
		
		make_button_pixmaps(tbd->wframe,items[i].bitmap,
			items[i].bm_width,items[i].bm_height,&normal,&armed,&grayed);
		
		n=0;
		XtSetArg(args[n],XmNlabelPixmap,normal); n++;
		XtSetArg(args[n],XmNlabelInsensitivePixmap,grayed); n++;
		XtSetArg(args[n],XmNlabelType,XmPIXMAP); n++;
		XtSetArg(args[n],XmNmarginWidth,2); n++;
		XtSetArg(args[n],XmNmarginHeight,2); n++;
		if(items[i].type==TB_PUSH){
			XtSetArg(args[n],XmNfillOnArm,False); n++;
			XtSetArg(args[n],XmNarmPixmap,armed); n++;
			w=XmCreatePushButtonGadget(tbd->wrowcol,items[i].name,args,n);
			XtAddCallback(w,XmNactivateCallback,items[i].callback,cbdata);
		}else{
			XtSetArg(args[n],XmNfillOnSelect,True); n++;
			XtSetArg(args[n],XmNshadowThickness,border_width); n++;
			XtSetArg(args[n],XmNindicatorOn,False); n++;
			XtSetArg(args[n],XmNselectPixmap,armed); n++;
			w=XmCreateToggleButtonGadget(tbd->wrowcol,items[i].name,args,n);
			XtAddCallback(w,XmNvalueChangedCallback,items[i].callback,cbdata);
		}

		XtManageChild(w);
	}
	XtManageChild(tbd->wrowcol);
	
	return tbd->wframe;
}

/* 
 * Build and color pixmaps according to widget back/foreground 
 * so these will appear consistent with user's preferred color scheme
 */
static void make_button_pixmaps(Widget w, const unsigned char *bits,
	unsigned int width, unsigned int height, Pixmap *normal,
	Pixmap *armed, Pixmap *grayed)
{
	Pixel fg,bg,ns,ts;
	GC gc=0;

	#ifdef TBR_STIPPLE_SHADING
	static Pixmap stipple=0;
	#endif
	Pixmap mask;
	
	static Display *dpy=NULL;
	static Window wnd;
	static int screen;
	static int depth;
	static int armed_offset;
	
	dbg_assert(width && height);
		
	if(!dpy){
		Dimension thin_borders;
		
		dpy=XtDisplay(app_inst.session_shell);
		screen=XScreenNumberOfScreen(XtScreen(app_inst.session_shell));
		wnd=XtWindow(app_inst.session_shell);
		depth=DefaultDepth(dpy,screen);
		XtVaGetValues(XmGetXmDisplay(app_inst.display),
			XmNenableThinThickness,&thin_borders,NULL);
		armed_offset=thin_borders?1:2;
	}
	
	gc=XtGetGC(w,0,NULL);
	
	XtVaGetValues(w,XmNforeground,&fg,
		XmNbackground,&bg,XmNbottomShadowColor,&ns,
		XmNtopShadowColor,&ts,NULL);

	*normal=XCreatePixmapFromBitmapData(dpy,wnd,
		(char*)bits,width,height,fg,bg,depth);

	/* armed pixmap shapes are offset in x,y */
	*armed=XCreatePixmap(dpy,wnd,width,height,depth);
	XSetClipMask(dpy,gc,None);
	XSetFillStyle(dpy,gc,FillSolid);	
	XSetForeground(dpy,gc,bg);
	XFillRectangle(dpy,*armed,gc,0,0,width,height);		
	XCopyArea(dpy,*normal,*armed,gc,0,0,
		width-armed_offset,height-armed_offset,
		armed_offset,armed_offset);

	#ifdef TBR_STIPPLE_SHADING
	/* shade grayed pixmap's foreground shape */
	*grayed=XCreatePixmapFromBitmapData(dpy,wnd,
		(char*)bits,width,height,fg,bg,depth);
	if(!stipple){
		char stipple_data[]={0x55,0xAA};
		stipple=XCreatePixmapFromBitmapData(
			dpy,wnd,stipple_data,8,2,0,1,1);
	}
	mask=XCreatePixmapFromBitmapData(
		dpy,wnd,(char*)bits,width,height,1,0,1);
	XSetStipple(dpy,gc,stipple);
	XSetClipMask(dpy,gc,mask);
	XSetFillStyle(dpy,gc,FillStippled);	
	XSetForeground(dpy,gc,bg);
	XFillRectangle(dpy,*grayed,gc,0,0,width,height);
	XFreePixmap(dpy,mask);
	#else
	/* create emboss effect on grayed pixmaps */
	mask=XCreatePixmapFromBitmapData(
		dpy,wnd,(char*)bits,width,height,1,0,1);

	*grayed=XCreatePixmap(dpy,wnd,width,height,depth);

	XSetFillStyle(dpy,gc,FillSolid);
	XSetForeground(dpy,gc,bg);
	XFillRectangle(dpy,*grayed,gc,0,0,width,height);
	XSetClipMask(dpy,gc,mask);
	XSetClipOrigin(dpy,gc,1,1);
	XSetForeground(dpy,gc,ts);
	XFillRectangle(dpy,*grayed,gc,0,0,width,height);
	XSetClipOrigin(dpy,gc,0,0);
	XSetForeground(dpy,gc,ns);
	XFillRectangle(dpy,*grayed,gc,0,0,width,height);
	
	XFreePixmap(dpy,mask);
	#endif
	
	XtReleaseGC(w,gc);
}

Widget get_toolbar_item(Widget toolbar, const char *name)
{
	Widget w;
	struct toolbar_data *tbd;
	Arg arg[1];
	
	XtSetArg(arg[0],XmNuserData,&tbd);
	XtGetValues(toolbar,arg,1);
	
	w=XtNameToWidget(tbd->wrowcol,name);
	dbg_assertmsg(w!=None,"toolbar item %s doesn't exist\n",name);
	return w;
}
