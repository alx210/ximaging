/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * CDE/ToolTalk IPC routines.
 */

#include <stdlib.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Tt/tt_c.h>
#include <Tt/tttk.h>
#include "common.h"
#include "browser.h"
#include "viewer.h"
#include "strings.h"
#include "comdlgs.h"
#include "const.h"
#include "debug.h"

/* Local prototypes */
static Tt_message media_load_cb(Tt_message msg, void *pwidget,
	Tttk_op op, Tt_status stat, unsigned char *contents, int len,
	char *file, char *docname);
static Tt_message media_load_msg_cb(Tt_message msg, void *client_data,
	Tttk_op op, unsigned char *content, int len, char *file);
static Tt_message contract_cb(Tt_message msg,
	void *client_data, Tt_message contract);
static Tt_message req_status_cb(Tt_message msg,
	void *client_data, Tt_message contract);
static void error_msg(Tt_status stat);
static Boolean join_session(Boolean static_ptype);
static Tt_callback_action com_pattern_cb(Tt_message,Tt_pattern);
static Tt_callback_action com_message_cb(Tt_message,Tt_pattern);
static void report_tt_error(const char *msg);
static void obtain_msg_resources(Tt_message,struct app_resources*);

/* ToolTalk session related globals */
static char *proc_id=NULL;
static Tt_pattern com_pat;	/* client/server communication pattern */
static Tt_pattern *dme_pat=NULL; /* desktop media exchange pattern */

/*
 * Initialize ToolTalk messaging
 */
void init_tt_media_exchange(void)
{
	int fd;
	char vr[8];

	snprintf(vr,8,"%d.%d",APP_VERSION,APP_REVISION);	
	proc_id=ttdt_open(&fd,BASE_NAME,BASE_NAME,vr,True);
	if(tt_ptr_error(proc_id)==TT_OK){
		Boolean ptype_installed=
			(tt_ptype_exists(APP_TT_PTYPE)==TT_OK)?True:False;
		#if 0
		if(!ptype_installed){
			warning_msg(nlstr(APP_MSGSET,SID_ENOPTYPE,
				BASE_TITLE " was built with CDE/ToolTalk IPC support, "
				"but the static ptype is not registered within the CDE type "
				"database. Other ToolTalk aware applications won't be able "
				"to communicate with " BASE_TITLE ". See the README file "
				"included with the application and/or the application manpage "
				"for information on how to fix this."));
		}
		#endif
		XtAppAddInput(app_inst.context,fd,(XtPointer)XtInputReadMask,
					tttk_Xt_input_handler,proc_id);
		if(join_session(ptype_installed)) return;
	}
	/* reached on failure */
	ttdt_close(NULL,NULL,True);
	fatal_error(0,NULL,nlstr(APP_MSGSET,SID_ETTSVC,
			"The ToolTalk service is not available."));
}

/*
 * Declare ptype and join the session. Returns true on success.
 */
static Boolean join_session(Boolean static_ptype)
{
	Tt_status status;
	
	/* dynamic client/server IPC pattern */
	com_pat=tt_pattern_create();
	tt_pattern_category_set(com_pat,TT_HANDLE);
	tt_pattern_scope_add(com_pat,TT_SESSION);
	tt_pattern_op_add(com_pat,APP_TT_COM_OP);
	tt_pattern_callback_add(com_pat,com_pattern_cb);
	tt_pattern_register(com_pat);
	
	/* static media exchange pattern */
	if(static_ptype){
		status=ttmedia_ptype_declare(APP_TT_PTYPE,0,media_load_cb,NULL,True);
		if(status!=TT_OK){
			char *error_msg=(nlstr(APP_MSGSET,SID_EDMX,
				"Unable to initialize desktop media exchange."));
			message_box(app_inst.session_shell,MB_ERROR,BASE_TITLE,error_msg);
			return False;
		}
		dme_pat=ttdt_session_join(NULL,contract_cb,
			app_inst.session_shell,NULL,True);
		if(tt_ptr_error(dme_pat)!=TT_OK) return False;
	}else{
		status=tt_session_join(tt_default_session());
		if(status!=TT_OK) return False;
	}
	
	return True;
}

/* Close the ToolTalk session */
void quit_tt_session(void)
{
	tt_pattern_unregister(com_pat);
	if(dme_pat) ttdt_session_quit(NULL,dme_pat,True);
	ttdt_close(proc_id,NULL,True);
}

/*
 * Server side desktop media exchange contract callback
 */
