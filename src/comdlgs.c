/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

/*
 * Implements common dialogs and message boxes
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/DialogS.h>
#include <Xm/MessageB.h>
#include <Xm/FileSB.h>
#include <Xm/SelectioB.h>
#include <Xm/TextF.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/MwmUtil.h>
#include "common.h"
#include "strings.h"
#include "comdlgs.h"
#include "const.h"
#include "guiutil.h"
#include "debug.h"

/* Local prototypes */
static void msgbox_btn_cb(Widget w, XtPointer client, XtPointer call);
static void input_dlg_cb(Widget w, XtPointer client, XtPointer call);
static void rename_modify_cb(Widget, XtPointer client, XtPointer call);
static void msgbox_popup_cb(Widget w, XtPointer client, XtPointer call);

/*
 * Display a modal message dialog.
 */
enum mb_result message_box(Widget parent, enum mb_type type,
	const char *msg_title, const char *msg_str)
{
	Widget wbox=0;
	XmString ok_text = NULL;
	XmString cancel_text=NULL;
	XmString extra_text=NULL;
	XmString msg_text;
	XmString title;
	Arg args[11];
	int i=0;
	enum mb_result result=_MBR_NVALUES;
	Boolean blocking=False;
	XWindowAttributes xwatt;
	Widget parent_shell;
	
	if(parent==None) parent=app_inst.session_shell;
	dbg_assert(parent);
	
	if(!msg_title){
		char *sz=NULL;
		XtSetArg(args[0],XmNtitle,&sz);
		XtGetValues(parent,args,1);
		title=XmStringCreateLocalized(sz);
	}else{
		title=XmStringCreateLocalized((char*)msg_title);
	}
	msg_text=XmStringCreateLocalized((char*)msg_str);	

	wbox=XmCreateMessageDialog(parent,"messageDialog",NULL,0);
	
	switch(type){
		case MB_CONFIRM:
		blocking=True;
		ok_text=XmStringCreateLocalized(nlstr(DLG_MSGSET,SID_OK,"OK"));
		cancel_text=XmStringCreateLocalized(
			nlstr(DLG_MSGSET,SID_CANCEL,"Cancel"));
		XtSetArg(args[i],XmNdialogType,XmDIALOG_WARNING); i++;
		XtSetArg(args[i],XmNokLabelString,ok_text); i++;
		XtSetArg(args[i],XmNcancelLabelString,cancel_text); i++;
		XtUnmanageChild(XmMessageBoxGetChild(wbox,XmDIALOG_HELP_BUTTON));	
		break;
		case MB_QUESTION:
		blocking=True;
		ok_text=XmStringCreateLocalized(nlstr(DLG_MSGSET,SID_YES,"Yes"));
		cancel_text=XmStringCreateLocalized(nlstr(DLG_MSGSET,SID_NO,"No"));
		XtSetArg(args[i],XmNdialogType,XmDIALOG_QUESTION); i++;
		XtSetArg(args[i],XmNokLabelString,ok_text); i++;
		XtSetArg(args[i],XmNcancelLabelString,cancel_text); i++;
		XtUnmanageChild(XmMessageBoxGetChild(wbox,XmDIALOG_HELP_BUTTON));
		break;
		case MB_CQUESTION:
		blocking=True;
		ok_text=XmStringCreateLocalized(nlstr(DLG_MSGSET,SID_YES,"Yes"));
		cancel_text=XmStringCreateLocalized(nlstr(DLG_MSGSET,SID_NO,"No"));
		extra_text=XmStringCreateLocalized(
			nlstr(DLG_MSGSET,SID_CANCEL,"Cancel"));
		XtSetArg(args[i],XmNdialogType,XmDIALOG_QUESTION); i++;
		XtSetArg(args[i],XmNokLabelString,ok_text); i++;
		XtSetArg(args[i],XmNcancelLabelString,cancel_text); i++;
		XtSetArg(args[i],XmNhelpLabelString,extra_text); i++;
		break;
		case MB_NOTIFY:
		blocking=True;
		case MB_NOTIFY_NB:
		ok_text=XmStringCreateLocalized(nlstr(DLG_MSGSET,SID_OK,"OK"));
		XtSetArg(args[i],XmNdialogType,XmDIALOG_INFORMATION); i++;
		XtSetArg(args[i],XmNokLabelString,ok_text); i++;
		XtUnmanageChild(XmMessageBoxGetChild(wbox,XmDIALOG_CANCEL_BUTTON));
		XtUnmanageChild(XmMessageBoxGetChild(wbox,XmDIALOG_HELP_BUTTON));
		break;
		case MB_ERROR:
		blocking=True;
		case MB_ERROR_NB:
		ok_text=XmStringCreateLocalized(
			nlstr(DLG_MSGSET,SID_DISMISS,"Dismiss"));
		XtSetArg(args[i],XmNdialogType,XmDIALOG_ERROR); i++;
		XtSetArg(args[i],XmNokLabelString,ok_text); i++;
		XtUnmanageChild(XmMessageBoxGetChild(wbox,XmDIALOG_CANCEL_BUTTON));
		XtUnmanageChild(XmMessageBoxGetChild(wbox,XmDIALOG_HELP_BUTTON));
		break;
	};
			
	XtSetArg(args[i],XmNmessageString,msg_text); i++;
	XtSetArg(args[i],XmNdialogStyle,XmDIALOG_PRIMARY_APPLICATION_MODAL); i++;
	XtSetArg(args[i],XmNnoResize,True); i++;
	XtSetArg(args[i],XmNdialogTitle,title); i++;
	XtSetValues(wbox,args,i);
	i=0;
	XtSetArg(args[i],XmNdeleteResponse,XmDESTROY); i++;
	XtSetArg(args[i],XmNmwmFunctions,MWM_FUNC_MOVE); i++;
	XtSetValues(XtParent(wbox),args,i);

	XmStringFree(title);
	XmStringFree(ok_text);
	XmStringFree(msg_text);
	if(cancel_text) XmStringFree(cancel_text);
	if(extra_text) XmStringFree(extra_text);
	
	if(blocking){
		XtAddCallback(wbox,XmNokCallback,msgbox_btn_cb,(XtPointer)&result);
		XtAddCallback(wbox,XmNcancelCallback,msgbox_btn_cb,(XtPointer)&result);
		XtAddCallback(wbox,XmNhelpCallback,msgbox_btn_cb,(XtPointer)&result);
	}
	
	parent_shell=parent;
	while(!XtIsShell(parent_shell)) parent_shell=XtParent(parent_shell);
	XGetWindowAttributes(XtDisplay(parent_shell),
		XtWindow(parent_shell),&xwatt);

	if(xwatt.map_state==IsUnmapped){
		/* this gets the dialog centered on the screen */
		XtAddCallback(XtParent(wbox),XmNpopupCallback,msgbox_popup_cb,
			(XtPointer)wbox);
	}

	XtManageChild(wbox);
	XmUpdateDisplay(wbox);
	if(blocking){
		while(result==_MBR_NVALUES)
			XtAppProcessEvent(app_inst.context,XtIMAll);
		return result;
	}
	return True;
}

