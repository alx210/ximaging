/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * X selection based server/client communication implementation.
 * This serves as a substitute for ToolTalk.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <X11/Intrinsic.h>
#include "const.h"
#include "common.h"
#include "strings.h"
#include "comdlgs.h"
#include "browser.h"
#include "viewer.h"
#include "debug.h"

static Boolean ipcs_convert_cb(Widget,Atom*,Atom*,Atom*,
	XtPointer*,unsigned long*,int*);
static void ipcs_lose_cb(Widget,Atom*);
static void ipcs_done_cb(Widget,Atom*,Atom*);
static void ipsc_value_ready_cb(Widget,XtPointer,Atom*,Atom*,
	XtPointer,unsigned long*,int*);
static Boolean try_own_selection(void);

static const char *open_file_name = NULL;
static char *msg_data = NULL;
static Boolean is_server = False;

/*
 * Checks for an existing instance.
 * Returns True if no other instance exists, initiates client/server
 * communication and returns False otherwise.
 */
Boolean init_x_ipc(const char *open_spec)
{
	char *login;
	char host[256]="localhost";
	char tmp[32];
	char *xa_server_str;
	char *xa_sreq_str;
	Window xw_owner;
	int retries = 3;

	open_file_name = open_spec;

	login = getlogin();
	if(!login) {
		snprintf(tmp, 32, "%u", getuid());
		login = tmp;
	}
	gethostname(host, 255);
	host[255] = '\0';

	xa_server_str=malloc(strlen(login)+strlen(host)+strlen(XA_SERVER)+3);
	xa_sreq_str=malloc(strlen(login)+strlen(host)+strlen(XA_SERVER_REQ)+3);
	if(!xa_server_str || !xa_sreq_str){
		warning_msg("Cannot initialize X IPC");
		return True;
	}
	sprintf(xa_server_str,"%s_%s_%s",XA_SERVER,login,host);
	sprintf(xa_sreq_str,"%s_%s_%s",XA_SERVER_REQ,login,host);

	app_inst.XaSERVER = XInternAtom(app_inst.display,xa_server_str,False);
	app_inst.XaSERVER_REQ = XInternAtom(app_inst.display,xa_sreq_str,False);
	free(xa_server_str);
	free(xa_sreq_str);
	
	for(;;) {
		xw_owner = XGetSelectionOwner(app_inst.display,app_inst.XaSERVER);
		if(xw_owner){
			Atom xa_ret_type;
			int ret_fmt;
			unsigned long ret_items;
			unsigned long ret_bytes_left;
			unsigned char *prop_data;
			int rstate;

			rstate = XGetWindowProperty(app_inst.display,xw_owner,
				app_inst.XaSERVER,0,strlen(XA_SERVER),False,
				XA_STRING,&xa_ret_type,&ret_fmt,&ret_items,
				&ret_bytes_left,&prop_data);
			
			if(rstate != Success) break;
			
			if(xa_ret_type == XA_STRING){
				XFree(prop_data);
				is_server=False;
				break;
			}else{
				usleep(100);
				if(!retries--){
					warning_msg("Timed out waiting for selection owner.\n");
					break;
				}
			}
		}else{
			is_server=True;
			break;
		}
	}
	
	if(is_server){
		XChangeProperty(app_inst.display,
			XtWindow(app_inst.session_shell),app_inst.XaSERVER,
			XA_STRING,8,PropModeReplace,(unsigned char*)XA_SERVER,
			strlen(XA_SERVER));
	}
	
	try_own_selection();

	XFlush(app_inst.display);
	
	dbg_trace("%d is %s\n",(int)getpid(),is_server?"server":"client");
	
	return is_server;
}

/*
 * IPC selection convert callback
 */