static Tt_message contract_cb(Tt_message msg,
	void *client_data, Tt_message contract)
{
	int mark;
	Tttk_op op;

	mark=tt_mark();
	op=tttk_string_op(tt_message_op(msg));
	
	if(op==TTDT_QUIT){
		Boolean handled=False;
		/* must be either one */
		handled=handle_viewer_ttquit(msg);
		handled|=handle_browser_ttquit(msg);
		tt_release(mark);
		if(!handled) tttk_message_fail(msg,TT_DESKTOP_ENOMSG,NULL,True);
		return ((Tt_message)0);		
	}	
	tt_release(mark);
    return msg;
}

/*
 * Media load message request handler
 */
static Tt_message media_load_cb(Tt_message msg, void *pwidget,
	Tttk_op op, Tt_status status, unsigned char *contents, int len,
	char *file, char *docname)
{
	struct stat st;
	struct app_resources res;
	int mark;
	Widget wshell;

	if(op!=TTME_DISPLAY && op!=TTME_INSTANTIATE){
		return msg;
	}

	/* merge default resources with requestor's */	
	memcpy(&res,&init_app_res,sizeof(struct app_resources));

	mark=tt_mark();	
	obtain_msg_resources(msg,&res);
	
	if(!file){
		/* no file specified, so create an empty viewer window */
		if(res.browse){
			wshell=get_browser(&res,msg);
		}else{
			wshell=get_viewer(&res,msg);
		}
		if(wshell){
			ttdt_message_accept(msg,contract_cb,wshell,NULL,True,True);
			tt_release(mark);
			return ((Tt_message)0);
		}	
	}else{
		/* if a file was specified, check if it's a regular file or directory */
		if(stat(file,&st)== -1){
			tttk_message_fail(msg,TT_DESKTOP_EPERM,strerror(errno),True);
			tt_release(mark);
			if(file) tt_free(file);
			return ((Tt_message)0);
		}
		
		/* launch viewer for regular files, browser for directories */
		if(S_ISREG(st.st_mode)){
			wshell=get_viewer(&res,NULL);
			if(wshell){
				ttdt_message_accept(msg,contract_cb,wshell,NULL,True,True);
				display_image(wshell,file,msg);
				tt_release(mark);
				return ((Tt_message)0);
			}
		}else if(S_ISDIR(st.st_mode)){
			wshell=get_browser(&res,NULL);
			if(wshell){
				ttdt_message_accept(msg,contract_cb,wshell,NULL,True,True);
				browse_path(wshell,file,msg);
				tt_release(mark);
				return ((Tt_message)0);
			}
		}
		/* not a regular file or directory */
	}
	
	/* if arrived here, then we couldn't handle the message */
	tttk_message_fail(msg,TT_DESKTOP_EINVAL,
		nlstr(APP_MSGSET,SID_EFILETYPE,"The specified file is invalid."),True);
	tt_free(file);
	tt_release(mark);
	return ((Tt_message)0);
}

/* 
 * Pass 'open_spec' to the server instance if it exists.
 * Returns True if a server instance accepted the request.
 */
Boolean query_server(const char *open_spec, const char *force_suffix)
{
	Tt_message msg;
	Tt_message reply;
	Tt_status status;
	Tt_state state;
	char *proc_id;
	char *stat_str;
	struct pollfd pfd;
	
	/* if _SUN_TT_TOKEN is set, we were instantiated by ttsession,
	 * and thus there is no server instance running */
	if(getenv("_SUN_TT_TOKEN") &&
		!strcmp(APP_TT_PTYPE,getenv("_SUN_TT_TOKEN"))){
		return False;
	}
	
	proc_id=tt_open();

	msg=tt_message_create();
	if((status=tt_ptr_error(msg))!=TT_OK){
		stat_str=tt_status_message(status);
		report_tt_error(stat_str);
		tt_free(stat_str);
		return False;
	}
	tt_message_op_set(msg,APP_TT_COM_OP);
	tt_message_address_set(msg,TT_PROCEDURE);
	tt_message_class_set(msg,TT_REQUEST);
	tt_message_disposition_set(msg,TT_DISCARD);
	tt_message_scope_set(msg,TT_SESSION);
	tt_message_callback_add(msg,com_message_cb);
	tt_message_context_set(msg,"open_spec",open_spec);
	tt_message_context_set(msg, "force_suffix", force_suffix);
	tt_message_context_set(msg,"geometry",init_app_res.geometry);
	tt_message_context_set(msg,"pin_window",
		(init_app_res.pin_window?"True":"False"));
	tt_message_context_set(msg,"browse",
		(init_app_res.browse?"True":"False"));
	status=tt_message_send(msg);
	if(status!=TT_OK){
		stat_str=tt_status_message(status);
		report_tt_error(stat_str);
		tt_free(stat_str);
		tt_close();
		return False;
	}
	/* wait for the reply */
	pfd.fd=tt_fd();
	pfd.events=POLLIN;
	switch(poll(&pfd,1,APP_TT_COM_TIMEOUT)){
		case (-1):
			perror("poll");
			tt_close();
		return True;
		case 0:{
			char *msg=nlstr(APP_MSGSET,SID_QSTIMEOUT,
				"Another instance of " BASE_TITLE
				" appears to be running, but is not responding.\n");

			message_box(app_inst.session_shell,MB_ERROR,BASE_TITLE,msg);
			tt_close();
		}return True;
	}
		
	reply=tt_message_receive();
	state=tt_message_state(msg);
	tt_message_destroy(msg);
	tt_message_destroy(reply);
	tt_close();
	return (state==TT_HANDLED)?True:False;
}

