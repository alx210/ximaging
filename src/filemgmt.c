/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * File management routines.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/Scale.h>
#include <Xm/Frame.h>
#include <Xm/Protocols.h>
#include <Xm/MwmUtil.h>
#include <Xm/MessageB.h>
#include "common.h"
#include "strings.h"
#include "comdlgs.h"
#include "filemgmt.h"
#include "const.h"
#include "guiutil.h"
#include "filemgmt_i.h"
#include "debug.h"

#include "bitmaps/wmfmgmt.bm"
#include "bitmaps/wmfmgmt_m.bm"

/* Local prototypes */
static void* proc_thread_entry(void *arg);
static struct proc_data* alloc_pdata(char **src,
	const char *dest, int count, fm_notify_cbt, void*);
static void free_pdata(struct proc_data *pd);
static void cancel_cb(Widget w, XtPointer call, XtPointer client);
static void create_progress_widget(struct proc_data*);
static int copy_file(struct proc_data*, const char *src, const char *dest);
static int move_file(struct proc_data*, const char *src, const char *dest);
static int launch_proc_thread(struct proc_data*);
static enum response_code ask_replace(struct proc_data*, const char *fname);
static char* make_dest_file_name(const char *src, const char *dir);
static void map_progress_widget_cb(XtPointer client, XtIntervalId *iid);
static void thread_callback_proc(XtPointer,int*,XtInputId*);
static int prepare_proc(struct proc_data*);
static void update_progress_widget(struct proc_data*);
static void shell_map_cb(Widget,XtPointer,XEvent*,Boolean*);

/*
 * Entry point of the file processing thread.
 */
static void* proc_thread_entry(void *data)
{
	struct proc_data *pd=(struct proc_data*)data;
	struct thread_msg tmsg;
	int res=0;
	int i;
	
	for(i=0; i<pd->nfiles && !pd->cancelled; i++, res=0){
		char *dest=NULL;
		pthread_mutex_lock(&pd->state_mutex);
		pd->cur_file=i;
		tmsg.code=TM_PRE_NOTE;
		tmsg.status=0;
		write(pd->thread_notify_fd[TNFD_OUT],&tmsg,sizeof(struct thread_msg));
		pthread_cond_wait(&pd->got_response,&pd->state_mutex);
		pthread_mutex_unlock(&pd->state_mutex);
		
		switch(pd->proc){
			case FPROC_MOVE:
			dest=make_dest_file_name(pd->src[i],pd->dest);
			res=move_file(pd,pd->src[i],dest);
			break;
			case FPROC_COPY:
			dest=make_dest_file_name(pd->src[i],pd->dest);
			res=copy_file(pd,pd->src[i],dest);
			break;
			case FPROC_DELETE:
			errno=0;
			unlink(pd->src[i]);
			res=errno;
			break;
			default: dbg_trap("illegal proc id\n"); break;
		}
		if(dest)free(dest);
		/* on error, notify the user and ask to continue or cancel */
		if(res){
			char *msg_buf;
			char *msg;
			char *reason;
			
			msg=nlstr(APP_MSGSET,
				SID_EFILEPROC,"Error while processing file:");
			reason=strerror(res);
			msg_buf=malloc(strlen(msg)+strlen(pd->src[i])+strlen(reason)+4);
			sprintf(msg_buf,"%s\n%s\n%s.",msg,pd->src[i],reason);
			pthread_mutex_lock(&pd->state_mutex);
			pd->proc_msg=msg_buf;
			tmsg.code=TM_WAIT_ACK;
			tmsg.status=0;
			write(pd->thread_notify_fd[TNFD_OUT],&tmsg,
				sizeof(struct thread_msg));
			pthread_cond_wait(&pd->got_response,&pd->state_mutex);
			pd->proc_msg=NULL;
			pthread_mutex_unlock(&pd->state_mutex);
			free(msg_buf);
		}
		pthread_mutex_lock(&pd->state_mutex);
		tmsg.code=TM_POST_NOTE;
		tmsg.status=res;
		write(pd->thread_notify_fd[TNFD_OUT],&tmsg,sizeof(struct thread_msg));
		pthread_cond_wait(&pd->got_response,&pd->state_mutex);
		pthread_mutex_unlock(&pd->state_mutex);
	}

	tmsg.code=TM_FINISHED;
	tmsg.status=0;
	write(pd->thread_notify_fd[TNFD_OUT],&tmsg,sizeof(struct thread_msg));

	return NULL;
}