static Boolean ipcs_convert_cb(Widget w, Atom *sel, Atom *tgt,
	Atom *type_ret, XtPointer *val_ret, unsigned long *len_ret, int *fmt_ret)
{
	size_t msg_len;
	char *msg_data;

	msg_len = 256 + (init_app_res.geometry?25:0) +
		(open_file_name?strlen(open_file_name):0);

	msg_data = malloc(msg_len+1);
	if(!msg_data) {
		warning_msg("Failed to send IPC message");
		return False;
	}

	snprintf(msg_data,msg_len,
		(open_file_name?
		"geometry=%s pin_window=%d browse=%d open_spec=%s":
		"geometry=%s pin_window=%d browse=%d"),
		init_app_res.geometry?init_app_res.geometry:"*",
		init_app_res.pin_window?1:0,
		init_app_res.browse?1:0,
		open_file_name);

	*type_ret = app_inst.XaSERVER_REQ;
	*val_ret = (XtPointer) msg_data;
	*len_ret = msg_len;
	*fmt_ret = 8;

	dbg_trace("reqest sent: %s\n",msg_data);
	
	return True;	
}

/*
 * IPC selection lose callback
 */
static void ipcs_lose_cb(Widget w, Atom *sel)
{
	if(!is_server) return;
	
	dbg_trace("selection grabbed by %d\n",(int)getpid());
	XtGetSelectionValue(w,app_inst.XaSERVER,app_inst.XaSERVER_REQ,
		ipsc_value_ready_cb,NULL,CurrentTime);
	
	try_own_selection();
}

/*
 * Data ready callback
 */
static void ipsc_value_ready_cb(Widget w, XtPointer client_data,
	Atom *selection, Atom *type, XtPointer value,
	unsigned long *length, int *format)
{
	char geometry[25];
	char *open_spec;
	int pin_window;
	int browse;
	int items;
	struct app_resources res;
	
	/* This should never happen^TM */
	if(*type != app_inst.XaSERVER_REQ) return;

	dbg_assert(*length);
	open_spec = malloc(*length);
	if(!open_spec){
		warning_msg(strerror(errno));
		return;
	}
	open_spec[0] = '\0';
	
	dbg_trace("request received: %s\n",(char*)value);
	
	items = sscanf((char*)value,
		"geometry=%s pin_window=%d browse=%d open_spec=%s",
		geometry,
		&pin_window,
		&browse,
		open_spec);
	
	if(items < 3){
		warning_msg("Invalid IPC request\n");
		free(open_spec);
		return;
	}
	
	memcpy(&res,&init_app_res,sizeof(struct app_resources));
	
	res.geometry = (geometry[0] == '*')?NULL:geometry;
	res.pin_window = (Boolean)pin_window;
	
	if(!open_spec[0]){
		if(browse){
			get_browser(&res,NULL);
		}else{
			get_viewer(&res,NULL);
		}
	}else{
		Widget wshell;
		struct stat st;
		
		if((stat(open_spec,&st) != -1) &&
			(S_ISDIR(st.st_mode) || S_ISREG(st.st_mode))){
			if(S_ISDIR(st.st_mode)){
				wshell=get_browser(&res,NULL);
				browse_path(wshell,open_spec,NULL);
			}else{
				wshell=get_viewer(&res,NULL);
				display_image(wshell,open_spec,NULL);
			}
		}else{
			message_box(app_inst.session_shell,MB_ERROR,BASE_TITLE,
				nlstr(APP_MSGSET,SID_EFILETYPE,
				"The specified file is invalid."));
		}
	}
	free(open_spec);
}

/*
 * IPC selection done callback
 */
static void ipcs_done_cb(Widget w, Atom *sel, Atom *tgt)
{
	free(msg_data);
	msg_data = NULL;
	dbg_trace("%d done sending data\n",(int)getpid());
	set_exit_flag(EXIT_SUCCESS);
}

/*
 * Try and retry to grab IPC selection atom. Returns True on success.
 */
static Boolean try_own_selection(void)
{
	int retries = 3;
	
	while(False == XtOwnSelection(app_inst.session_shell,
		app_inst.XaSERVER,CurrentTime,ipcs_convert_cb,
		ipcs_lose_cb,ipcs_done_cb)){
		usleep(100);
		if(!retries--){
			warning_msg("Timed out waiting for selection.\n");
			return False;
		}
	}
	return True;
}