/* message_box response callback */
static void msgbox_btn_cb(Widget w, XtPointer client, XtPointer call)
{
	XmAnyCallbackStruct *cbs=(XmAnyCallbackStruct*)call;
	enum mb_result *res=(enum mb_result*)client;

	if(res){
		switch(cbs->reason){
			case XmCR_OK:
			*res=MBR_CONFIRM;
			break;
			case XmCR_CANCEL:
			*res=MBR_DECLINE;
			break;
			case XmCR_HELP:
			*res=MBR_CANCEL;
			break;
		}
	}
	XtDestroyWidget(w);
}

/*
 * Manage message box position if parent isn't mapped
 */
static void msgbox_popup_cb(Widget w, XtPointer client, XtPointer call)
{
	int sw, sh, sx, sy;
	Dimension mw, mh;
	Arg args[2];
	Widget wbox=(Widget)client;

	XtRemoveCallback(w,XmNpopupCallback,msgbox_popup_cb,client);

	XtSetArg(args[0],XmNwidth,&mw);
	XtSetArg(args[1],XmNheight,&mh);
	XtGetValues(wbox,args,2);

	get_screen_size(wbox,&sw,&sh,&sx,&sy);

	XtSetArg(args[0],XmNx,(sx+(sw-mw)/2));
	XtSetArg(args[1],XmNy,(sy+(sh-mh)/2));
	XtSetValues(wbox,args,2);
}

/*
 * Display a blocking file title input dialog.
 * Returns a valid string or NULL if cancelled.
 */
