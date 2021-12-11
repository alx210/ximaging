/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/TextF.h>
#include <Xm/ArrowB.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include "common.h"
#include "strings.h"
#include "comdlgs.h"
#include "navbar.h"
#include "debug.h"

struct navbar_data {
	char *cur_path;
	Widget wparent;
	Widget wform;
	Widget wtext;
	Widget wdirup;
	navbar_cb callback;
	void *cb_data;
	Pixel form_bg;
	Pixel edit_bg;
};

static void path_changed_cb(Widget,XtPointer,XtPointer);
static void frame_destroy_cb(Widget,XtPointer,XtPointer);
static void path_focus_cb(Widget,XtPointer,XtPointer);
static void dir_up_cb(Widget,XtPointer,XtPointer);
static char* get_displayed_path(struct navbar_data *pbd);
static void set_displayed_path(struct navbar_data *pbd, const char *path);

/*
 * Create navigation bar widgets. Return their container widget
 * handle, which isn't managed yet.
 */
Widget create_navbar(Widget parent, navbar_cb callback, void *cb_data )
{
	Cardinal n=0;
	Arg args[10];
	struct navbar_data *data;
	Dimension border_width=0;
	
	data=calloc(1,sizeof(struct navbar_data));
	if(!data) return None;
	
	data->wparent=parent;
	data->callback=callback;
	data->cb_data=cb_data;

	XtVaGetValues(XmGetXmDisplay(app_inst.display),
		XmNenableThinThickness,&border_width,NULL);
	border_width=border_width?1:2;
	
	XtSetArg(args[n],XmNhorizontalSpacing,2); n++;
	XtSetArg(args[n],XmNuserData,(XtPointer)data); n++;
	XtSetArg(args[n],XmNshadowThickness,border_width); n++;
	XtSetArg(args[n],XmNshadowType,XmSHADOW_OUT); n++;
	XtSetArg(args[n],XmNhorizontalSpacing,2); n++;
	XtSetArg(args[n],XmNverticalSpacing,2); n++;
	data->wform=XmCreateForm(parent,"pathBox",args,n);
	XtAddCallback(data->wform,XmNdestroyCallback,frame_destroy_cb,data);
	
	XtSetArg(args[0],XmNbackground,&data->form_bg);
	XtGetValues(data->wform,args,1);

	n=0;
	XtSetArg(args[n],XmNtopAttachment,XmATTACH_FORM); n++;
	XtSetArg(args[n],XmNbottomAttachment,XmATTACH_FORM); n++;
	XtSetArg(args[n],XmNrightAttachment,XmATTACH_FORM); n++;
	XtSetArg(args[n],XmNarrowDirection,XmARROW_UP); n++;
	XtSetArg(args[n],XmNdetailShadowThickness,border_width); n++;
	/* XtSetArg(args[n],XmNforeground,data->form_bg); n++; */
	XtSetArg(args[n],XmNsensitive,False); n++;
	data->wdirup=XmCreateArrowButton(data->wform,"navigateUp",args,n);
	XtAddCallback(data->wdirup,XmNactivateCallback,dir_up_cb,data);
		
	n=0;
	XtSetArg(args[n],XmNtopAttachment,XmATTACH_FORM); n++;
	XtSetArg(args[n],XmNbottomAttachment,XmATTACH_FORM); n++;
	XtSetArg(args[n],XmNleftAttachment,XmATTACH_FORM); n++;
	XtSetArg(args[n],XmNrightAttachment,XmATTACH_WIDGET); n++;
	XtSetArg(args[n],XmNrightWidget,data->wdirup); n++;
	XtSetArg(args[n],XmNcursorPositionVisible,False); n++;
	XtSetArg(args[n],XmNmarginHeight,4); n++;
	data->wtext=XmCreateTextField(data->wform,"pathText",args,n);
	XtAddCallback(data->wtext,XmNactivateCallback,path_changed_cb,data);
	XtAddCallback(data->wtext,XmNfocusCallback,path_focus_cb,data);
	XtAddCallback(data->wtext,XmNlosingFocusCallback,path_focus_cb,data);
	
	
	XtSetArg(args[0],XmNbackground,&data->edit_bg);
	XtGetValues(data->wtext,args,1);
	XtSetArg(args[0],XmNbackground,data->form_bg);
	XtSetValues(data->wtext,args,1);
	
	XtManageChild(data->wdirup);
	XtManageChild(data->wtext);
	XtManageChild(data->wform);

	return data->wform;
}

/*
 * Set the path to be displayed in the navigation bar.
 * Returns zero on success, errno otherwise.
 */
int set_navbar_path(Widget navbar, const char *path)
{
	struct navbar_data *data=NULL;
	char *new_path;
	Arg args[1];
	
	XtSetArg(args[0],XmNuserData,&data);
	XtGetValues(navbar,args,1);
	dbg_assert(data);

	if(!path){
		if(data->cur_path){
			free(data->cur_path);
			data->cur_path=NULL;
		}
		XmTextFieldSetString(data->wtext,"");
		XtSetSensitive(data->wdirup,False);
		return 0;
	}
		
	new_path=realpath(path,NULL);
	if(!new_path || access(new_path,R_OK|X_OK)){
		int ret=errno;
		errno_message_box(data->wparent,ret,nlstr(APP_MSGSET,SID_EFAILED,
			"The action couldn't be completed."),False);
		return ret;
	}
	if(data->cur_path) free(data->cur_path);
	data->cur_path=new_path;
	set_displayed_path(data,new_path);
	XtSetSensitive(data->wdirup,True);
	
	return 0;
}