/*
 * Update labels in the progress widget
 */
static void update_progress_widget(struct proc_data *pd)
{
	char *sz_label;
	XmString str_label;
	Arg arg[1];
	
	if(pd->wshell==None) return;
	sz_label = shorten_mb_string(pd->src[pd->cur_file],
		MAX_LABEL_PATH_NAME, True);
	str_label = XmStringCreateLocalized(sz_label);
	free(sz_label);
	
	XtSetArg(arg[0],XmNlabelString,str_label);
	XtSetValues(pd->wsrc,arg,1);
	XmUpdateDisplay(pd->wshell);
	XmStringFree(str_label);
}

/*
 * Thread message handler
 */
static void thread_callback_proc(XtPointer client,int *pfd, XtInputId *iid)
{
	struct proc_data *pd=(struct proc_data*)client;
	struct thread_msg tmsg;
	
	if(read(pd->thread_notify_fd[TNFD_IN],&tmsg,sizeof(struct thread_msg))<1)
		return;
	
	switch(tmsg.code){
		enum mb_result res;
		
		case TM_PRE_NOTE:
		pthread_mutex_lock(&pd->state_mutex);
		update_progress_widget(pd);
		pthread_cond_signal(&pd->got_response);
		pthread_mutex_unlock(&pd->state_mutex);
		break;

		case TM_POST_NOTE:
		pthread_mutex_lock(&pd->state_mutex);
		if(pd->caller_notify_cb){
			pd->caller_notify_cb(pd->proc,pd->src[pd->cur_file],
				tmsg.status,pd->caller_cb_data);
		}
		pthread_cond_signal(&pd->got_response);
		pthread_mutex_unlock(&pd->state_mutex);
		break;
		
		/* the thread is waiting for acknowledgement */
		case TM_WAIT_ACK:
		if(pd->map_delay){
			XtRemoveTimeOut(pd->map_delay);
			map_progress_widget_cb((XtPointer)pd,NULL);
			update_progress_widget(pd);
		}
		res=message_box(pd->wshell,MB_CONFIRM,pd->proc_title,pd->proc_msg);
		pthread_mutex_lock(&pd->state_mutex);
		pd->response=(res==MBR_DECLINE)?RC_QUIT:RC_CONTINUE;
		pthread_cond_signal(&pd->got_response);
		pthread_mutex_unlock(&pd->state_mutex);
		break;
		
		/* the thread is waiting for confitmation */
		case TM_WAIT_CONFIRM:
		if(pd->map_delay){
			XtRemoveTimeOut(pd->map_delay);
			map_progress_widget_cb((XtPointer)pd,NULL);
			update_progress_widget(pd);
		}
		res=message_box(pd->wshell,MB_CQUESTION,pd->proc_title,pd->proc_msg);
		pthread_mutex_lock(&pd->state_mutex);
		if(res==MBR_CONFIRM)
			pd->response=RC_CONTINUE;
		else if(res==MBR_DECLINE)
			pd->response=RC_SKIP;
		else
			pd->response=RC_QUIT;
		pthread_cond_signal(&pd->got_response);
		pthread_mutex_unlock(&pd->state_mutex);
		break;
		
		/* the thread has finished processing the queue */
		case TM_FINISHED:
		if(pd->map_delay){
			XtRemoveTimeOut(pd->map_delay);
		}else if(pd->wshell!=None){
			XtUnmapWidget(pd->wshell);
			XmUpdateDisplay(pd->wshell);
		}
		if(pd->caller_notify_cb)
			pd->caller_notify_cb(FPROC_FINISHED,NULL,0,pd->caller_cb_data);
		free_pdata(pd);
		app_inst.active_shells--;
		if(!app_inst.active_shells){
			dbg_printf("exit flag set in %s: %s()\n",__FILE__,__FUNCTION__);
			set_exit_flag(EXIT_SUCCESS);
		}
	}
}