char* rename_file_dlg(Widget parent, char *file_title)
{
	static Widget dlg;
	Arg arg[8];
	char *ret_string=NULL;
	char *label_string;
	char *token;
	XmString xm_label_string;
	size_t label_string_len;
	Widget wlabel;
	Widget wtext;
	int i=0;
	char *new_fn_text=nlstr(DLG_MSGSET,SID_INPUTFNAME,
			"Specify a new name for");
	XtSetArg(arg[i],XmNdialogType,XmDIALOG_PROMPT); i++;
	XtSetArg(arg[i],XmNminWidth,200); i++;
	XtSetArg(arg[i],XmNtitle,nlstr(DLG_MSGSET,SID_RENAME,"Rename")); i++;
	XtSetArg(arg[i],XmNdialogStyle,XmDIALOG_PRIMARY_APPLICATION_MODAL); i++;

	dlg=XmCreatePromptDialog(parent,"renameDialog",arg,i);
	XtAddCallback(dlg,XmNokCallback,input_dlg_cb,(XtPointer)&ret_string);
	XtAddCallback(dlg,XmNcancelCallback,
		input_dlg_cb,(XtPointer)&ret_string);
	XtUnmanageChild(XmSelectionBoxGetChild(dlg,XmDIALOG_HELP_BUTTON));

	wlabel=XmSelectionBoxGetChild(dlg,XmDIALOG_SELECTION_LABEL);
	wtext=XmSelectionBoxGetChild(dlg,XmDIALOG_TEXT);
	XtAddCallback(wtext,XmNmodifyVerifyCallback,&rename_modify_cb,NULL);
	label_string_len=strlen(new_fn_text)+strlen(file_title)+2;
	label_string=malloc(label_string_len);
	snprintf(label_string,label_string_len,"%s %s",new_fn_text,file_title);
	xm_label_string=XmStringCreateLocalized(label_string);
	free(label_string);

	XtSetArg(arg[0],XmNlabelString,xm_label_string);
	XtSetValues(wlabel,arg,1);
	XmStringFree(xm_label_string);
	
	i=0;
	XtSetArg(arg[i],XmNvalue,file_title); i++;
	XtSetArg(arg[i],XmNpendingDelete,True); i++;
	XtSetValues(wtext,arg,i);

	XtManageChild(dlg);
	
	/* preselect file title sans extension */
	token=strrchr(file_title,'.');
	if(token){
		XmTextFieldSetSelection(wtext,0,
			strlen(file_title)-strlen(token),
			XtLastTimestampProcessed(XtDisplay(wtext)));
	}

	while(!ret_string){
		XtAppProcessEvent(app_inst.context,XtIMAll);
	}
	XtDestroyWidget(dlg);
	XmUpdateDisplay(parent);
	return (ret_string[0]=='\0')?NULL:ret_string;
}

/*
 * Displays a blocking command input dialog.
 * Returns a valid string or NULL if cancelled.
 */
char* pass_to_input_dlg(Widget parent)
{
	static Widget dlg;
	Arg arg[8];
	Widget wtext;
	Widget wlabel;
	static char *last_input = NULL;
	char *ret_string = NULL;
	XmString xm_label_string;
	int i = 0;

	XtSetArg(arg[i], XmNdialogType, XmDIALOG_PROMPT); i++;
	XtSetArg(arg[i], XmNminWidth, 200); i++;
	XtSetArg(arg[i], XmNtitle, nlstr(DLG_MSGSET, SID_PASSTO, "Pass To")); i++;
	XtSetArg(arg[i], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); i++;

	dlg = XmCreatePromptDialog(parent,"passToDialog", arg ,i);
	XtAddCallback(dlg, XmNokCallback, input_dlg_cb, (XtPointer)&ret_string);
	XtAddCallback(dlg, XmNcancelCallback,
		input_dlg_cb, (XtPointer)&ret_string);
	XtUnmanageChild(XmSelectionBoxGetChild(dlg, XmDIALOG_HELP_BUTTON));

	wlabel = XmSelectionBoxGetChild(dlg, XmDIALOG_SELECTION_LABEL);
	wtext = XmSelectionBoxGetChild(dlg, XmDIALOG_TEXT);

	xm_label_string = XmStringCreateLocalized(
		nlstr(DLG_MSGSET, SID_INPUTCMD,
			"Specify a command to pass the file name to") );
	XtSetArg(arg[0], XmNlabelString, xm_label_string);
	XtSetValues(wlabel, arg, 1);
	XmStringFree(xm_label_string);

	XtManageChild(dlg);
	
	if(last_input) {
		i = 0;
		XtSetArg(arg[i], XmNvalue, last_input); i++;
		XtSetArg(arg[i], XmNpendingDelete, True); i++;
		XtSetValues(wtext, arg, i);

		XmTextFieldSetSelection(wtext, 0, strlen(last_input),
				XtLastTimestampProcessed(XtDisplay(wtext)));
		
		free(last_input);
		last_input = NULL;
	}

	while(!ret_string){
		XtAppProcessEvent(app_inst.context,XtIMAll);
	}
	
	XtDestroyWidget(dlg);
	XmUpdateDisplay(parent);
	
	if(ret_string[0] != '\0') {
		last_input = strdup(ret_string);
		return ret_string;
	}
	return NULL;
}

