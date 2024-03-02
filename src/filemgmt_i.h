/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Private file management type definitions constants and data structures.
 */

#ifndef FILEMGMT_I_H
#define FILEMGMT_I_H

#include <Xm/Xm.h>
#include <pthread.h>

/* Count of chars at which the path name in dialog
 * labels should be abbreviated */
#define MAX_LABEL_PATH_NAME 60

/* Read/Write buffer size */
#define COPY_BUFFER_SIZE 524288	/* 512 KB */

/* Delay in ms before the progress widget gets mapped */
#define PROG_WIDGET_MAP_DELAY 1200

enum msg_code {
	TM_PRE_NOTE,
	TM_POST_NOTE,
	TM_WAIT_CONFIRM,
	TM_WAIT_ACK,
	TM_FINISHED,
};

enum response_code {
	RC_CONTINUE,
	RC_SKIP,
	RC_QUIT
};

struct thread_msg {
	enum msg_code code;
	int status;
};

struct proc_data{
	/* Progress shell and widgets */
	Widget wshell;
	Widget wsrc;
	Widget wdest;
	Widget wdest_frm;
	Widget wcancel;
	XtIntervalId map_delay;
	XtInputId input_id;
	Boolean is_mapped;
	
	char *cp_buf; /* copy buffer */
	enum file_proc_id proc;	/* what to do */
	char **src; /* list of source file names */
	char *dest;	/* destination directory for copy/move procs */
	int nfiles;	/* number of items in src */
	
	/* current state */
	Boolean cancelled;
	unsigned int cur_file;
	pthread_mutex_t state_mutex;
	pthread_cond_t got_response;

	/* For message boxes */
	char *proc_title;
	char *proc_msg;
	enum response_code response;
	
	/* Notification pipe */
	int thread_notify_fd[2];
	pthread_mutex_t thread_notify_mutex;
	
	/* Caller notification callback */
	fm_notify_cbt caller_notify_cb;
	void *caller_cb_data;
};

/* Notification pipe constants */
enum { TNFD_IN, TNFD_OUT };

#endif /* FILEMGMT_I_H */