/*
 * Move a file. Returns zero on success, or errno otherwise.
 */
static int move_file(struct proc_data *pd, const char *src, const char *dest)
{
	struct stat st;
	
	if(!stat(dest,&st)){
		enum response_code rc;
		rc=ask_replace(pd,dest);
		if(rc!=RC_CONTINUE){
			if(rc==RC_QUIT) pd->cancelled=True;
			return 0;
		}else{
			if(unlink(dest)) return errno;
		}
	}
	if(stat(src,&st)) return errno;
	
	/* try to rename the file first */
	if(rename(src,dest)){
		/* if rename failed because src/dest locations are on different mount
		 * points, try to copy the source, otherwise return */
		if(errno!=EXDEV) return errno;
		return copy_file(pd,src,dest);	
	}
	return 0;
}

/*
 * Copy a file. Returns zero on success, or errno otherwise.
 * If proc is FPROC_MOVE the source is unlinked on successfull copy.
 */
static int copy_file(struct proc_data *pd, const char *src, const char *dest)
{
	struct stat st;
	int fin, fout;
	unsigned int nchunks, ichunk;
	off_t rest;
	ssize_t rw;
	int res=0;

	if(!stat(dest ,&st) && (pd->proc != FPROC_MOVE)){
		enum response_code rc;
		rc = ask_replace(pd,dest);
		if(rc != RC_CONTINUE){
			if(rc == RC_QUIT) pd->cancelled = True;
			return 0;
		}
		if(unlink(dest)) return errno;
	}
	if(stat(src,&st)) return errno;
	fin=open(src,O_RDONLY);
	if(fin== -1) return errno;
	fout=open(dest,O_CREAT|O_WRONLY,(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP));
	if(fout== -1) return errno;
	
	if(st.st_size>COPY_BUFFER_SIZE){
		nchunks=st.st_size/COPY_BUFFER_SIZE;
		rest=st.st_size-(COPY_BUFFER_SIZE*nchunks);
	}else{
		nchunks=0;
		rest=st.st_size;
	}
	for(ichunk=0; ichunk<nchunks; ichunk++){
		rw=read(fin,pd->cp_buf,COPY_BUFFER_SIZE);
		if(rw!=COPY_BUFFER_SIZE) break;
		rw=write(fout,pd->cp_buf,COPY_BUFFER_SIZE);
		if(rw!=COPY_BUFFER_SIZE) break;
		if(pd->cancelled){
			close(fin);
			close(fout);
			unlink(dest);
			return 0;
		}
	}
	if(ichunk==nchunks && rest){
		if(read(fin,pd->cp_buf,rest)==rest){
			rw=write(fout,pd->cp_buf,rest);
		}
		if(rw!=rest) res=errno;
	}

	close(fin);
	close(fout);
	if(res){
		unlink(dest);
	}else if(pd->proc==FPROC_MOVE){
		unlink(src);
	}
	return res;	
}

/*
 * Display a modal message box asking to replace 'fname'
 */
static enum response_code ask_replace(struct proc_data *pd, const char *fname)
{
	char *msg, *msg_buf;
	size_t msg_len;
	struct thread_msg tmsg;

	msg=nlstr(APP_MSGSET,SID_FREPLACE,"File already exists. Overwrite?");
	msg_len=strlen(fname)+strlen(msg)+2;
	msg_buf=malloc(msg_len);
	sprintf(msg_buf,"%s\n%s",fname,msg);
	pthread_mutex_lock(&pd->state_mutex);
	pd->proc_msg=msg_buf;
	tmsg.code=TM_WAIT_CONFIRM;
	tmsg.status=0;
	write(pd->thread_notify_fd[TNFD_OUT],&tmsg,sizeof(struct thread_msg));
	pthread_cond_wait(&pd->got_response,&pd->state_mutex);
	pd->proc_msg=NULL;
	pthread_mutex_unlock(&pd->state_mutex);
	free(msg_buf);
	return pd->response;
}