/*
 * Verification callback for file name input. Rejects path separator characters.
 */
static void rename_modify_cb(Widget w, XtPointer client, XtPointer call)
{
	XmTextVerifyCallbackStruct *vcs=(XmTextVerifyCallbackStruct*)call;
	
	if(vcs->reason==XmCR_MODIFYING_TEXT_VALUE && vcs->text->ptr &&
		strchr(vcs->text->ptr,'/')) vcs->doit=False;
}

/*
 * Display a blocking directory selection dialog.
 * Returns a valid path name or NULL if selection was cancelled.
 * If a valid path name is returned it must be freed by the caller.
 */
char* dir_select_dlg(Widget parent, const char *title,
	const char *init_path)
{
	static Widget dlg;
	Arg arg[8];
	int i=0;
	char *dir_name=NULL;
	XmString xm_init_path=NULL;

	if(!init_path) init_path=getenv("HOME");
	if(init_path)
		xm_init_path=XmStringCreateLocalized((String)init_path);

	XtSetArg(arg[i],XmNfileTypeMask,XmFILE_DIRECTORY); i++;
	XtSetArg(arg[i],XmNpathMode,XmPATH_MODE_FULL); i++;
	XtSetArg(arg[i],XmNresizePolicy,XmRESIZE_GROW); i++;
	XtSetArg(arg[i],XmNdirectory,xm_init_path); i++;
	XtSetArg(arg[i],XmNtitle,title); i++;
	XtSetArg(arg[i],XmNdialogStyle,XmDIALOG_PRIMARY_APPLICATION_MODAL); i++;

	dlg=XmCreateFileSelectionDialog(parent,"dirSelect",arg,i);
	if(xm_init_path) XmStringFree(xm_init_path);
	XtUnmanageChild(XmFileSelectionBoxGetChild(dlg,XmDIALOG_LIST_LABEL));
	XtUnmanageChild(XtParent(
		XmFileSelectionBoxGetChild(dlg,XmDIALOG_LIST)));
	XtAddCallback(dlg,XmNokCallback,input_dlg_cb,(XtPointer)&dir_name);
	XtAddCallback(dlg,XmNcancelCallback,input_dlg_cb,(XtPointer)&dir_name);
	XtUnmanageChild(XmFileSelectionBoxGetChild(dlg,XmDIALOG_HELP_BUTTON));
	XtManageChild(dlg);

	while(!dir_name){
		XtAppProcessEvent(app_inst.context,XtIMAll);
	}
	XtDestroyWidget(dlg);
	XmUpdateDisplay(parent);
	return (dir_name[0]=='\0')?NULL:dir_name;
}

/*
 * Called by directory selection and string input dialogs.
 */
static void input_dlg_cb(Widget w, XtPointer client, XtPointer call)
{
	XmFileSelectionBoxCallbackStruct *fscb=
		(XmFileSelectionBoxCallbackStruct*)call;
	char **ret_name=(char**)client;

	if(fscb->reason==XmCR_CANCEL || fscb->reason==XmCR_NO_MATCH){
		*ret_name="\0";
	}else{
		*ret_name=(char*)XmStringUnparse(fscb->value,NULL,0,
			XmCHARSET_TEXT,NULL,0,XmOUTPUT_ALL);
	}
	XtUnmanageChild(w);
}

/* 
 * Display an MB_ERROR message dialog with user specified message and errno
 * string appended to it following a colon. If the session shell isn't
 * available and 'parent' is None the string is printed to stderr.
 */
