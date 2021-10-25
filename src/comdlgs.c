/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
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
#include <Xm/MessageB.h>
#include <Xm/FileSB.h>
#include <Xm/SelectioB.h>
#include <Xm/TextF.h>
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
void display_about_dlgbox(Widget parent)
{
	Widget wbox;
	const char fmt_string[]="%s v%.1d.%.1d\n\n%s";
	char *version_text;
	XmString msg_text;
	XmString title;
	Arg args[5];
	int i=0;

	version_text=malloc(strlen(BASE_TITLE)+
		strlen(fmt_string)+strlen(COPYRIGHT)+1);
	sprintf(version_text,fmt_string,BASE_TITLE,APP_VERSION,
		APP_REVISION,COPYRIGHT);
	wbox=XmCreateInformationDialog(parent,"messageDialog",NULL,0);
	msg_text=XmStringCreateLocalized(version_text);
	free(version_text);
	title=XmStringCreateLocalized(BASE_TITLE);
	
	XtSetArg(args[i],XmNmessageString,msg_text); i++;
	XtSetArg(args[i],XmNnoResize,True); i++;
	XtSetArg(args[i],XmNdialogTitle,title); i++;
	XtSetArg(args[i],XmNdialogStyle,XmDIALOG_PRIMARY_APPLICATION_MODAL); i++;
	XtSetValues(wbox,args,i);
	i=0;
	XtSetArg(args[i],XmNdeleteResponse,XmDESTROY); i++;
	XtSetArg(args[i],XmNmwmFunctions,MWM_FUNC_MOVE); i++;
	XtSetValues(XtParent(wbox),args,i);
	
	XtUnmanageChild(XmMessageBoxGetChild(wbox,XmDIALOG_CANCEL_BUTTON));
	XtUnmanageChild(XmMessageBoxGetChild(wbox,XmDIALOG_HELP_BUTTON));

	XtManageChild(wbox);

	XmStringFree(title);
	XmStringFree(msg_text);
}
