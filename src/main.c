/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
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
#include <Xm/Xm.h>
#include <Xm/Protocols.h>
#ifdef ENABLE_CDE
#include <Dt/Dt.h>
#endif
#include "const.h"
#include "common.h"
#include "browser.h"
#include "viewer.h"
#include "menu.h"
#include "strings.h"
#include "comdlgs.h"
#include "tooltalk.h"
#include "guiutil.h"
#include "cmap.h"
#include "debug.h"

/* Local prototypes */
static Boolean init_instance(const char *open_spec);
static void init_display_globals(void);
static void sig_handler(int);

/* Application global variables */
struct app_resources init_app_res;
struct app_inst_data app_inst={0};
#ifdef ENABLE_NLS
nl_catd msg_cat_desc;
#endif
static int app_ret=0; /* see set_exit_flag */

/* XRDB Entries */
#define RESFIELD(f) XtOffset(struct app_resources*,f)
static XtResource xrdb_resources[]={
	{ "geometry","Geometry",XmRString,sizeof(char*),
		RESFIELD(geometry),XmRImmediate,(XtPointer)NULL
	},
	{ "tileSize","TileSize",XmRString,sizeof(char*),
		RESFIELD(tile_size),XmRImmediate,(XtPointer)DEF_TILE_SIZE
	},
	{ "tileSizeList","TileSizeList",XmRString,sizeof(char*),
		RESFIELD(tile_presets),XmRImmediate,(XtPointer)DEF_TILE_PRESETS
	},
	{ "tileAspectRatio","TileAspectRatio",XmRString,sizeof(char*),
		RESFIELD(tile_asr),XmRImmediate,(XtPointer)DEF_TILE_ASR
	},
	{ "refreshInterval","RefreshInterval",XmRInt,sizeof(int),
		RESFIELD(refresh_int),XmRImmediate,(XtPointer)DEF_REFRESH_INT
	},
	{ "keyPanAmount","KeyPanAmount",XmRInt,sizeof(int),
		RESFIELD(key_pan_amount),XmRImmediate,(XtPointer)DEF_KEY_PAN_AMOUNT
	},
	{ "zoomFit","ZoomFit",XmRBoolean,sizeof(Boolean),
		RESFIELD(zoom_fit),XmRImmediate,(XtPointer)True
	},
	{ "zoomIncrement","ZoomIncrement",XmRString,sizeof(char*),
		RESFIELD(zoom_inc),XmRImmediate,(XtPointer)DEF_ZOOM_INC
	},
	{ "visualProgress","VisualProgress",XmRBoolean,sizeof(Boolean),
		RESFIELD(vprog),XmRImmediate,(XtPointer)True
	},
	{ "lockRotation","LockRoration",XmRBoolean,sizeof(Boolean),
		RESFIELD(keep_tform),XmRImmediate,(XtPointer)False
	},
	{ "upsamplingFilter","UpsamplingFilter",XmRBoolean,sizeof(Boolean),
		RESFIELD(int_up),XmRImmediate,(XtPointer)False
	},
	{ "downsamplingFilter","DownsamplingFilter",XmRBoolean,sizeof(Boolean),
		RESFIELD(int_down),XmRImmediate,(XtPointer)True
	},
	{ "fastPanning","FastPanning",XmRBoolean,sizeof(Boolean),
		RESFIELD(fast_pan),XmRImmediate,(XtPointer)False
	},
	{ "confirmFileRemoval","ConfirmFileRemoval",XmRBoolean,sizeof(Boolean),
		RESFIELD(confirm_rm),XmRImmediate,(XtPointer)True
	},
	{ "viewerToolbar","ViewerToolbar",XmRBoolean,sizeof(Boolean),
		RESFIELD(show_viewer_tbr),XmRImmediate,(XtPointer)False
	},
	{ "largeToolbarIcons","LargeToolbarIcons",XmRBoolean,sizeof(Boolean),
		RESFIELD(large_tbr_icons),XmRImmediate,(XtPointer)False
	},
	{ "largeCursors","LargeCursors",XmRBoolean,sizeof(Boolean),
		RESFIELD(large_cursors),XmRImmediate,(XtPointer)False
	},
	{ "editCommand","EditCommand",XmRString,sizeof(char*),
		RESFIELD(edit_cmd),XmRImmediate,(XtPointer)NULL
	},
	{ "quiet","Quiet",XmRBoolean,sizeof(Boolean),
		RESFIELD(quiet),XmRImmediate,(XtPointer)False
	},
	{ "pinWindow","PinWindow",XmRBoolean,sizeof(Boolean),
		RESFIELD(pin_window),XmRImmediate,(XtPointer)False
	},
	{ "server","Server",XmRBoolean,sizeof(Boolean),
		RESFIELD(server),XmRImmediate,(XtPointer)False
	},
	{ "browse","Browse",XmRBoolean,sizeof(Boolean),
		RESFIELD(browse),XmRImmediate,(XtPointer)False
	},
	{ "showDirectories","ShowDirectories",XmRBoolean,sizeof(Boolean),
		RESFIELD(show_dirs),XmRImmediate,(XtPointer)True
	},
	{ "showDotFiles","ShowDotFiles",XmRBoolean,sizeof(Boolean),
		RESFIELD(show_dot_files),XmRImmediate,(XtPointer)True
	}

};
#undef RESFIELD