/*
 * Client side message callback that handles replies to messages
 * sent in query_server.
 */
static Tt_callback_action com_message_cb(Tt_message msg, Tt_pattern pat)
{
	Tt_state state=tt_message_state(msg);
	Tt_status status=tt_message_status(msg);
	char *stat_str;
	
	if(state!=TT_HANDLED && status!=TT_ERR_NO_MATCH){
		stat_str=tt_status_message(status);
		report_tt_error(stat_str);
		tt_free(stat_str);
	}
	/* this msg is destroyed in query_server */
	return TT_CALLBACK_PROCESSED;
}

/*
 * Server side pattern callback that processes messages sent
 * in query_server.
 */
static Tt_callback_action com_pattern_cb(Tt_message msg, Tt_pattern pat)
{
	int mark;
	char *open_spec;
	char *force_suffix;
	struct stat st;
	struct app_resources res;
	Widget wshell;

	tt_message_reply(msg);

	mark=tt_mark();
	/* merge default resources with requestor's */	
	memcpy(&res,&init_app_res,sizeof(struct app_resources));
	obtain_msg_resources(msg,&res);	
	open_spec=tt_message_context_val(msg,"open_spec");
	force_suffix = tt_message_context_val(msg, "force_suffix");
	
	/* no file/directory name specified */
	if(!open_spec[0]){
		if(res.browse){
			get_browser(&res,msg);
		}else{
			get_viewer(&res,msg);
		}
		tt_release(mark);
		return TT_CALLBACK_PROCESSED;
	}
	/* get a viewer/browser window, depending on the open_spec contents */
	if((stat(open_spec,&st) != -1) &&
		(S_ISDIR(st.st_mode) || S_ISREG(st.st_mode))){
		if(S_ISDIR(st.st_mode)){
			wshell=get_browser(&res,NULL);
			browse_path(wshell,open_spec,NULL);
		}else{
			wshell=get_viewer(&res,NULL);
			display_image(wshell, open_spec, force_suffix, NULL);
		}
	}else{
		message_box(app_inst.session_shell,MB_ERROR,BASE_TITLE,
			nlstr(APP_MSGSET,SID_EFILETYPE,
			"The specified file is invalid."));
	}
	tt_release(mark);
	tt_message_destroy(msg);
	return TT_CALLBACK_PROCESSED;
}

/*
 * Retrieve application resources from client's TT message.
 */
static void obtain_msg_resources(Tt_message msg,
	struct app_resources *res)
{
	char *p;
	
	if(tt_message_contexts_count(msg)){
		p=tt_message_context_val(msg,"geometry");
		if(*p) res->geometry=p;

		p=tt_message_context_val(msg,"pin_window");
		res->pin_window=(strcasecmp(p,"True"))?False:True;

		p=tt_message_context_val(msg,"browse");
		res->browse=(strcasecmp(p,"True"))?False:True;
	}
}

/*
 * Convenience function for reporting tooltalk related errors
 */
static void report_tt_error(const char *msg)
{
	char *p;
	char *buffer;
	
	p=nlstr(APP_MSGSET,SID_ETTMSG,"ToolTalk message delivery failed.");
	if(msg){
		buffer=malloc(strlen(p)+strlen(msg)+2);
		sprintf(buffer,"%s\n%s",p,msg);
		message_box(app_inst.session_shell,MB_ERROR,BASE_TITLE,buffer);
		free(buffer);
	}else{
		message_box(app_inst.session_shell,MB_ERROR,BASE_TITLE,p);
	}
}