/*
 * Allocate proc_data and copy the pointed source/destination arrays.
 */
static struct proc_data* alloc_pdata(char **src, const char *dest,
	int count, fm_notify_cbt notify_cb, void *cb_data)
{
	int i;
	struct proc_data *pd;
	
	pd=calloc(1,sizeof(struct proc_data));
	if(!pd) return NULL;
	pd->src=calloc(count,sizeof(char*));
	if(!pd->src){
		free(pd);
		return NULL;
	}

	pd->nfiles=count;
	for(i=0; i<count; i++){
		pd->src[i]=strdup(src[i]);
		if(!pd->src[i]) break;
	}
	if(i<count){
		free_pdata(pd);
		return NULL;
	}

	if(dest){
		pd->dest=strdup(dest);
		if(!pd->dest){
			free_pdata(pd);
			return NULL;
		}
	}
	pd->caller_notify_cb=notify_cb;
	pd->caller_cb_data=cb_data;
	return pd;
}

/*
 * Free the proc_data fields and the container itself.
 * This must properly handle partially allocated proc_data originating
 * from alloc_pdata in case of malloc failure.
 */
static void free_pdata(struct proc_data *pd)
{
	int i;
	XtRemoveInput(pd->input_id);
	close(pd->thread_notify_fd[0]);
	close(pd->thread_notify_fd[1]);
	for(i=0; i<pd->nfiles; i++){
		if(pd->src[i]) free(pd->src[i]);
	}
	free(pd->src);
	if(pd->dest) free(pd->dest);
	if(pd->wshell) XtDestroyWidget(pd->wshell);
	if(pd->cp_buf) free(pd->cp_buf);
	free(pd);
}

/*
 * 'Cancel' push button callback.
 */
static void cancel_cb(Widget w, XtPointer client, XtPointer call)
{
	struct proc_data *pd=(struct proc_data*)client;
	if(pd->wshell) XtSetSensitive(pd->wcancel,False);
	pd->cancelled=True;
}

/*
 * Progress widget mapping handler
 */
static void shell_map_cb(Widget w, XtPointer data, XEvent *evt, Boolean *cont)
{
	struct proc_data *pd=(struct proc_data*)data;
	if(evt->type!=MapNotify) return;
	pd->is_mapped=True;
}

/*
 * Timeout handler for the progress widget delay.
 */
static void map_progress_widget_cb(XtPointer client, XtIntervalId *iid)
{
	struct proc_data *pd=(struct proc_data*)client;
	if(pd->wshell==None) create_progress_widget(pd);
	map_shell_unpositioned(pd->wshell);
	pd->map_delay=None;
	while(!pd->is_mapped) XtAppProcessEvent(app_inst.context,XtIMAll);
}

/*
 * Construct a full path to the destination file from source file name.
 * Allocates dynamic storage for the returned string.
 */
static char* make_dest_file_name(const char *src, const char *dir)
{
	char *buf;
	char *ptr;
	size_t len;
	
	ptr=strrchr(src,'/');
	if(dir[strlen(dir)-1]=='/') ptr++;
	len=strlen(ptr)+strlen(dir)+1;
	buf=malloc(len);
	if(!buf) return NULL;
	strcpy(buf,dir);
	strcat(buf,ptr);
	return buf;
}

/*
 * Create the progress widget, which is reused by all procs.
 */