/* Command line options */
static XrmOptionDescRec xrdb_options[]={
	{"-pin","pinWindow",XrmoptionNoArg,(caddr_t)"True"},
	{"+pin","pinWindow",XrmoptionNoArg,(caddr_t)"False"},
	{"-server","server",XrmoptionNoArg,(caddr_t)"True"},
	{"-browse","browse",XrmoptionNoArg,(caddr_t)"True"},
	{"-quiet","quiet",XrmoptionNoArg,(caddr_t)"True"},
};
static const int num_xrdb_options=
	(sizeof(xrdb_options)/sizeof(XrmOptionDescRec));

/*
 * Launch a viewer or browser instance, depending on 'open_spec' contents.
 * If 'open_spec' is NULL launch as specified in app-defaults.
 */
static Boolean init_instance(const char *open_spec)
{
	Widget wshell;
	struct stat st;
	
	if(!open_spec){
		if(init_app_res.browse)
			get_browser(&init_app_res,NULL);
		else
			get_viewer(&init_app_res,NULL);
		return True;
	}else{
		if(stat(open_spec,&st)==(-1)){
			errno_message_box(None,errno,open_spec,True);
			return False;
		}
		if(S_ISDIR(st.st_mode)){
			wshell=get_browser(&init_app_res,NULL);
			if(wshell) browse_path(wshell,open_spec,NULL);
		}else{
			wshell=get_viewer(&init_app_res,NULL);
			if(wshell) display_image(wshell,open_spec,NULL);
		}
	}
	return True;
}

/*
 * Initialize display related app instance variables
 */
static void init_display_globals(void)
{
	int i, scr, depth;
	XVisualInfo *vi;
	Screen *pscreen;
	int classes[]={
		PseudoColor,TrueColor,DirectColor,
		GrayScale,StaticColor,StaticGray
	};
	
	pscreen=XtScreen(app_inst.session_shell);
	scr=XScreenNumberOfScreen(pscreen);
	depth=XDefaultDepth(app_inst.display,scr);

	/* find the most appropriate visual */
	for(i=(depth>8)?1:0; i<(sizeof(classes)/sizeof(int)); i++){
		XVisualInfo tvi;
		int items;
		
		tvi.class=classes[i];
		tvi.depth=depth;
		vi=XGetVisualInfo(app_inst.display,VisualClassMask,&tvi,&items);
		if(vi){
			memcpy(&app_inst.visual_info,&vi[0],sizeof(XVisualInfo));
			XFree(vi);
			
			/* determine and store padded pixel size */
			if(app_inst.visual_info.depth>16)
				app_inst.pixel_size=32;
			else if(app_inst.visual_info.depth==16)
				app_inst.pixel_size=16;
			else
				app_inst.pixel_size=8;
				
			if(app_inst.visual_info.class==PseudoColor){
				int ret;
				/* try to allocate a usable amount of pixels in the shared
				 * colormap, fall back to a private colormap otherwise */
				app_inst.colormap=XDefaultColormap(app_inst.display,scr);
				ret=cm_alloc_spectrum(app_inst.colormap,
					SHARED_CMAP_ALLOC,False);
				if(!ret){
					app_inst.colormap=XCreateColormap(app_inst.display,
						XtWindow(app_inst.session_shell),
						app_inst.visual_info.visual,AllocAll);
					ret=cm_alloc_spectrum(app_inst.colormap,
						PRIV_CMAP_ALLOC,True);
				}
				if(!ret) fatal_error(0,NULL,nlstr(APP_MSGSET,SID_ENORES,
					"Cannot allocate colormap"));
			}else{
				app_inst.colormap=XDefaultColormap(app_inst.display,scr);
			}
			return;
		}
	}
	fatal_error(0,NULL,nlstr(APP_MSGSET,SID_NOVISUAL,
			"No appropriate visual found"));
}

/*
 * Instantiate the application if no other instance is running,
 * or send an appropriate ToolTalk message to the existing instance and exit.
 */
