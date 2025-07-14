/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Menu construction routines
 */

#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeB.h>
#include <Xm/SeparatoG.h>
#include <Xm/PushBG.h>
#include <Xm/ToggleBG.h>
#include "common.h"
#include "menu.h"
#include "strings.h"
#include "debug.h"

static XmString munge_label(const char *text, KeySym *mnemonic);
static void create_buttons(Widget parent, const struct menu_item *items,
	int nitems, const void *cbc_data, int msg_set);

/*
 * Create a pulldown menu and attach it to the given menubar widget.
 * First item in 'items' array specifies the cascade button. A message set
 * for menu item label string IDs must be specified in msg_set. If is_help
 * is True, the pulldown will be right-aligned. User data for callbacks may
 * be specified in cbc_data.
 */
Widget create_pulldown(Widget wmbar, const struct menu_item *items,
	int nitems, const void *cbc_data, int msg_set, Bool is_help)
{
	Widget wpulldown, wcascade;
	XmString label_str=NULL;
	Arg args[4];
	int n;
	KeySym mnemonic;

	/* first item in the array describes the cascade button */	
	wpulldown=XmCreatePulldownMenu(wmbar,items[0].name,NULL,0);
	if(items[0].type==IT_RADIO){
		XtVaSetValues(wpulldown,XmNradioAlwaysOne,True,
			XmNradioBehavior,True,NULL);
	}
	label_str=munge_label(
		nlstr(msg_set,items[0].label_sid,items[0].label),&mnemonic);
	n=0;
	XtSetArg(args[n],XmNlabelString,label_str); n++;
	XtSetArg(args[n],XmNsubMenuId,wpulldown); n++;
	if(mnemonic) {
		XtSetArg(args[n],XmNmnemonic,mnemonic);
		n++;
	}
	wcascade=XmCreateCascadeButton(wmbar,items[0].name,args,n);
	XmStringFree(label_str);
	create_buttons(wpulldown,&items[1],nitems-1,cbc_data,msg_set);
	if(is_help) XtVaSetValues(wmbar,XmNmenuHelpWidget,wcascade,NULL);
	XtManageChild(wcascade);
	return wpulldown;
}

/*
 * Create a popup menu
 */
Widget create_popup(Widget wparent, const char *name,
	const struct menu_item *items, int nitems,
	const void *cbc_data, int msg_set)
{
	Widget wpopup;
	
	wpopup=XmCreatePopupMenu(wparent,(String)name,NULL,0);
	create_buttons(wpopup,items,nitems,cbc_data,msg_set);
	return wpopup;
}
	

/*
 * Create menu buttons in wparent
 */
static void create_buttons(Widget wparent, const struct menu_item *items,
	int nitems, const void *cbc_data, int msg_set)
{
	int i, n;
	XmString label_str=NULL;
	Arg args[8];
	KeySym mnemonic;
	WidgetClass wc[]={
		xmPushButtonGadgetClass,
		xmToggleButtonGadgetClass,
		xmToggleButtonGadgetClass,
		xmSeparatorGadgetClass
	};
		
	for(i=0, n=0; i<nitems; i++, n=0){
		XtCallbackRec item_cb[]={
			{(XtCallbackProc)items[i].callback,(XtPointer)cbc_data},
			{(XtCallbackProc)NULL,(XtPointer)NULL}
		};
		
		/* create and set label string if any */
		if(items[i].label){
			label_str=munge_label(
				nlstr(msg_set,items[i].label_sid,items[i].label),&mnemonic);
			XtSetArg(args[n],XmNlabelString,label_str); n++;
			if(mnemonic){
				XtSetArg(args[n],XmNmnemonic,mnemonic);
				n++;
			}
		}
		
		switch(items[i].type){
			case IT_PUSH:
			if(items[i].callback){
				XtSetArg(args[n],XmNactivateCallback,item_cb); n++;
			}
			break;
			case IT_TOGGLE:
			if(items[i].callback){
				XtSetArg(args[n],XmNvalueChangedCallback,item_cb); n++;
			}
			break;
			case IT_RADIO:
			XtSetArg(args[n],XmNindicatorType,XmONE_OF_MANY); n++;
			if(items[i].callback){
				XtSetArg(args[n],XmNvalueChangedCallback,item_cb); n++;
			}
			break;
			case IT_SEP: break;
		}

		XtCreateManagedWidget(items[i].name,
			wc[items[i].type],wparent,args,n);
		
		/* temporary label XmString */
		if(items[i].label) XmStringFree(label_str);
	}
}

/* 
 * Parse the label string for a mnemonic and create a compound string.
 * The caller is responsible for freeing the returned XmString.
 */
static XmString munge_label(const char *text, KeySym *mnemonic)
{
	XmString label;
	char *token;
	char *tmp;

	tmp=strdup(text);
	token=strchr(tmp,'_');
	if(token){
		*mnemonic=(KeySym)token[1];
		memmove(token,token+1,strlen(token+1));
		token[strlen(token)-1]=0;
	}else{
		*mnemonic=0;
	}
	label=XmStringCreateLocalized(tmp);
	free(tmp);
	return label;
}