void errno_message_box(Widget parent, int errno_val,
	const char *msg, Boolean blocking)
{
	char *errno_msg=strerror(errno_val);
	
	if(parent || app_inst.session_shell){
		char *buffer;
		
		buffer=malloc(strlen(msg)+strlen(errno_msg)+4);
		sprintf(buffer,"%s\n%s.",msg,errno_msg);
		message_box(parent,(blocking?MB_ERROR:MB_ERROR_NB),
			BASE_TITLE,buffer);
		free(buffer);
	}else{
		fprintf(stderr,"<%s> %s %s\n",BASE_NAME,msg,errno_msg);
	}
}

/*
 * Display the 'About' dialog box
 */
void display_about_dlgbox(Widget wparent)
{
	Arg args[10];
	Cardinal n;
	Widget wdlg;
	Widget wform;
	Widget wtext;
	Widget wicon;
	Widget wclose;
	Widget wsep;
	Pixmap icon_pix;
	char *about_text;
	size_t text_len;
	XmString xms;
	
	#include "bitmaps/appicon.xbm"
	#include "bitmaps/appicon_m.xbm"
	
	text_len = snprintf(NULL, 0,
		"%s\nVersion %d.%d (%s; Motif %d.%d.%d)\n\n%s",
		DESCRIPTION_CS, APP_VERSION, APP_REVISION, APP_BUILD,
		XmVERSION, XmREVISION, XmUPDATE_LEVEL, COPYRIGHT_CS) + 1;
	about_text = malloc(text_len);
	snprintf(about_text, text_len,
		"%s\nVersion %d.%d (%s; Motif %d.%d.%d)\n\n%s",
		DESCRIPTION_CS, APP_VERSION, APP_REVISION, APP_BUILD,
		XmVERSION, XmREVISION, XmUPDATE_LEVEL, COPYRIGHT_CS);
	
	n = 0;
	XtSetArg(args[n], XmNmappedWhenManaged, True); n++;
	XtSetArg(args[n], XmNallowShellResize, True); n++;
	XtSetArg(args[n], XmNdeleteResponse, XmDESTROY); n++;
	wdlg = XmCreateDialogShell(wparent, "about", args, n);

	n = 0;
	xms = XmStringCreateLocalized(nlstr(DLG_MSGSET, SID_ABOUT, "About"));
	XtSetArg(args[n], XmNdialogTitle, xms); n++;
	XtSetArg(args[n], XmNhorizontalSpacing, 4); n++;
	XtSetArg(args[n], XmNverticalSpacing, 4); n++;
	XtSetArg(args[n], XmNnoResize, True); n++;
	XtSetArg(args[n], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); n++;
	wform = XmCreateForm(wdlg, "form", args, n);
	XmStringFree(xms);

	if( (icon_pix = load_masked_bitmap(wform, appicon_bits,
		appicon_m_bits, appicon_width, appicon_height)) == None)
		icon_pix = XmUNSPECIFIED_PIXMAP;

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNlabelPixmap, icon_pix); n++;
	XtSetArg(args[n], XmNlabelType, XmPIXMAP); n++;
	wicon = XmCreateLabel(wform, "icon", args, n);
	
	n = 0;
	xms = XmStringCreateLocalized(about_text);
	XtSetArg(args[n], XmNlabelString, xms); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, wicon); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	wtext = XmCreateLabel(wform, "text", args, n);
	XmStringFree(xms);

	n = 0;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, wtext); n++;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg(args[n], XmNseparatorType, XmSHADOW_ETCHED_IN); n++;
	wsep = XmCreateSeparator(wform, "separator", args, n);

	n = 0;
	xms = XmStringCreateLocalized(nlstr(DLG_MSGSET, SID_CLOSE, "Close"));
	XtSetArg(args[n], XmNlabelString, xms); n++;
	XtSetArg(args[n], XmNshowAsDefault, True); n++;
	XtSetArg(args[n], XmNsensitive, True); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, wsep); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	wclose = XmCreatePushButton(wform, "closeButton", args, n);
	XmStringFree(xms);

	n = 0;
	XtSetArg(args[n], XmNdefaultButton, wclose); n++;
	XtSetArg(args[n], XmNcancelButton, wclose); n++;
	XtSetValues(wform, args, n);

	XtManageChild(wicon);
	XtManageChild(wtext);
	XtManageChild(wsep);
	XtManageChild(wclose);	
	XtManageChild(wform);

	XtRealizeWidget(wdlg);
	
	free(about_text);
}