int main(int argc, char **argv)
{
	char *open_spec=NULL;
	/* fallback resources defined in defaults.c */
	extern char *fallback_res[];

	app_inst.bin_name=argv[0];
	
	signal(SIGCHLD,sig_handler);
	
	XtSetLanguageProc(NULL,NULL,NULL);

	#ifdef ENABLE_NLS
	msg_cat_desc=catopen(BASE_NAME,NL_CAT_LOCALE);
	if((long)msg_cat_desc== -1)
		perror("Couldn't open " BASE_NAME " message catalog");
	#endif
	
	XtToolkitThreadInitialize();
	XtToolkitInitialize();
	app_inst.context=XtCreateApplicationContext();
	XtAppSetFallbackResources(app_inst.context,fallback_res);
	app_inst.display=XtOpenDisplay(app_inst.context,NULL,NULL,
		APP_CLASS,xrdb_options,num_xrdb_options,&argc,argv);
	app_inst.session_shell=XtVaAppCreateShell(NULL,APP_CLASS,
		applicationShellWidgetClass,app_inst.display,
		XmNmappedWhenManaged,False,XmNwidth,1,XmNheight,1,NULL);
	
	XtRealizeWidget(app_inst.session_shell);
	
	app_inst.XaWM_DELETE_WINDOW=XInternAtom(app_inst.display,
		"WM_DELETE_WINDOW",False);
	
	#ifdef ENABLE_CDE
	if(!DtAppInitialize(app_inst.context,app_inst.display,
		app_inst.session_shell,BASE_NAME,APP_CLASS)){
		fatal_error(0,NULL,nlstr(APP_MSGSET,SID_EDTSVC,
			"Unable to initialize desktop services."));
	}
	#endif /* ENABLE_CDE */

	XtGetApplicationResources(app_inst.session_shell,
		(XtPointer)&init_app_res,xrdb_resources,
		XtNumber(xrdb_resources),NULL,0);
	
	init_display_globals();
		
	/* non XRDB arguments */
	if(argc>1){
		open_spec=argv[1];
		if(argc>2)
			warning_msg(nlstr(APP_MSGSET,SID_EARG,
				"Ignoring redundant arguments.\n"));
	}
	
	if(open_spec){
		char *real_path;
		/* we need full path, since the file name is
		*  passed to another instance */
		real_path=realpath(open_spec,NULL);
		if(!real_path){
			errno_message_box(None,errno,open_spec,True);
			return EXIT_FAILURE;
		}
		open_spec=real_path;
	}
	
	#ifdef ENABLE_CDE
	if(!query_server(open_spec)){
		init_tt_media_exchange(True);
		if(!init_app_res.server){
			if(!init_instance(open_spec)) return EXIT_FAILURE;
		}
	}else{
		if(init_app_res.server){
			fatal_error(0,NULL,nlstr(APP_MSGSET,SID_ESERVING,
				BASE_TITLE " is already serving this display."));
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}
	#else /* ENABLE_CDE */
	if(init_x_ipc(open_spec) && !init_app_res.server){
		if(!init_instance(open_spec)) return EXIT_FAILURE;
	}
	#endif /* ENABLE_CDE */
	
	XtAppMainLoop(app_inst.context);
	return app_ret;
}

/* Anything that ought to be done before exiting should go here */
void set_exit_flag(int ret)
{
	#ifdef ENABLE_CDE
	quit_tt_session();
	#endif /* ENABLE_CDE */
	app_ret=ret;
	XtAppSetExitFlag(app_inst.context);
}

/* 
 * Display an error message and terminate.
 * if "title" is null "Fatal Error" will be used.
 * if "message" is NULL then strerror() text for the errno_value is used.
 */
void fatal_error(int rv, const char *title, const char *msg)
{
	if(msg==NULL) msg=strerror(rv);
	
	if(app_inst.session_shell){
		char *buffer;
		
		if(!title) title=nlstr(APP_MSGSET,SID_EFATAL,"Fatal error");
		buffer=malloc(strlen(title)+strlen(msg)+4);
		sprintf(buffer,"%s:\n%s",title,msg);
		message_box(app_inst.session_shell,MB_ERROR,BASE_TITLE,buffer);
		free(buffer);
	}else{
		fprintf(stderr,"[%s] %s: %s\n",BASE_TITLE,
			title?title:nlstr(APP_MSGSET,SID_EFATAL,"Fatal error"),msg);
	}
	exit(rv?rv:EXIT_FAILURE);
}

/* Print a warning message to stderr */
void warning_msg(const char *msg)
{
	if(init_app_res.quiet) return;
	fprintf(stderr,"[%s] %s: %s\n",BASE_TITLE,
		nlstr(APP_MSGSET,SID_WARNING,"Warning"),msg);
}

/* Handles sigchld */
void sig_handler(int sig)
{
	int status;
	if(sig == SIGCHLD) {
		wait(&status);
		#ifdef __svr4__
		signal(SIGCHLD, sig_handler);
		#endif
	}
}