static void create_progress_widget(struct proc_data *pd)
{
	Widget wform, wsrc_frm;
	char *title=NULL;
	XmString title_str, str_cancel;
	static Pixmap wmicon = 0, wmicon_mask;
	
	pd->wshell=XtVaAppCreateShell("xiFileProgress",APP_CLASS,
		applicationShellWidgetClass,app_inst.display,
		XmNallowShellResize,True,XmNminWidth,320,
		XmNmwmFunctions,MWM_FUNC_MOVE|MWM_FUNC_MINIMIZE,NULL);
	XtAddEventHandler(pd->wshell,StructureNotifyMask,
		False,shell_map_cb,(XtPointer)pd);	
	wform=XmVaCreateForm(pd->wshell,"form",
		XmNhorizontalSpacing,8,XmNverticalSpacing,8,NULL);
	
	wsrc_frm=XmVaCreateManagedFrame(wform,"sourceFrame",
		XmNmarginHeight,2,XmNmarginWidth,2,
		XmNshadowType,XmSHADOW_ETCHED_IN,XmNleftAttachment,
		XmATTACH_FORM,XmNrightAttachment,XmATTACH_FORM,
		XmNtopAttachment,XmATTACH_FORM,NULL);

	title_str=XmStringCreateLocalized(nlstr(DLG_MSGSET,SID_SOURCE,"Source"));
	XmVaCreateManagedLabel(wsrc_frm,"sourceTitle",
		XmNlabelString,title_str,XmNchildType,XmFRAME_TITLE_CHILD,
		XmNalignment,XmALIGNMENT_BEGINNING,NULL);
	XmStringFree(title_str);
	
	title_str=XmStringCreateLocalized(" ");
	pd->wsrc=XmVaCreateManagedLabel(wsrc_frm,"sourceLabel",
		XmNlabelString,title_str,XmNchildType,XmFRAME_WORKAREA_CHILD,
		XmNalignment,XmALIGNMENT_BEGINNING,NULL);
	XmStringFree(title_str);
	
	if(pd->proc!=FPROC_DELETE){
		char *sz_label;
		sz_label = shorten_mb_string(pd->dest, MAX_LABEL_PATH_NAME, True);
		
		pd->wdest_frm=XmVaCreateManagedFrame(wform,"destinationFrame",
			XmNmarginHeight,2,XmNmarginWidth,2,
			XmNshadowType,XmSHADOW_ETCHED_IN,XmNleftAttachment,
			XmATTACH_FORM,XmNrightAttachment,XmATTACH_FORM,
			XmNtopAttachment,XmATTACH_WIDGET,XmNtopWidget,wsrc_frm,NULL);

		title_str=XmStringCreateLocalized(
			nlstr(DLG_MSGSET,SID_DESTINATION,"Destination"));
		XmVaCreateManagedLabel(pd->wdest_frm,"destinationTitle",
			XmNlabelString,title_str,XmNchildType,XmFRAME_TITLE_CHILD,
			XmNalignment,XmALIGNMENT_BEGINNING,NULL);
		XmStringFree(title_str);

		title_str=XmStringCreateLocalized(sz_label);
		free(sz_label);
		pd->wdest=XmVaCreateManagedLabel(pd->wdest_frm,"destinationLabel",
			XmNlabelString,title_str,XmNchildType,XmFRAME_WORKAREA_CHILD,
			XmNalignment,XmALIGNMENT_BEGINNING,NULL);
		XmStringFree(title_str);
	}
		
	str_cancel=XmStringCreateLocalized(nlstr(DLG_MSGSET,SID_CANCEL,"Cancel"));
	pd->wcancel=XmVaCreateManagedPushButton(wform,"cancelButton",
		XmNlabelString,str_cancel,XmNshowAsDefault,True,XmNsensitive,True,
		XmNrightAttachment,XmATTACH_FORM,XmNtopAttachment,XmATTACH_WIDGET,
		XmNtopWidget,(pd->proc!=FPROC_DELETE)?pd->wdest_frm:wsrc_frm,
		XmNbottomAttachment,XmATTACH_FORM,NULL);
	XmStringFree(str_cancel);
	XtAddCallback(pd->wcancel,XmNactivateCallback,
		cancel_cb,(XtPointer)pd);
	
	XtManageChild(wform);
	XtRealizeWidget(pd->wshell);
	
	switch(pd->proc){
		case FPROC_COPY:
		title=nlstr(DLG_MSGSET,SID_COPYING,"Copying");
		break;
		case FPROC_MOVE:
		title=nlstr(DLG_MSGSET,SID_MOVING,"Moving");
		break;
		case FPROC_DELETE:
		title=nlstr(DLG_MSGSET,SID_DELETING,"Deleting");
		break;
		default: dbg_trap("illegal proc id\n");	break;
	}
	pd->proc_title=title;
	#ifndef NO_DEFAULT_WM_ICON
	if(!wmicon){
		load_icon(wmfmgmt_bits,wmfmgmt_m_bits,
		wmfmgmt_width,wmfmgmt_height,&wmicon,&wmicon_mask);
	}
	#endif /* NO_DEFAULT_WM_ICON */

	XtVaSetValues(pd->wshell,XmNtitle,title,XmNiconName,title,
		XmNiconPixmap,wmicon,XmNiconMask,wmicon_mask,NULL);
	XmUpdateDisplay(pd->wshell);
}