/*
 * Invoked by the text field displaying current path.
 */
static void path_changed_cb(Widget w,
	XtPointer client_data, XtPointer call_data)
{
	struct navbar_data *pbd=(struct navbar_data*)client_data;
	char *text;
	char *new_path;
	
	text=get_displayed_path(pbd);
	
	dbg_assert(text);
	
	new_path=realpath(text,NULL);
	if(!new_path){
		errno_message_box(pbd->wparent,errno,nlstr(APP_MSGSET,SID_EFAILED,
			"The action couldn't be completed."),False);
		if(pbd->cur_path){
			set_displayed_path(pbd,pbd->cur_path);
		}else{
			XmTextFieldSetString(w,"");
		}
		free(text);
		return;
	}
	free(text);
		
	if(pbd->cur_path && !strcmp(new_path,pbd->cur_path)){
		pbd->callback(pbd->cur_path,pbd->cb_data);
		return;
	}
	
	if(pbd->cur_path) free(pbd->cur_path);
	pbd->cur_path=new_path;
	pbd->callback(pbd->cur_path,pbd->cb_data);
	
	if(pbd->cur_path){
		set_displayed_path(pbd,pbd->cur_path);
		XtSetSensitive(pbd->wdirup,True);
	}else{
		XmTextFieldSetString(w,"");
		XtSetSensitive(pbd->wdirup,False);
	}
}

/*
 * Release memory held by the navbar_data struct
 */
static void frame_destroy_cb(Widget w,
	XtPointer client_data, XtPointer call_data)
{
	struct navbar_data *pbd=(struct navbar_data*)client_data;
	
	dbg_assert(pbd);
	if(pbd->cur_path) free(pbd->cur_path);
	free(pbd);
}

/*
 * Directory Up button callback
 */
static void dir_up_cb(Widget w,
	XtPointer client_data, XtPointer call_data)
{
	struct navbar_data *pbd=(struct navbar_data*)client_data;
	char *new_path;
	
	dbg_assert(pbd->cur_path);
	new_path=malloc(strlen(pbd->cur_path)+4);
	sprintf(new_path,"%s/..",pbd->cur_path);
	if(!set_navbar_path(pbd->wform,new_path))
			pbd->callback(pbd->cur_path,pbd->cb_data);
	free(new_path);
}

/*
 * Called on text field focus change. Changes the text field background
 * to form color and hides the cursor when unfocused.
 */
static void path_focus_cb(Widget w,
	XtPointer client_data, XtPointer call_data)

{
	Arg args[2];
	Cardinal n;
	XmTextVerifyCallbackStruct *cbs=(XmTextVerifyCallbackStruct*)call_data;
	struct navbar_data *pbd=(struct navbar_data*)client_data;
	
	n=0;
	if(cbs->reason==XmCR_FOCUS){
		XtSetArg(args[n],XmNcursorPositionVisible,True); n++;
		XtSetArg(args[n],XmNbackground,pbd->edit_bg); n++;
	}else{
		if(pbd->cur_path)
			set_displayed_path(pbd, pbd->cur_path);
		else
			XmTextFieldSetString(pbd->wtext,"");
		XtSetArg(args[n],XmNcursorPositionVisible,False); n++;
		XtSetArg(args[n],XmNbackground,pbd->form_bg); n++;
	}
	XtSetValues(w,args,n);
}

/* 
 * Retrieve text from the text field. Replace ~(if any) by user's home path.
 */
static char* get_displayed_path(struct navbar_data *pbd)
{
	char *path;
	char *new_path=NULL;
	char *home=getenv("HOME");
	
	path=XmTextFieldGetString(pbd->wtext);
	
	if(path[0]=='~'){
		new_path=malloc(strlen(home)+strlen(path)+1);
		sprintf(new_path,"%s%s",home,&path[1]);
	}else{
		new_path=strdup(path);
	}
	XtFree(path);
	return new_path;
}

/*
 * Substitute user's home path with a ~ and put it into the text field.
 */
static void set_displayed_path(struct navbar_data *pbd, const char *path)
{
	char *new_path=NULL;
	char *home=getenv("HOME");

	if(!strncmp(path,home,strlen(home))){
		new_path=malloc((strlen(path)-strlen(home))+3);
		sprintf(new_path,"~%s",&path[strlen(home)]);
		XmTextFieldSetString(pbd->wtext,new_path);
		free(new_path);
	}else{
		new_path=(char*)path;
		XmTextFieldSetString(pbd->wtext,new_path);
	}
	XmTextFieldSetInsertionPosition(pbd->wtext,strlen(new_path));
}