/*
 * Create a new processing thread.
 */
static int launch_proc_thread(struct proc_data *pd)
{
	int res;
	pthread_t pid;
	
	errno=0;
	res=pthread_create(&pid,NULL,proc_thread_entry,(void*)pd);
	if(!res){
		app_inst.active_shells++;
		pd->map_delay=XtAppAddTimeOut(app_inst.context,PROG_WIDGET_MAP_DELAY,
			map_progress_widget_cb,(XtPointer)pd);
	}
	return errno;
}

/*
 * Open communication pipe and add input callback.
 */
static int prepare_proc(struct proc_data *pd)
{
	int res;
	res=pipe(pd->thread_notify_fd);
	if(!res){
		pd->input_id=XtAppAddInput(app_inst.context,
			pd->thread_notify_fd[TNFD_IN],(XtPointer)XtInputReadMask,
			thread_callback_proc,(XtPointer)pd);
	}
	return res;
}

/*
 * Exported copy/move/delete batch processing functions.
 */
int copy_files(char **src, const char *dest, int count,
	fm_notify_cbt notify_cb, void *cb_data)
{
	struct proc_data *pd;
	char *cp_buf;
	int res=0;
	
	cp_buf=malloc(COPY_BUFFER_SIZE);
	if(!cp_buf)	return ENOMEM;
	
	pd=alloc_pdata(src,dest,count,notify_cb,cb_data);
	if(pd){
		pd->proc=FPROC_COPY;
		pd->cp_buf=cp_buf;
		res=prepare_proc(pd);
		if(!res) res=launch_proc_thread(pd);
		if(res) free_pdata(pd);
	}else{
		free(cp_buf);
		res=ENOMEM;
	}
	return res;	
}

int move_files(char **src, const char *dest, int count,
	fm_notify_cbt notify_cb, void *cb_data)
{
	struct proc_data *pd;
	char *cp_buf;
	int res=0;

	cp_buf=malloc(COPY_BUFFER_SIZE);
	if(!cp_buf) return ENOMEM;
	
	pd=alloc_pdata(src,dest,count,notify_cb,cb_data);
	if(pd){
		pd->proc=FPROC_MOVE;
		pd->cp_buf=cp_buf;
		res=prepare_proc(pd);
		if(!res) res=launch_proc_thread(pd);
		if(res) free_pdata(pd);
	}else{
		free(cp_buf);
		res=ENOMEM;
	}
	return res;
}

int delete_files(char **names, int count,
	fm_notify_cbt notify_cb, void *cb_data)
{
	struct proc_data *pd;
	int res=0;

	pd=alloc_pdata(names,NULL,count,notify_cb,cb_data);
	if(pd){
		pd->proc=FPROC_DELETE;
		res=prepare_proc(pd);
		if(!res) res=launch_proc_thread(pd);
		if(res) free_pdata(pd);
	}else{
		res=ENOMEM;
	}
	return res;
}
