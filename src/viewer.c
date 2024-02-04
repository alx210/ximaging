/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * Implements the viewer.
 */

#include <stdlib.h>
#include <stdio.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/DrawingA.h>
#include <Xm/LabelG.h>
#include <Xm/SeparatoG.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/Protocols.h>
#include <Xm/ToggleBG.h>
#include <Xm/FileSB.h>
#include <X11/ImUtil.h>
#ifdef ENABLE_CDE
#include <Tt/tt_c.h>
#include <Tt/tttk.h>
#include <Dt/Action.h>
#endif
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <math.h>
#include "common.h"
#include "viewer_i.h"
#include "menu.h"
#include "cursor.h"
#include "strings.h"
#include "comdlgs.h"
#include "imgblt.h"
#include "guiutil.h"
#include "filemgmt.h"
#include "toolbar.h"
#include "const.h"
#include "browser.h"
#include "bswap.h"
#include "debug.h"
#include "bitmaps/wmiconv.bm"
#include "bitmaps/wmiconv_m.bm"

/* Local prototypes */
static struct viewer_data* create_viewer(const struct app_resources *res);
static struct viewer_data* get_viewer_inst_data(Widget wshell);
static void destroy_viewer(struct viewer_data *vd);
static void create_viewer_menubar(struct viewer_data *vd);
static void create_viewer_toolbar(struct viewer_data *vd);
static Boolean load_image(struct viewer_data *vd, const char *fname);
static void reset_viewer(struct viewer_data *vd);
static int alloc_storage(struct viewer_data *vd);
static void set_status_msg(struct viewer_data *vd,int msg_id, const char *text);
static void update_back_buffer(struct viewer_data *vd);
static void redraw_view(struct viewer_data *vd, Boolean clear);
static void zoom_view(struct viewer_data *vd, float zoom);
static void rotate_view(struct viewer_data *vd, Boolean cw);
static void scroll_view(struct viewer_data *vd, int x, int y);
static void* loader_thread(void*);
static void thread_callback_proc(XtPointer,int*,XtInputId*);
static int fn_sort_compare(const void *pa, const void *pb);
static void* dir_reader_thread(void *arg);
static void clear_dir_cache(struct viewer_data*);
static int scanline_read_cb(unsigned long, const uint8_t*, void*);
static void load_next_page(struct viewer_data *vd, Bool forward);
static void load_next_file(struct viewer_data *vd, Bool forward);
static void update_shell_title(struct viewer_data *vd);
static void update_props_msg(struct viewer_data *vd);
static void update_page_msg(struct viewer_data *vd);
static void display_status_summary(struct viewer_data *vd);
static void update_controls(struct viewer_data *vd);
static void update_pointer_shape(struct viewer_data *vd);
static Widget get_menu_item(struct viewer_data *vd, const char *name);
static float compute_fit_zoom(struct viewer_data *vd);
static void compute_image_dimensions(struct viewer_data *vd,
	float zoom, unsigned int transfm, int *width, int *height);
static void report_img_error(struct viewer_data *vd, int img_errno);
static char* get_path(const char *file_name);
static void cancel_cb(struct viewer_data *vd);
static void close_cb(Widget,XtPointer,XtPointer);
static void input_cb(Widget,XtPointer,XtPointer);
static void resize_cb(Widget,XtPointer,XtPointer);
static void expose_cb(Widget,XtPointer,XtPointer);
static void pin_window_cb(Widget,XtPointer,XtPointer);
static void next_page_cb(Widget,XtPointer,XtPointer);
static void prev_page_cb(Widget,XtPointer,XtPointer);
static void next_file_cb(Widget,XtPointer,XtPointer);
static void prev_file_cb(Widget,XtPointer,XtPointer);
static void copy_to_cb(Widget,XtPointer,XtPointer);
static void move_to_cb(Widget,XtPointer,XtPointer);
static void rename_cb(Widget,XtPointer,XtPointer);
static void delete_cb(Widget,XtPointer,XtPointer);
static void zoom_fit_cb(Widget,XtPointer,XtPointer);
static void zoom_in_cb(Widget,XtPointer,XtPointer);
static void zoom_out_cb(Widget,XtPointer,XtPointer);
static void zoom_reset_cb(Widget,XtPointer,XtPointer);
static void vflip_cb(Widget,XtPointer,XtPointer);
static void hflip_cb(Widget,XtPointer,XtPointer);
static void rotate_right_cb(Widget,XtPointer,XtPointer);
static void rotate_left_cb(Widget,XtPointer,XtPointer);
static void rotate_reset_cb(Widget,XtPointer,XtPointer);
static void rotate_lock_cb(Widget,XtPointer,XtPointer);
static void refresh_cb(Widget,XtPointer,XtPointer);
static void open_cb(Widget,XtPointer,XtPointer);
static void edit_cb(Widget,XtPointer,XtPointer);
static void pass_to_cb(Widget,XtPointer,XtPointer);
static void file_select_cb(Widget,XtPointer,XtPointer);
static void browse_cb(Widget,XtPointer,XtPointer);
static void browse_here_cb(Widget,XtPointer,XtPointer);
static void next_file_cb(Widget,XtPointer,XtPointer);
static void prev_file_cb(Widget,XtPointer,XtPointer);
static void new_viewer_cb(Widget,XtPointer,XtPointer);
static void about_cb(Widget,XtPointer,XtPointer);
static void toolbar_cb(Widget,XtPointer,XtPointer);
static void load_prog_handler(XtPointer client, XtIntervalId *iid);
static void load_fb_update_handler(XtPointer client, XtIntervalId *iid);
#ifdef ENABLE_CDE
static void help_topics_cb(Widget w, XtPointer client, XtPointer call);
#endif /* ENABLE_CDE */

/* Linked list of viewer instances */
struct viewer_data *viewers=NULL;

/*
 * Create new viewer_data with a shell and widgets and link it into the list.
 */
static struct viewer_data* create_viewer(const struct app_resources *res)
{
	static XtTranslations view_tt=NULL;
	Widget wmsgbar;
	Widget wsep;
	char *data=NULL;
	int width=0, height=0;
	XGCValues gc_values;
	struct viewer_data *vd=viewers;
	Colormap cmap;
	Pixel bg_pixel, tmp_pixel;
	static Pixmap wmicon=0;
	static Pixmap wmicon_mask=0;
	
	vd=calloc(1,sizeof(struct viewer_data));
	if(!vd) return NULL;
	if(pipe(vd->tnfd)){
		free(vd);
		return NULL;
	}
	vd->vprog=res->vprog;
	vd->keep_tform=res->keep_tform;
	vd->show_tbr=res->show_viewer_tbr;
	vd->large_tbr=res->large_tbr_icons;

	vd->zoom_inc=str_to_double(res->zoom_inc);
	if((vd->zoom_inc<1.1) || vd->zoom_inc>4){
		warning_msg("Illegal value for \"ZoomIncrement\". Using default.");
		vd->zoom_inc=1.6;
	}	
	if(res->key_pan_amount<1 || res->key_pan_amount>MAX_KEY_PAN_AMOUNT){
		warning_msg("Illegal value for \"KeyPanAmount\". Using default.");
		vd->key_pan_amount=DEF_KEY_PAN_AMOUNT;
	}else{
		vd->key_pan_amount=res->key_pan_amount;
	}
	
	vd->wshell=XtVaAppCreateShell("ximagingViewer",APP_CLASS "Viewer",
		applicationShellWidgetClass,app_inst.display,
		XmNvisual,app_inst.visual_info.visual,
		XmNcolormap,app_inst.colormap,
		XmNmappedWhenManaged,False,
		XmNdeleteResponse,XmUNMAP,
		XmNgeometry,res->geometry,NULL);
	
	XmAddWMProtocolCallback(vd->wshell,app_inst.XaWM_DELETE_WINDOW,
		close_cb,(XtPointer)vd);	

	vd->wmain=XmVaCreateMainWindow(vd->wshell,"main",NULL);
	create_viewer_menubar(vd);
	create_viewer_toolbar(vd);

	XtVaGetValues(vd->wmain,XmNcolormap,&cmap,XmNbackground,&tmp_pixel,NULL);
	XmGetColors(XtScreen(vd->wshell),cmap,
		tmp_pixel,&tmp_pixel,&tmp_pixel,&tmp_pixel,&bg_pixel);
	vd->wview=XmVaCreateManagedDrawingArea(
		vd->wmain,"view",XmNbackground,bg_pixel,XmNuserData,vd,NULL);

	/* create mouse translations for continguous panning and 
	 * add keyboard interaction actions if not done yet */
	if(!view_tt){
		view_tt=XtParseTranslationTable(
			"<Btn1Down>:DrawingAreaInput() ManagerGadgetArm()\n"
			"<Btn1Up>:DrawingAreaInput() ManagerGadgetActivate()\n"
			"<Btn1Motion>:DrawingAreaInput() ManagerGadgetButtonMotion()\n" 
			"<Motion>:DrawingAreaInput()\n<Key>osfCancel:DrawingAreaInput()\n"
			"<Key>osfUp:DrawingAreaInput()\n<Key>osfDown:DrawingAreaInput()\n"
			"<Key>osfLeft:DrawingAreaInput()\n<Key>osfRight:DrawingAreaInput()"
		);
	}
	XtOverrideTranslations(vd->wview,view_tt);

	wmsgbar=XmVaCreateForm(vd->wmain,"messageArea",
		XmNmarginWidth, 2, XmNshadowType, XmSHADOW_OUT,
		XmNshadowThickness, 1,NULL);
	
	vd->wzoom=XmVaCreateManagedLabelGadget(wmsgbar,"zoom",
		XmNmarginHeight,4,XmNrightAttachment,XmATTACH_FORM,NULL);
	wsep=XmVaCreateManagedSeparatorGadget(wmsgbar,"separator",
		XmNorientation,XmVERTICAL,
		XmNrightAttachment,XmATTACH_WIDGET,
		XmNbottomAttachment,XmATTACH_FORM,
		XmNtopAttachment,XmATTACH_FORM,
		XmNrightWidget,vd->wzoom,NULL);
	vd->wprops=XmVaCreateManagedLabelGadget(wmsgbar,"properties",
		XmNmarginHeight,4,
		XmNrightAttachment,XmATTACH_WIDGET,
		XmNrightWidget,wsep,NULL);
	wsep=XmVaCreateManagedSeparatorGadget(wmsgbar,"separator",
		XmNorientation,XmVERTICAL,
		XmNbottomAttachment,XmATTACH_FORM,
		XmNtopAttachment,XmATTACH_FORM,
		XmNrightAttachment,XmATTACH_WIDGET,
		XmNrightWidget,vd->wprops,NULL);
	vd->wmsg=XmVaCreateManagedLabelGadget(wmsgbar,"status",
		XmNmarginHeight,4,
		XmNalignment,XmALIGNMENT_BEGINNING,
		XmNrightAttachment,XmATTACH_WIDGET,
		XmNrightWidget,wsep,XmNleftAttachment,XmATTACH_FORM,NULL);
	
	XtVaSetValues(vd->wmain,XmNmenuBar,vd->wmenubar,
		XmNworkWindow,vd->wview,XmNmessageWindow,wmsgbar,
		XmNcommandWindow,vd->wtoolbar,
		XmNcommandWindowLocation,XmCOMMAND_ABOVE_WORKSPACE,NULL);
	
	XtManageChild(wmsgbar);
	XtManageChild(vd->wmenubar);
	if(vd->show_tbr) XtManageChild(vd->wtoolbar);
	XtManageChild(vd->wmain);
	XtAddCallback(vd->wview,XmNinputCallback,input_cb,(XtPointer)vd);
	XtAddCallback(vd->wview,XmNexposeCallback,expose_cb,(XtPointer)vd);
	XtAddCallback(vd->wview,XmNresizeCallback,resize_cb,(XtPointer)vd);
	XtRealizeWidget(vd->wshell);
	
	if(!wmicon){
		load_icon(wmiconv_bits,wmiconv_m_bits,
		wmiconv_width,wmiconv_height,&wmicon,&wmicon_mask);
	}
	XtVaSetValues(vd->wshell,XmNiconPixmap,wmicon,
		XmNiconMask,wmicon_mask,NULL);
	
	XtVaGetValues(vd->wshell,XmNtitle,&vd->title,NULL);
	vd->title=strdup(vd->title);
	XtVaGetValues(vd->wview,XmNbackground,&vd->bg_pixel,NULL);
	
	if(app_inst.visual_info.depth>8){
		if(init_pixel_format(&vd->display_pf,app_inst.pixel_size,
			app_inst.visual_info.red_mask,app_inst.visual_info.green_mask,
			app_inst.visual_info.blue_mask,0,0,IMGF_BGCOLOR)){
			fatal_error(0,NULL,"Unsupported display pixel format.");
		}
	}
	
	/* initialize Ximage back-buffer and allocate initial storage */	
	XtVaGetValues(vd->wview,XmNwidth,&width,XmNheight,&height,NULL);
	if(!width || !height){
		/* use something adequate if the window is too small */
		width=640;
		height=480;
	}

	vd->bkbuf_size=(width*height)*(app_inst.pixel_size/8);
	data=malloc(vd->bkbuf_size);
	vd->bkbuf=XCreateImage(app_inst.display,app_inst.visual_info.visual,
		app_inst.visual_info.depth,ZPixmap,0,data,width,height,
		app_inst.pixel_size,0);
	if(!data || !vd->bkbuf){
		if(vd->bkbuf) XDestroyImage(vd->bkbuf);
		XtDestroyWidget(vd->wshell);
		close(vd->tnfd[0]);
		close(vd->tnfd[1]);
		free(vd->title);
		free(vd);
		return NULL;
	}
	vd->bkbuf->bitmap_bit_order=vd->bkbuf->byte_order=
		(is_big_endian())?MSBFirst:LSBFirst;
	_XInitImageFuncPtrs(vd->bkbuf);

	if(vd->vprog){
		img_fill_rect(vd->bkbuf,0,0,vd->bkbuf->width,
			vd->bkbuf->height,vd->bg_pixel);
	}
	
	gc_values.function=GXcopy;
	gc_values.plane_mask=AllPlanes;
	vd->blit_gc=XCreateGC(app_inst.display,XtWindow(vd->wview),
		GCFunction|GCBackground|GCPlaneMask,&gc_values);

	/* initialize thread related data and add thread notification input */
	if(pthread_cond_init(&vd->ldr_finished_cond,NULL)||
		pthread_cond_init(&vd->rdr_finished_cond,NULL)||
		pthread_mutex_init(&vd->ldr_cond_mutex,NULL) ||
		pthread_mutex_init(&vd->rdr_cond_mutex,NULL) ||
		pthread_mutex_init(&vd->thread_notify_mutex,NULL)){
			XtDestroyWidget(vd->wshell);
			XDestroyImage(vd->bkbuf);
			XFreeGC(app_inst.display,vd->blit_gc);
			close(vd->tnfd[0]);
			close(vd->tnfd[1]);
			free(vd->title);
			free(vd);
			return NULL;
		}
		
	vd->thread_notify_input=XtAppAddInput(app_inst.context,
		vd->tnfd[TNFD_IN],(void*)XtInputReadMask,
		thread_callback_proc,(XtPointer)vd);

	/* link in the instance */
	vd->next=viewers;
	viewers=vd;
	app_inst.active_shells++;
	
	/* set initial vd values and update the gui */
	vd->zoom=1;
	vd->load_prog=0;

	XmToggleButtonGadgetSetState(
		get_menu_item(vd, "*toolbar"), res->show_viewer_tbr, False);
	XmToggleButtonGadgetSetState(
		get_menu_item(vd,"*pinThis"),res->pin_window,True);
	XmToggleButtonGadgetSetState(
			get_toolbar_item(vd->wtoolbar,"*pinThis"),
			res->pin_window,False);

	XmToggleButtonGadgetSetState(
		get_menu_item(vd,"*zoomFit"),res->zoom_fit,True);
	update_controls(vd);
	update_props_msg(vd);
	display_status_summary(vd);

	return vd;
}

/*
 * Load and display an image
 */
static Boolean load_image(struct viewer_data *vd, const char *fname)
{
	struct stat st;
	int img_errno;
	char *new_path;
	
	if(vd->state&(ISF_OPENED|ISF_LOADING|ISF_READY))
		reset_viewer(vd);

	vd->file_name=strdup(fname);
	set_status_msg(vd,SID_LOADING,"Loading...");
	/* save the modification time and the file size */
	if(stat(vd->file_name,&st)== -1){
		errno_message_box(vd->wshell,errno,vd->file_name,False);
		reset_viewer(vd);
		return False;
	}
	vd->mod_time=st.st_mtime;
	vd->file_size=st.st_size;
	
	new_path=get_path(fname);
	if(vd->dir_name){
		if(strcmp(vd->dir_name,new_path)){
			clear_dir_cache(vd);
			vd->dir_name=new_path;
		}else{
			free(new_path);
		}
	}else{
		vd->dir_name=new_path;
	}

	/* open the image and allocate initial buffers */
	img_errno=img_open(vd->file_name,&vd->img_file,0);
	if(img_errno){
		report_img_error(vd,img_errno);
		reset_viewer(vd);
		return False;
	}

	img_errno=alloc_storage(vd);
	if(img_errno){
		report_img_error(vd,img_errno);
		reset_viewer(vd);
		return False;
	}

	/* set up vprog if enabled */
	if(vd->vprog){
		img_fill_rect(vd->bkbuf,0,0,vd->bkbuf->width,
			vd->bkbuf->height,vd->bg_pixel);
		img_fill_rect(vd->image,0,0,vd->image->width,
			vd->image->height,vd->bg_pixel);
		if(vd->zoom_fit)
			vd->zoom=compute_fit_zoom(vd);
		else
			vd->zoom=1;
	}

	/* read the image */
	vd->state|=ISF_OPENED;
	update_shell_title(vd);
	update_props_msg(vd);
	vd->state|=ISF_LOADING;
	vd->load_prog=0;
	if(pthread_create(&vd->ldr_thread,NULL,loader_thread,(void*)vd)){
		vd->state&=(~ISF_LOADING);
		report_img_error(vd,IMG_ENOMEM);
		reset_viewer(vd);
		return False;
	}
	pthread_detach(vd->ldr_thread);

	update_controls(vd);
	/* set up timers */
	if(vd->vprog){
		vd->vprog_timer=XtAppAddTimeOut(app_inst.context,LOADER_FB_UPDATE_INT,
			load_fb_update_handler,(XtPointer)vd);
	}
	vd->prog_timer=XtAppAddTimeOut(app_inst.context,PROG_UPDATE_INT,
		load_prog_handler,(XtPointer)vd);
	return True;
}

/*
 * Find viewer instance data for 'wshell' and return it.
 * Returns NULL on failure.
 */
static struct viewer_data* get_viewer_inst_data(Widget wshell)
{
	struct viewer_data *vd=viewers;

	while(vd){
		if(vd->wshell==wshell) break;
		vd=vd->next;
	}
	return vd;
}

/*
 * Destroy all viewer data and widgets.
 */
static void destroy_viewer(struct viewer_data *vd)
{
	#ifdef ENABLE_CDE
	/* reply if destruction was invoked through a ToolTalk request */
	if(vd->tt_quit_req){
		tt_message_reply(vd->tt_quit_req);
		tt_message_destroy(vd->tt_quit_req);
	}
	#endif /* ENABLE_CDE */

	if(vd->state&(ISF_READY|ISF_LOADING|ISF_OPENED))
		reset_viewer(vd);
	clear_dir_cache(vd);
	if(vd->wfile_dlg) XtDestroyWidget(vd->wfile_dlg);
	XtDestroyWidget(vd->wshell);
	XFreeGC(app_inst.display,vd->blit_gc);
	XDestroyImage(vd->bkbuf);
	
	pthread_cond_destroy(&vd->ldr_finished_cond);
	pthread_cond_destroy(&vd->rdr_finished_cond);
	pthread_mutex_destroy(&vd->ldr_cond_mutex);
	pthread_mutex_destroy(&vd->rdr_cond_mutex);
	pthread_mutex_destroy(&vd->thread_notify_mutex);
	XtRemoveInput(vd->thread_notify_input);
	close(vd->tnfd[0]);
	close(vd->tnfd[1]);
	free(vd->title);
	if(vd->last_dest_dir) free(vd->last_dest_dir);

	app_inst.active_shells--;

	/* unlink and free the instance data container */
	if(vd!=viewers){
		struct viewer_data *prev=viewers;
		while(prev->next!=vd)
			prev=prev->next;
		prev->next=vd->next;
	}else{
		viewers=vd->next;
	}
	free(vd);
}

/*
 * Retrieve absolute path name from a file name.
 * The caller is responsible for freeing the allocated memory.
 */
static char* get_path(const char *file_name)
{
	char *token;
	char *path;

	path=realpath(file_name,NULL);
	token=strrchr(path,'/');
	if(token){
		if(token==path)
			token[1]='\0';
		else
			token[0]='\0';
	}
	return path;
}

/*
 * Construct and display an IMG_* error message
 */
static void report_img_error(struct viewer_data *vd, int img_errno)
{
	const char msg_fmt[]="%s: %s\n%s";
	char *ftitle;
	char *msg_buf;
	char *err_msg;
	char *reason_msg;

	err_msg=nlstr(APP_MSGSET,SID_EOPEN,"Could not open file.");
	reason_msg=img_strerror(img_errno);
	ftitle=strrchr(vd->file_name,'/');
	if(ftitle)
		ftitle++;
	else
		ftitle=vd->file_name;
	msg_buf=malloc(strlen(err_msg)+strlen(reason_msg)+
		strlen(msg_fmt)+strlen(ftitle)+1);
	sprintf(msg_buf,msg_fmt,ftitle,err_msg,reason_msg);
	message_box(vd->wshell,MB_ERROR_NB,NULL,msg_buf);
	free(msg_buf);
}

/*
 * Load and display next page in the image
 */
static void load_next_page(struct viewer_data *vd, Bool forward)
{
	int img_errno;
	struct stat st;
	
	dbg_assert(vd->img_file.npages);
	
	/* make sure the file is still there */
	if(stat(vd->file_name,&st) == -1){
		errno_message_box(vd->wshell,errno,
			nlstr(APP_MSGSET,SID_IMGERROR,
			"An error occured while reading file."),False);
		reset_viewer(vd);
		return;
	}
	
	/* if a loader thread is running set state to cancelled
	 * and wait for it to exit */
	pthread_mutex_lock(&vd->ldr_cond_mutex);
	if(vd->state&ISF_LOADING){
		vd->state|=ISF_CANCEL;
		if(vd->prog_timer) XtRemoveTimeOut(vd->prog_timer);
		if(vd->vprog_timer) XtRemoveTimeOut(vd->vprog_timer);
		vd->prog_timer=None;
		vd->vprog_timer=None;
		XClearArea(app_inst.display,XtWindow(vd->wview),0,0,0,0,False);
		pthread_cond_wait(&vd->ldr_finished_cond,&vd->ldr_cond_mutex);
	}
	pthread_mutex_unlock(&vd->ldr_cond_mutex);
	
	if(forward){
		if(++vd->cur_page==vd->img_file.npages)
			update_controls(vd);
	}else{
		if(--vd->cur_page==0)
			update_controls(vd);
	}

	vd->state&=(~ISF_READY);
	XClearArea(app_inst.display,XtWindow(vd->wview),0,0,0,0,False);
	set_status_msg(vd,SID_LOADING,"Loading...");
	img_errno=img_set_page(&vd->img_file,vd->cur_page);
	if(img_errno){
		report_img_error(vd,img_errno);
		reset_viewer(vd);
		return;
	}
	/* realloc storage and reset view values if pages differ in size */
	if(vd->img_file.width!=vd->image->width ||
		vd->img_file.height!=vd->image->height ||
		vd->img_file.bpp!=vd->image->depth){
		XDestroyImage(vd->image);
		img_errno=alloc_storage(vd);
		if(img_errno){
			report_img_error(vd,img_errno);
			reset_viewer(vd);
			return;
		}
		vd->xoff=vd->yoff=0;
		vd->zoom=1;
		update_props_msg(vd);
	}
	/* setup vprog if enabled */
	if(vd->vprog){
		img_fill_rect(vd->bkbuf,0,0,vd->bkbuf->width,
			vd->bkbuf->height,vd->bg_pixel);
		img_fill_rect(vd->image,0,0,vd->image->width,
			vd->image->height,vd->bg_pixel);
		if(vd->zoom_fit)
			vd->zoom=compute_fit_zoom(vd);
		else
			vd->zoom=1;
	}
	
	update_controls(vd);
	
	/* launch the loader thread */
	vd->state|=ISF_LOADING;
	vd->load_prog=0;
	if(pthread_create(&vd->ldr_thread,NULL,loader_thread,(void*)vd)){
		vd->state&=(~ISF_LOADING);
		report_img_error(vd,IMG_ENOMEM);
		reset_viewer(vd);
		return;
	}
	pthread_detach(vd->ldr_thread);
	
	/* setup timers */
	if(vd->vprog){
		vd->vprog_timer=XtAppAddTimeOut(app_inst.context,LOADER_FB_UPDATE_INT,
			load_fb_update_handler,(XtPointer)vd);
	}
	vd->prog_timer=XtAppAddTimeOut(app_inst.context,PROG_UPDATE_INT,
		load_prog_handler,(XtPointer)vd);
}

/*
 * Load next file in the directory
 */
static void load_next_file(struct viewer_data *vd, Bool forward)
{
	struct stat st;
	char *new_file_name;

	if(vd->dir_name && stat(vd->dir_name,&st)){
		errno_message_box(vd->wshell,errno,nlstr(
				APP_MSGSET,SID_EREADDIR,"Error reading directory."),False);
		return;
	}

	/* check if the directory cache is usable, re/build if not */	
	if(!vd->dir_files || (vd->dir_stat.st_mtime!=st.st_mtime) ||
		(vd->dir_stat.st_ino!=st.st_ino)){

		memcpy(&vd->dir_stat,&st,sizeof(struct stat));
		pthread_mutex_lock(&vd->ldr_cond_mutex);
		if(vd->state&ISF_LOADING){
			pthread_mutex_unlock(&vd->ldr_cond_mutex);
			reset_viewer(vd);
		}
		pthread_mutex_unlock(&vd->ldr_cond_mutex);
		vd->state|=DSF_READING;
		if(pthread_create(&vd->rdr_thread,NULL,dir_reader_thread,(void*)vd)){
			errno_message_box(vd->wshell,errno,NULL,False);
			return;
		}
		pthread_detach(vd->rdr_thread);
		vd->dir_forward=forward;
		set_widget_cursor(vd->wshell,CUR_HOURGLAS);
		update_controls(vd);
		set_status_msg(vd,SID_READDIRPG,"Reading directory...");
		/* this function will be invoked again after directory cache is built */
		return;
	}
	if(!vd->dir_nfiles) return;
	
	if(forward){
		if(vd->dir_cur_file+1==vd->dir_nfiles)
			vd->dir_cur_file=0;
		else
			vd->dir_cur_file++;
	}else{
		if(vd->dir_cur_file==0)
			vd->dir_cur_file=vd->dir_nfiles-1;
		else
			vd->dir_cur_file--;
	}
	new_file_name=malloc(strlen(vd->dir_name)+
		strlen(vd->dir_files[vd->dir_cur_file])+2);
	sprintf(new_file_name,"%s/%s",vd->dir_name,
		vd->dir_files[vd->dir_cur_file]);
	load_image(vd,new_file_name);
	free(new_file_name);
}

/*
 * Allocate an XImage according to img_file properties
 * Must return an IMG_* status value.
 */
static int alloc_storage(struct viewer_data *vd)
{
	/* storage for a complete image */
	vd->image=XCreateImage(app_inst.display,app_inst.visual_info.visual,
		app_inst.visual_info.depth,ZPixmap,0,NULL,vd->img_file.width,
		vd->img_file.height,app_inst.pixel_size,0);
	if(vd->image){
		vd->image->data=calloc(1,vd->img_file.height*vd->image->bytes_per_line);
	}
	if(!vd->image || !vd->image->data){
		XDestroyImage(vd->image);
		return IMG_ENOMEM;
	}
	vd->image->bitmap_bit_order=vd->image->byte_order=
		(is_big_endian())?MSBFirst:LSBFirst;
	_XInitImageFuncPtrs(vd->image);
	return 0;
}

/*
 * The image loader thread entry point. Don't make any X/Motif calls here.
 */
static void* loader_thread(void *arg)
{
	struct proc_thread_msg tmsg;
	struct loader_cb_data *cbd;
	struct viewer_data *vd=(struct viewer_data*)arg;
	int img_errno=0;

	cbd=calloc(1,sizeof(struct loader_cb_data));
	if(!cbd){
		img_errno=IMG_ENOMEM;
		free(cbd);
		goto loader_thread_exit;
	}
	cbd->vd=vd;
	/* check if pseudo-color and read the lookup table if so */
	if(vd->img_file.format==IMG_PSEUDO){
		cbd->clut=malloc(IMG_CLUT_SIZE);
		if(!cbd->clut){
			img_errno=IMG_ENOMEM;
			free(cbd);
			goto loader_thread_exit;
		}
		img_errno=img_read_cmap(&vd->img_file,cbd->clut);
		if(img_errno){
			free(cbd->clut);
			free(cbd);
			goto loader_thread_exit;
		}
	}

	img_errno=init_pixel_format(&cbd->image_pf,vd->img_file.bpp,
		vd->img_file.red_mask,vd->img_file.green_mask,
		vd->img_file.blue_mask,vd->img_file.alpha_mask,
		vd->img_file.bg_pixel,vd->img_file.flags);

	if(!img_errno){
		img_errno=img_read_scanlines(&vd->img_file,
			scanline_read_cb,(void*)cbd);
	}
	if(cbd->clut) free(cbd->clut);
	free(cbd);
	
	/* always go there to exit the thread */
	loader_thread_exit:
	pthread_mutex_lock(&vd->ldr_cond_mutex);

	tmsg.cancelled=(vd->state&ISF_CANCEL)?True:False;
	vd->state&=(~(ISF_LOADING|ISF_CANCEL));
	pthread_cond_signal(&vd->ldr_finished_cond);
	pthread_mutex_unlock(&vd->ldr_cond_mutex);
	tmsg.proc=TP_IMG_LOAD;
	tmsg.result=img_errno;
	pthread_mutex_lock(&vd->thread_notify_mutex);
	write(vd->tnfd[TNFD_OUT],&tmsg,sizeof(struct proc_thread_msg));
	pthread_mutex_unlock(&vd->thread_notify_mutex);
	return NULL;
}

/*
 * The scaline reader callback proc
 */
static int scanline_read_cb(unsigned long iscl,
	const uint8_t *data, void *client)
{
	struct loader_cb_data *cbd=(struct loader_cb_data*)client;
	uint8_t *ptr=(uint8_t*)&cbd->vd->image->data
		[iscl*cbd->vd->image->bytes_per_line];

	dbg_assert(iscl < cbd->vd->img_file.height);
	
	if(app_inst.visual_info.class==PseudoColor){
		if(cbd->vd->img_file.format==IMG_PSEUDO){
			remap_pixels(ptr,data,cbd->clut,cbd->vd->img_file.width);
		}else{
			rgb_pixels_to_clut(ptr,data,&cbd->image_pf,
				cbd->vd->img_file.width);
		}
	}else{
		if(cbd->vd->img_file.format==IMG_PSEUDO){
			clut_to_rgb_pixels(ptr,&cbd->vd->display_pf,data,
				cbd->clut,cbd->vd->img_file.width);
		}else{
			convert_rgb_pixels(ptr,&cbd->vd->display_pf,data,
				&cbd->image_pf,cbd->vd->img_file.width);
		}
	}
	if(cbd->vd->state&ISF_CANCEL) return IMG_READ_CANCEL;

	cbd->nscl++;
	cbd->vd->load_prog=ceil((float)cbd->nscl/
		((float)cbd->vd->img_file.height*0.01));

	return IMG_READ_CONT;
}

/*
 * Receives notifications from work threads through XtInput
 */
static void thread_callback_proc(XtPointer client, int *pfd, XtInputId *iid)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	struct proc_thread_msg tmsg;
	
	if(read(vd->tnfd[TNFD_IN],&tmsg,sizeof(struct proc_thread_msg))<1)
		return;

	if(tmsg.proc==TP_IMG_LOAD){	
		if(tmsg.result){
			report_img_error(vd,tmsg.result);
			reset_viewer(vd);
		}else if(!tmsg.cancelled){
			vd->state|=ISF_READY;
			display_status_summary(vd);
			update_controls(vd);
			if(vd->zoom_fit)
				vd->zoom=compute_fit_zoom(vd);
			else
				vd->zoom=1;
			update_back_buffer(vd);
			redraw_view(vd,False);
			update_page_msg(vd);
			update_pointer_shape(vd);
			XmUpdateDisplay(vd->wshell);
		}
	}else if(tmsg.proc==TP_DIR_READ){
		set_widget_cursor(vd->wshell,CUR_POINTER);
		update_controls(vd);
		display_status_summary(vd);
		XmUpdateDisplay(vd->wshell);
		if(tmsg.result){
			errno_message_box(vd->wshell,tmsg.result,
				nlstr(APP_MSGSET,SID_EREADDIR,
				"Error reading directory."),False);
		}else if(!tmsg.cancelled && vd->dir_nfiles){
			load_next_file(vd,vd->dir_forward);
		}
	}
}

/*
 * Timed framebuffer update handler
 */
static void load_fb_update_handler(XtPointer client, XtIntervalId *iid)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	
	if(vd->state&ISF_LOADING && !(vd->state&ISF_CANCEL)){
		update_back_buffer(vd);
		redraw_view(vd,False);
		vd->vprog_timer=XtAppAddTimeOut(app_inst.context,LOADER_FB_UPDATE_INT,
				load_fb_update_handler,(XtPointer)vd);
	}else{
		vd->vprog_timer=None;
	}
}

/*
 * Update the loading progress message.
 * This function is called by a timed event.
 */
static void load_prog_handler(XtPointer client, XtIntervalId *iid)
{
	Arg arg[1];
	XmString str;
	char *msg_text;
	char *prog_msg;
	size_t len;
	struct viewer_data *vd=(struct viewer_data*)client;
	
	if(vd->state&ISF_LOADING && !(vd->state&ISF_CANCEL)){
		msg_text=nlstr(APP_MSGSET,SID_LOADING,"Loading...");
		len=strlen(msg_text)+12;
		prog_msg=alloca(len+1);
		snprintf(prog_msg,len,"%s%d%%",msg_text,vd->load_prog);
		str=XmStringCreateLocalized(prog_msg);
		XtSetArg(arg[0],XmNlabelString,str);
		XtSetValues(vd->wmsg,arg,1);
		XmUpdateDisplay(vd->wmsg);
		XmStringFree(str);
		vd->prog_timer=XtAppAddTimeOut(app_inst.context,PROG_UPDATE_INT,
				load_prog_handler,(XtPointer)vd);
	}else{
		vd->prog_timer=None;
	}
}

/*
 * qsort compare function for read_directory
 */
static int fn_sort_compare(const void *pa, const void *pb)
{
	const char **a=(const char**)pa;
	const char **b=(const char**)pb;
	return strcmp(*a,*b);
}

/*
 * Directory reader thread entry point
 */
static void* dir_reader_thread(void *arg)
{
	struct viewer_data *vd=(struct viewer_data*)arg;
	struct proc_thread_msg tmsg;
	DIR *dir;
	struct dirent *dir_ent;
	size_t buf_size=0;
	int i;
	int icur_file=0;
	int ret_code=0;
	char **files=NULL;
	unsigned long nfiles=0;

	dbg_assert(vd->dir_name);
	
	if(vd->dir_nfiles){
		while(vd->dir_nfiles--)
			free(vd->dir_files[vd->dir_nfiles]);
		vd->dir_nfiles=0;
	}
	
	if(vd->dir_files) {
		free(vd->dir_files);
		vd->dir_files=NULL;
		vd->dir_cur_file=0;
	}

	dir=opendir(vd->dir_name);
	if(!dir){
		ret_code=errno;
		goto exit_thread;
	}
	
	while((dir_ent=readdir(dir))){
		if(vd->state & DSF_CANCEL) {
			closedir(dir);
			goto exit_thread;
		}

		if(!strcmp(dir_ent->d_name, "..") ||
			!strcmp(dir_ent->d_name, ".")) continue;
			
		if(nfiles==buf_size){
			char **new_buf;
			buf_size+=DIR_CACHE_GROWBY;
			new_buf=realloc(files,buf_size*sizeof(char*));
			if(!new_buf){
				ret_code=errno;
				closedir(dir);
				goto exit_thread;
			}
			files=new_buf;
		}
		if(img_get_format_info(dir_ent->d_name)){
			files[nfiles]=strdup(dir_ent->d_name);
		}else{	continue; }
		nfiles++;
	}
	closedir(dir);
	
	if(nfiles){
		/* sort file names and move file pointer to current */
		qsort(files,nfiles,sizeof(char*),&fn_sort_compare);

		if(vd->file_name){
			char *cur_file;
			cur_file=strrchr(vd->file_name,'/');
			if(cur_file)
				cur_file++;
			else
				cur_file=vd->file_name;

			for(i=0; i<nfiles; i++){
				if(vd->state&DSF_CANCEL) goto exit_thread;
				if(!strcmp(files[i],cur_file))
					icur_file=i;
			}
		}
	}
	
	/* cleanup and terminate */
	exit_thread:
	pthread_mutex_lock(&vd->rdr_cond_mutex);
	if(ret_code || (vd->state&DSF_CANCEL)){
		while(nfiles--) free(files[nfiles]);
		free(files);
		if(vd->state&DSF_CANCEL)
			pthread_cond_signal(&vd->rdr_finished_cond);
	}else{
		vd->dir_files=files;
		vd->dir_nfiles=nfiles;
		vd->dir_cur_file=icur_file;
	}
	tmsg.cancelled=(vd->state&DSF_CANCEL)?True:False;
	vd->state&=(~(DSF_READING|DSF_CANCEL));
	pthread_mutex_unlock(&vd->rdr_cond_mutex);
	tmsg.proc=TP_DIR_READ;
	tmsg.result=ret_code;
	pthread_mutex_lock(&vd->thread_notify_mutex);
	write(vd->tnfd[TNFD_OUT],&tmsg,sizeof(struct proc_thread_msg));
	pthread_mutex_unlock(&vd->thread_notify_mutex);

	return NULL;
}

/*
 * Free directory cache data, stop directory reader thread if any is running.
 */
static void clear_dir_cache(struct viewer_data *vd)
{
	if(!vd->dir_files) return;
	pthread_mutex_lock(&vd->rdr_cond_mutex);
	if(vd->state&DSF_READING){
		vd->state|=DSF_CANCEL;
		pthread_cond_wait(&vd->rdr_finished_cond,&vd->rdr_cond_mutex);
		pthread_mutex_unlock(&vd->rdr_cond_mutex);
		return;
	}
	pthread_mutex_unlock(&vd->rdr_cond_mutex);
	while(vd->dir_nfiles--) free(vd->dir_files[vd->dir_nfiles]);
	free(vd->dir_files);
	free(vd->dir_name);
	vd->dir_files=NULL;
	vd->dir_nfiles=0;
	vd->dir_name=NULL;
	vd->dir_cur_file=0;
}

/*
 * Reset the viewer to the initial state.
 */
static void reset_viewer(struct viewer_data *vd)
{
	/* if a loader thread is running set state to cancelled
	 * and wait for it to exit */
	pthread_mutex_lock(&vd->ldr_cond_mutex);
	if(vd->state&ISF_LOADING){
		vd->state|=ISF_CANCEL;
		if(vd->prog_timer) XtRemoveTimeOut(vd->prog_timer);
		if(vd->vprog_timer) XtRemoveTimeOut(vd->vprog_timer);
		vd->prog_timer=None;
		vd->vprog_timer=None;
		pthread_cond_wait(&vd->ldr_finished_cond,&vd->ldr_cond_mutex);
	}
	pthread_mutex_unlock(&vd->ldr_cond_mutex);

	if(vd->image){
		free(vd->image->data);
		vd->image->data=NULL;
		XDestroyImage(vd->image);
		vd->image=NULL;
	}
	if(vd->file_name){
		free(vd->file_name);
		vd->file_name=NULL;
	}
	if(vd->state&ISF_OPENED){
		img_close(&vd->img_file);
	}
	
	#ifdef ENABLE_CDE
	/* notify the requestor if the viewer was instantiated through ToolTalk */
	if(vd->tt_disp_req){
		ttmedia_load_reply(vd->tt_disp_req,NULL,0,True);
		vd->tt_disp_req=NULL;
	}
	#endif /* ENABLE_CDE */
	
	/* reset properties to defaults */
	vd->state&=(~(ISF_OPENED|ISF_READY));
	vd->cur_page=0;
	vd->zoom=1;
	vd->xoff=0;
	vd->yoff=0;
	if(!vd->keep_tform) vd->tform=0;
	
	/* update the user interface */
	XClearArea(app_inst.display,XtWindow(vd->wview),0,0,0,0,False);
	update_shell_title(vd);
	display_status_summary(vd);
	update_props_msg(vd);
	update_pointer_shape(vd);
	update_controls(vd);
	XmUpdateDisplay(vd->wshell);
}

/*
 * Compute the zoom value so that the image fits within view boundaries.
 * Returns 1 if the image is smaller than the view.
 */
static float compute_fit_zoom(struct viewer_data *vd)
{
	Dimension vw=0, vh=0;
	int img_width, img_height, img_min;
	float xratio, yratio;
	float zoom;
	XtVaGetValues(vd->wview,XmNwidth,&vw,XmNheight,&vh,NULL);
	
	dbg_assert(vd->image->width && vd->image->height);
	
	img_width=(vd->tform&IMGT_ROTATE)?vd->image->height:vd->image->width;
	img_height=(vd->tform&IMGT_ROTATE)?vd->image->width:vd->image->height;
	xratio=(float)vw/img_width;
	yratio=(float)vh/img_height;
	
	if(xratio<1.0 || yratio<1.0){
		zoom=(xratio<yratio?xratio:yratio);
		compute_image_dimensions(vd,zoom,vd->tform,&img_width,&img_height);
		img_min = ((img_width<img_height)?img_width:img_height);
		zoom = img_min ? zoom : 1;
	}else{
		zoom=1;
	}

	return zoom;
}

/*
 * Compute image dimensions according to the specified zoom value
 * and transformations flags.
 */
static void compute_image_dimensions(struct viewer_data *vd,
	float zoom, unsigned int tform, int *width, int *height)
{
	if(tform&IMGT_ROTATE){
		*width=vd->image->height*zoom;
		*height=vd->image->width*zoom;
	}else{
		*width=vd->image->width*zoom;
		*height=vd->image->height*zoom;
	}
}

/*
 * Set shell and icon title to reflect the current state of the viewer instance.
 */
static void update_shell_title(struct viewer_data *vd)
{
	if(vd->state&ISF_OPENED){
		char *buffer;
		char *ftitle;
		ftitle=strrchr(vd->file_name,'/');
		if(!ftitle)
			ftitle=vd->file_name;
		else
			ftitle++;
		buffer=malloc(strlen(vd->title)+strlen(ftitle)+4);
		sprintf(buffer,"%s - %s",ftitle,vd->title);
		XtVaSetValues(vd->wshell,XmNtitle,buffer,XmNiconName,buffer,NULL);
		free(buffer);
	}else{
		XtVaSetValues(vd->wshell,XmNtitle,vd->title,
			XmNiconName,vd->title,NULL);
	}
}

/*
 * Display a message in the status area
 */
static void set_status_msg(struct viewer_data *vd,
	int msg_id, const char *text)
{
	Arg arg[1];
	XmString str;

	str=XmStringCreateLocalized(nlstr(APP_MSGSET,msg_id,(char*)text));
	XtSetArg(arg[0],XmNlabelString,str);
	XtSetValues(vd->wmsg,arg,1);
	XmStringFree(str);
	XmUpdateDisplay(vd->wmsg);
}

/*
 * Build file info summary and put it in the status area
 */
static void display_status_summary(struct viewer_data *vd)
{
	char *buffer;
	size_t len;
	struct tm *time;
	char size_str[SIZE_CS_MAX];
	const char fmt_string[]="%s, %s, %d-%.2d-%.2d %.2d:%.2d";
	XmString xm_str;
	Arg arg[1];
	
	if(vd->state&ISF_OPENED){
		get_size_string(vd->file_size, size_str);

		time=localtime(&vd->img_file.cr_time);
		
		len = snprintf(NULL, 0, fmt_string, vd->img_file.type_str,
			size_str, time->tm_year + 1900, time->tm_mon + 1,
			time->tm_mday + 1, time->tm_hour, time->tm_min) + 1;

		buffer = malloc(len);
	
		snprintf(buffer, len, fmt_string, vd->img_file.type_str,
			size_str, time->tm_year + 1900, time->tm_mon + 1,
			time->tm_mday + 1, time->tm_hour, time->tm_min);
			
		xm_str=XmStringCreateLocalized(buffer);
		XtSetArg(arg[0],XmNlabelString,xm_str);
		XtSetValues(vd->wmsg,arg,1);
		XmStringFree(xm_str);
		free(buffer);
		XmUpdateDisplay(vd->wprops);
	}else{
		update_page_msg(vd);
		set_status_msg(vd,SID_READY,"Ready");
	}
}

/*
 * Update the properties status area
 */
static void update_props_msg(struct viewer_data *vd)
{
	/* NxN - N BPP*/
	char props_str[32];
	XmString xm_str;
	Arg arg[1];

	if(vd->state&ISF_OPENED){
		snprintf(props_str,30,"%ldx%ld, %hd BPP",vd->img_file.width,
			vd->img_file.height,vd->img_file.orig_bpp);
	}else{
		strncpy(props_str,nlstr(APP_MSGSET,SID_NOIMAGE,"No image"),30);
	}
	xm_str=XmStringCreateLocalized(props_str);
	XtSetArg(arg[0],XmNlabelString,xm_str);
	XtSetValues(vd->wprops,arg,1);
	XmUpdateDisplay(vd->wprops);
	XmStringFree(xm_str);
}

/*
 * Show the current zoom value and the page number in the status area
 */
static void update_page_msg(struct viewer_data *vd)
{
	XmString xmstr;
	Arg arg[1];
	/* D.F% (N/N) */
	char zoom_str[32];
	
	if(vd->img_file.npages>1){
		snprintf(zoom_str,sizeof(zoom_str),"%.0f%% (%d/%d)",
			(vd->zoom*100),vd->cur_page+1,vd->img_file.npages);
	}else{
		snprintf(zoom_str,sizeof(zoom_str),"%.0f%%",(vd->zoom*100));
	}
	xmstr=XmStringCreateLocalized(zoom_str);
	XtSetArg(arg[0],XmNlabelString,xmstr);
	XtSetValues(vd->wzoom,arg,1);
	XmStringFree(xmstr);
	XmUpdateDisplay(vd->wzoom);
}

/*
 * Update input widgets to reflect the current viewer state
 */
static void update_controls(struct viewer_data *vd)
{
	Boolean ready=((vd->state&ISF_READY)!=0);
	
	if(vd->state&DSF_READING){
		XtSetSensitive(vd->wmenubar,False);
		if(vd->show_tbr) XtSetSensitive(vd->wtoolbar,False);
		return;
	}else{
		XtSetSensitive(vd->wmenubar,True);
		if(vd->show_tbr) XtSetSensitive(vd->wtoolbar,True);
	}
	
	XtSetSensitive(get_menu_item(vd,"*zoomMenu"),ready);
	XtSetSensitive(get_menu_item(vd,"*editMenu"),ready);
	XtSetSensitive(get_menu_item(vd,"*refresh"),ready);
	XtSetSensitive(get_menu_item(vd,"*editMenu"),ready);
	XtSetSensitive(get_menu_item(vd,"*rotateMenu"),ready);

	XtSetSensitive(get_menu_item(vd,"*browseHere"),(vd->file_name!=NULL));
	
	XtSetSensitive(get_menu_item(vd,"*nextFile"),
		(vd->dir_name || vd->file_name));
	XtSetSensitive(get_menu_item(vd,"*previousFile"),
		(vd->dir_name || vd->file_name));

	XtSetSensitive(get_menu_item(vd,"*nextPage"),
		(ready && vd->img_file.npages &&
		vd->cur_page+1<vd->img_file.npages));
	XtSetSensitive(get_menu_item(vd,"*previousPage"),
		(ready && vd->img_file.npages && vd->cur_page!=0));
	
	XtSetSensitive(get_menu_item(vd,"*zoomIn"),True);
	XtSetSensitive(get_menu_item(vd,"*zoomOut"),True);
	XtSetSensitive(get_menu_item(vd,"*edit"),(ready && init_app_res.edit_cmd));

	XmUpdateDisplay(vd->wmenubar);

	if(vd->show_tbr){
		XtSetSensitive(get_toolbar_item(vd->wtoolbar,"*browseHere"),
			(vd->file_name!=NULL));
		XtSetSensitive(get_toolbar_item(vd->wtoolbar,"*zoomIn"),ready);
		XtSetSensitive(get_toolbar_item(vd->wtoolbar,"*zoomOut"),ready);
		XtSetSensitive(get_toolbar_item(vd->wtoolbar,"*zoomNone"),ready);
		XtSetSensitive(get_toolbar_item(vd->wtoolbar,
			"*flipHorizontally"),ready);
		XtSetSensitive(get_toolbar_item(vd->wtoolbar,
			"*flipVertically"),ready);
		XtSetSensitive(get_toolbar_item(vd->wtoolbar,"*rotateLeft"),ready);
		XtSetSensitive(get_toolbar_item(vd->wtoolbar,"*rotateRight"),ready);
		XtSetSensitive(get_toolbar_item(vd->wtoolbar,"*nextFile"),
			(vd->dir_name || vd->file_name));
		XtSetSensitive(get_toolbar_item(vd->wtoolbar,"*previousFile"),
			(vd->dir_name || vd->file_name));
	
		XmUpdateDisplay(vd->wtoolbar);
	}
}

/*
 * Set the pointer shape to indicate if panning is possible
 */
static void update_pointer_shape(struct viewer_data *vd)
{
	Dimension view_width,view_height;
	int img_width,img_height;
	Arg arg[]={{XmNwidth,(XtArgVal)&view_width},
		{XmNheight,(XtArgVal)&view_height}};
	
	if(vd->state&ISF_READY){
		XtGetValues(vd->wview,arg,2);
		compute_image_dimensions(vd,vd->zoom,vd->tform,
			&img_width,&img_height);
		if(img_width>view_width || img_height>view_height)
			set_widget_cursor(vd->wview,CUR_GRAB);
		else
			set_widget_cursor(vd->wview,CUR_POINTER);
	}else{
		set_widget_cursor(vd->wview,CUR_POINTER);
	}
}


/*
 * Update the zoom value, recompute offsets and redraw the view.
 */
static void zoom_view(struct viewer_data *vd, float zoom)
{
	Boolean erase;
	int iw, ih, ipw, iph;
	int xoff=vd->xoff, yoff=vd->yoff;
	float min_zoom;
	float nx, ny;

	/* compute the minimum zoom value */
	min_zoom = ((vd->image->width > vd->image->height) ?
		((float)MIN_ZOOMED_SIZE / vd->image->width) :
		((float)MIN_ZOOMED_SIZE / vd->image->height));
	if(min_zoom > 1.0) min_zoom = 1.0;

	/* reenable controls if we're out of the min/max zone */
	if(vd->zoom==MAX_ZOOM){
		XtSetSensitive(get_menu_item(vd,"*zoomIn"),True);
		if(vd->show_tbr) XtSetSensitive(
			get_toolbar_item(vd->wtoolbar,"*zoomIn"),True);
	}else if(vd->zoom==min_zoom){
		XtSetSensitive(get_menu_item(vd,"*zoomOut"),True);
		if(vd->show_tbr) XtSetSensitive(
			get_toolbar_item(vd->wtoolbar,"*zoomOut"),True);
	}
		
	/* clamp the zoom value */
	if(zoom<=min_zoom){
		zoom=min_zoom;
		XtSetSensitive(get_menu_item(vd,"*zoomOut"),False);
		if(vd->show_tbr) XtSetSensitive(
			get_toolbar_item(vd->wtoolbar,"*zoomOut"),False);
	}else if(zoom>=MAX_ZOOM){
		zoom=MAX_ZOOM;
		XtSetSensitive(get_menu_item(vd,"*zoomIn"),False);
		if(vd->show_tbr) XtSetSensitive(
			get_toolbar_item(vd->wtoolbar,"*zoomIn"),False);
	}
	
	/* figure out how to distribute offsets for the new image size */
	compute_image_dimensions(vd,vd->zoom,vd->tform,&ipw,&iph);
	compute_image_dimensions(vd,zoom,vd->tform,&iw,&ih);
	nx=(float)(ipw-vd->bkbuf->width)/(xoff?xoff:1);
	ny=(float)(iph-vd->bkbuf->height)/(yoff?yoff:1);

	if(zoom>vd->zoom){
		/* we want to zoom centered if the image was previously
		 * smaller than the viewport */
		if(ipw<=vd->bkbuf->width && iw>vd->bkbuf->width)
			xoff-=(iw-vd->bkbuf->width)/2;
		else if(xoff) xoff-=fabs((float)(ipw-iw)/nx);

		if(iph<=vd->bkbuf->height && ih>vd->bkbuf->height)
			yoff-=(ih-vd->bkbuf->height)/2;
		else if(yoff) yoff-=fabs((float)(iph-ih)/ny);

		erase=False;
	}else{
		if(xoff) xoff+=fabs((float)(ipw-iw)/nx);
		if(yoff) yoff+=fabs((float)(iph-ih)/ny);
		erase=True;
	}

	/* clamp and store offsets */
	vd->xoff=(xoff<0)?xoff:0;
	vd->yoff=(yoff<0)?yoff:0;

	/* store the zoom value and update visuals */
	vd->zoom=zoom;
	update_page_msg(vd);
	update_back_buffer(vd);
	redraw_view(vd,erase);
	update_pointer_shape(vd);
}

/*
 * Compute offsets according to new transform flags and redraw
 */
static void rotate_view(struct viewer_data *vd, Boolean cw)
{
	float xoff=vd->xoff;
	float yoff=vd->yoff;
	unsigned int tform=vd->tform;
	unsigned int rotation=0;
	
	/* The blitter doesn't do per-pixel transforms, because this is not
	 * really needed, instead it uses 90 deg cw rotation + flipping to
	 * simulate transformations. This part figures out the flag combination. */
	if(tform&IMGT_ROTATE){
		if(tform&IMGT_VFLIP)
			rotation=270;
		else
			rotation=90;
	}else{
		if(tform&IMGT_VFLIP) rotation=180;
	}
	if(cw) rotation+=90; else rotation-=90;

	switch(rotation){
		case 90: tform=IMGT_ROTATE; break;
		case 180: tform=IMGT_VFLIP|IMGT_HFLIP; break;
		case -90:
		case 270: tform=IMGT_ROTATE|IMGT_VFLIP|IMGT_HFLIP; break;
		case 0:
		case 360: tform=0; break;
	};
	
	/* adjust the offsets if image orientation has changed */
	if((tform&IMGT_ROTATE && !(vd->tform&IMGT_ROTATE))||
		(!(tform&IMGT_ROTATE) && vd->tform&IMGT_ROTATE))
	{
		Dimension vw, vh;
		int iw, ih;
		float vpc=0;
		float hpc=0;
		
		XtVaGetValues(vd->wview,XmNwidth,&vw,XmNheight,&vh,NULL);
		compute_image_dimensions(vd,vd->zoom,vd->tform,&iw,&ih);
		
		if(vw<iw) hpc=(float)fabs(xoff)/(iw-vw);
		if(vh<ih) vpc=(float)fabs(yoff)/(ih-vh);
	
		compute_image_dimensions(vd,vd->zoom,tform,&iw,&ih);
		
		if(vw<iw) xoff= -((iw-vw)*hpc); else xoff=0;
		if(vh<ih) yoff= -((ih-vh)*vpc); else yoff=0;
	}

	vd->tform=tform;
	if(vd->zoom_fit){
		zoom_view(vd,compute_fit_zoom(vd));
	}else{
		vd->xoff=(short)xoff;
		vd->yoff=(short)yoff;
	}
	update_pointer_shape(vd);
	update_back_buffer(vd);
	redraw_view(vd,True);
}

/*
 * Scroll image. x,y specify offsets relative to the current scroll position.
 */
static void scroll_view(struct viewer_data *vd, int x, int y)
{
	int iw,ih;
	int xmax, ymax;
	float xoff=vd->xoff, yoff=vd->yoff;
	unsigned short tform=vd->tform^vd->img_file.tform;
	
	if(tform&IMGT_ROTATE){
		if(!(tform&IMGT_VFLIP)) x=(-x);
		if(tform&IMGT_HFLIP) y=(-y);
	}else{
		if(tform&IMGT_HFLIP) x=(-x);
		if(tform&IMGT_VFLIP) y=(-y);
	}	
	/* compute and clamp scroll offsets */
	compute_image_dimensions(vd,vd->zoom,vd->tform,&iw,&ih);
	xmax= -(iw-vd->bkbuf->width);
	ymax= -(ih-vd->bkbuf->height);
	if(xmax<0){
		if(xoff+x < xmax) x-=(xoff+x)-xmax;
		else if(xoff+x > 0)	x-=xoff+x;
	}else x=0;
	if(ymax<0){
		if(yoff+y < ymax) y-=(yoff+y)-ymax;
		else if(yoff+y > 0)	y-=yoff+y;
	}else y=0;
	
	if(!x && !y) return;

	/* store the offsets and sync the image */
	vd->xoff += x;
	vd->yoff += y;

	update_back_buffer(vd);
	redraw_view(vd,False);
}

/*
 * Update the back-buffer
 */
static void update_back_buffer(struct viewer_data *vd)
{
	int sx, sy, sw, sh;
	unsigned short tform = vd->tform ^ vd->img_file.tform;
	unsigned short intp = (init_app_res.int_up ? BLTF_INT_UP : 0) |
		(init_app_res.int_down ? BLTF_INT_DOWN : 0);
	
	if(init_app_res.fast_pan && vd->panning) {
		intp &= ~(BLTF_INT_UP|BLTF_INT_DOWN);
	}

	/* Offsets are screen/window coordinates, so we need to flip them
	 * according to the image rotation flag for blitting. */
	if(tform & IMGT_ROTATE) {
		sx = fabs((float)vd->yoff / vd->zoom);
		sy = fabs((float)vd->xoff / vd->zoom);
	} else {
		sx = fabs((float)vd->xoff / vd->zoom);
		sy = fabs((float)vd->yoff / vd->zoom);
	}

	sw = vd->image->width - sx;
	sh = vd->image->height - sy;
	if(sw && sh) img_blt(vd->image, sx, sy, sw, sh,
		vd->bkbuf, vd->zoom, tform, intp);
}


/*
 * Redraw the view area. Clear it before drawing if necessary.
 */
static void redraw_view(struct viewer_data *vd, Boolean clear)
{
	int iw, ih;
	
	compute_image_dimensions(vd,vd->zoom,vd->tform,&iw,&ih);

	/* only clear it if the image is smaller than the view area */
	if((vd->bkbuf->width>iw || vd->bkbuf->height>ih) && clear){
		XClearArea(app_inst.display,XtWindow(vd->wview),0,0,0,0,True);
	}else{
		XExposeEvent evt;
		XmDrawingAreaCallbackStruct cs;

		evt.type=Expose;
		evt.serial=0;
		evt.send_event=False;
		evt.display=app_inst.display;
		evt.window=XtWindow(vd->wview);
		evt.x=0;
		evt.y=0;
		evt.width=vd->bkbuf->width;
		evt.height=vd->bkbuf->height;
		evt.count=0;

		cs.reason=0;
		cs.window=evt.window;
		cs.event=(XEvent*)&evt;
		expose_cb(vd->wview,(XtPointer)vd,(XtPointer)&cs);
	}
}

/*
 * Called on shell resize 
 */
static void resize_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	struct viewer_data *vd=(struct viewer_data*)client_data;
	Dimension vw=0, vh=0;
	Arg arg[]={{XmNwidth,(XtArgVal)&vw},{XmNheight,(XtArgVal)&vh}};
	size_t size;
	
	if(!(vd->state&(ISF_LOADING|ISF_READY))) return;
	/* grow the back buffer if necessary */
	XtGetValues(vd->wview,arg,2);
	size=(vw*vh)*(vd->bkbuf->bitmap_pad/8);
	if(vd->bkbuf_size<size){
		void *new_mem;
		new_mem=realloc(vd->bkbuf->data,size);
		if(!new_mem){
			reset_viewer(vd);
			message_box(vd->wshell,MB_ERROR,NULL,nlstr(APP_MSGSET,SID_ENORES,
				"Not enough resources available for this task."));
			destroy_viewer(vd);
			return;
		}
		vd->bkbuf->data=new_mem;
		vd->bkbuf_size=size;
	}
	
	/* update and re-initialize the back-buffer XImage parameters */
	vd->bkbuf->width=vw;
	vd->bkbuf->height=vh;
	vd->bkbuf->bytes_per_line=0;
	XInitImage(vd->bkbuf);
	img_fill_rect(vd->bkbuf,0,0,vw,vh,vd->bg_pixel);

	/* recompute the zoom and clamp offsets */
	if(vd->zoom_fit) vd->zoom=compute_fit_zoom(vd);
	if(vd->xoff || vd->yoff){
		int iw, ih;
		int dx, dy;
		compute_image_dimensions(vd,vd->zoom,vd->tform,&iw,&ih);
		dx=iw-vw;
		dy=ih-vh;
		/* offsets are always <= zero */
		if(abs(vd->xoff)>dx) vd->xoff-=(vd->xoff+dx);
		if(vd->xoff>0) vd->xoff=0;
		if(abs(vd->yoff)>dy) vd->yoff-=(vd->yoff+dy);
		if(vd->yoff>0) vd->yoff=0;
	}
	update_page_msg(vd);
	update_pointer_shape(vd);
	update_back_buffer(vd);
	redraw_view(vd,True);
}

/*
 * Called when the drawing area is exposed
 */
static void expose_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	struct viewer_data *vd=(struct viewer_data*)client_data;
	XExposeEvent *evt=(XExposeEvent*)
		((XmDrawingAreaCallbackStruct*)call_data)->event;
	Dimension view_width,view_height;
	int dest_x,dest_y;
	int src_x,src_y,img_width,img_height;
	Arg arg[]={{XmNwidth,(XtArgVal)&view_width},
		{XmNheight,(XtArgVal)&view_height}};
	
	if(!(vd->state&(ISF_LOADING|ISF_READY))) return;
	
	XtGetValues(vd->wview,arg,2);
	compute_image_dimensions(vd,vd->zoom,vd->tform,&img_width,&img_height);

	/* if the displayed image is smaller than the view, and therefore only
	 * occupies a portion of the back-buffer, it is drawn centered within
	 * the view */
	dest_x=(view_width>img_width)?((view_width-img_width)/2):0;
	dest_y=(view_height>img_height)?((view_height-img_height)/2):0;

	/* check if the damaged rectangle is totally outside the image */
	if(((evt->x>dest_x+img_width)||(evt->y>dest_y+img_height))||
		((evt->x+evt->width<dest_x) || (evt->y+evt->height<dest_y)))
		return;

	/* now clip event coordinates to the image area */
	src_x=(evt->x<dest_x)?0:(evt->x-dest_x);
	src_y=(evt->y<dest_y)?0:(evt->y-dest_y);
	img_width=(src_x+evt->width>img_width)?img_width-src_x:evt->width;
	img_height=(src_y+evt->height>img_height)?img_height-src_y:evt->height;

	/* redraw damaged portion of the image */
	XPutImage(app_inst.display,evt->window,vd->blit_gc,vd->bkbuf,
		src_x,src_y,dest_x+src_x,dest_y+src_y,img_width,img_height);
	XFlush(app_inst.display);
}

/*
 * Called on drawing area interaction
 */
static void input_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	XmDrawingAreaCallbackStruct *cbs=
		(XmDrawingAreaCallbackStruct*)call_data;
	struct viewer_data *vd=(struct viewer_data*)client_data;
	Dimension view_width,view_height;
	int img_width, img_height;
	Arg arg[]={{XmNwidth,(XtArgVal)&view_width},
		{XmNheight,(XtArgVal)&view_height}};
	static int cx=0, cy=0;
	
	if(!(vd->state&ISF_READY)){
		/* handle cancel requests */
		if(vd->state&ISF_LOADING &&	cbs->event->type==KeyPress){
			XKeyEvent *e=(XKeyEvent*)cbs->event;
			Modifiers mods;
			KeySym sym;
			XmTranslateKey(app_inst.display,e->keycode,e->state,&mods,&sym);
			if(sym==osfXK_Cancel) cancel_cb(vd);
		}
		return;
	}
	
	XtGetValues(w,arg,2);
	compute_image_dimensions(vd,vd->zoom,vd->tform,&img_width,&img_height);

	switch(cbs->event->type){
		case ButtonPress:
		cx=cbs->event->xbutton.x;
		cy=cbs->event->xbutton.y;
		
		if((cbs->event->xbutton.button == Button1) &&
			(img_width > view_width || img_height > view_height)) {
				set_widget_cursor(w,CUR_DRAG);
				vd->panning = True;
		} else if(cbs->event->xbutton.button == Button4) {
			zoom_view(vd, vd->zoom * vd->zoom_inc);
		} else if(cbs->event->xbutton.button == Button5) {
			zoom_view(vd, vd->zoom / vd->zoom_inc);
		}
		break;	/* ButtonPress */
		
		case ButtonRelease:
		/* set the appropriate pointer */
		if(img_width>view_width || img_height>view_height)
			set_widget_cursor(w,CUR_GRAB);
		else
			set_widget_cursor(w,CUR_POINTER);
		
		vd->panning = False;
		if(init_app_res.fast_pan) {
			update_back_buffer(vd);
			redraw_view(vd,False);
		}
		break; /* ButtonRelease */
		
		/* Pan on MB1 */
		case MotionNotify:
		if(cbs->event->xbutton.state&Button1Mask){
			static int dx=0, dy=0;
			/* compute pan offset deltas */
			if(img_width>view_width)
				dx+=(signed)cbs->event->xbutton.x-cx;
			if(img_height>view_height)
				dy+=(signed)cbs->event->xbutton.y-cy;
			/* scroll zoomed in images by pixel square */
			if(vd->zoom>1){
				if(abs(dx)>vd->zoom || abs(dy)>vd->zoom){
					scroll_view(vd,dx,dy);
					dx=0; dy=0;
				}
			}else{
				scroll_view(vd,dx,dy);
				dx=0; dy=0;
			}
			cx=cbs->event->xbutton.x;
			cy=cbs->event->xbutton.y;
		}
		break; /* MotionNotify */
		
		case KeyPress:{
			XKeyEvent *e=(XKeyEvent*)cbs->event;
			Modifiers mods;
			KeySym sym;
			XEvent etmp;
			
			XmTranslateKey(app_inst.display,e->keycode,e->state,&mods,&sym);
			switch(sym){
				case osfXK_Up:
				scroll_view(vd,0,vd->key_pan_amount);
				break;
				case osfXK_Down:
				scroll_view(vd,0,-vd->key_pan_amount);
				break;
				case osfXK_Left:
				scroll_view(vd,vd->key_pan_amount,0);
				break;
				case osfXK_Right:
				scroll_view(vd,-vd->key_pan_amount,0);
				break;
			}
			
			/* discard stale events (in case we lag behind autorepeat),
			 * otherwise scrolling may continue for a bit even after
			 * the user has released the key */
			while(XCheckMaskEvent(app_inst.display,
				KeyPressMask|KeyReleaseMask,&etmp)){
				if(etmp.xkey.keycode!=e->keycode){
					XPutBackEvent(app_inst.display,&etmp);
					break;
				}
			}
		}break;
	}
}

/*
 * Cancel whatever the viewer is busy with.
 */
static void cancel_cb(struct viewer_data *vd)
{
	Boolean reset=False;

	pthread_mutex_lock(&vd->rdr_cond_mutex);
	if(vd->state&DSF_READING){
		vd->state|=DSF_CANCEL;
		pthread_cond_wait(&vd->rdr_finished_cond,&vd->rdr_cond_mutex);
	}
	pthread_mutex_unlock(&vd->rdr_cond_mutex);

	pthread_mutex_lock(&vd->ldr_cond_mutex);
	if(vd->state&ISF_LOADING){
		reset=True;
		vd->state|=ISF_CANCEL;
		pthread_cond_wait(&vd->ldr_finished_cond,&vd->ldr_cond_mutex);
	}
	pthread_mutex_unlock(&vd->ldr_cond_mutex);
	if(reset) reset_viewer(vd);
}

/*
 * The window manager <close> callback
 */
static void close_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	struct viewer_data *vd=(struct viewer_data*)client_data;

	destroy_viewer(vd);
	if(!app_inst.active_shells){
		dbg_printf("exit flag set in %s: %s()\n",__FILE__,__FUNCTION__);
		set_exit_flag(EXIT_SUCCESS);
	}
}

/*
 * Menubar/Accelerator callbacks
 */
static void pin_window_cb(Widget w, XtPointer client, XtPointer call)
{
	Widget wbutton;
	
	/* this menu item is only available with tooltalk enabled */
	struct viewer_data *vd=(struct viewer_data*)client;
	vd->pinned=((XmToggleButtonCallbackStruct*)call)->set;
	
	/* figure out which of menu/toolbar button we need to sync */
	wbutton=get_menu_item(vd,"*pinThis");
	if(w!=wbutton){
		XmToggleButtonGadgetSetState(wbutton,vd->pinned,False);
	}else{
		XmToggleButtonGadgetSetState(
			get_toolbar_item(vd->wtoolbar,"*pinThis"),
			vd->pinned,False);
	}

}

static void zoom_fit_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	Boolean set=((XmToggleButtonCallbackStruct*)call)->set;
	float zoom;
	
	vd->zoom_fit=set;
	/* for convenience this menu is enabled even if no image is loaded yet */
	if(vd->state&ISF_READY){
		if(set)
			zoom=compute_fit_zoom(vd);
		else
			zoom=1;
		zoom_view(vd,zoom);
	}else{
		vd->zoom=1;
	}
	XmToggleButtonGadgetSetState(get_menu_item(vd,"*zoomFit"), set, False);
	XmToggleButtonGadgetSetState(
		get_toolbar_item(vd->wtoolbar,"*zoomFit"), set, False);
}

static void zoom_in_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	if(vd->zoom_fit){
		XmToggleButtonGadgetSetState(get_menu_item(vd,"*zoomFit"),False,False);
		XmToggleButtonGadgetSetState(
			get_toolbar_item(vd->wtoolbar,"*zoomFit"), False, False);

		vd->zoom_fit=False;
	}
	zoom_view(vd,vd->zoom*vd->zoom_inc);
}

static void zoom_out_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	if(vd->zoom_fit){
		XmToggleButtonGadgetSetState(get_menu_item(vd,"*zoomFit"),False,False);
		XmToggleButtonGadgetSetState(
			get_toolbar_item(vd->wtoolbar,"*zoomFit"), False, False);

		vd->zoom_fit=False;
	}
	zoom_view(vd,vd->zoom/vd->zoom_inc);
}

static void zoom_reset_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	if(vd->zoom_fit){
		XmToggleButtonGadgetSetState(
			get_menu_item(vd,"*zoomFit"), False, True);
		XmToggleButtonGadgetSetState(
			get_toolbar_item(vd->wtoolbar,"*zoomFit"), False, True);
	}else{
		zoom_view(vd,1);
	}
}

static void refresh_cb(Widget w, XtPointer client, XtPointer call)
{
	char *fname;
	struct viewer_data *vd=(struct viewer_data*)client;	

	fname=strdup(vd->file_name);
	reset_viewer(vd);
	load_image(vd,fname);
	free(fname);
}

static void vflip_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	vd->tform^=IMGT_VFLIP;
	update_back_buffer(vd);
	redraw_view(vd,False);
}

static void hflip_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	vd->tform^=IMGT_HFLIP;
	update_back_buffer(vd);
	redraw_view(vd,False);
	
}

static void rotate_right_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	rotate_view(vd,True);
}

static void rotate_left_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	rotate_view(vd,False);
}

static void rotate_reset_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	vd->tform=0;
	update_back_buffer(vd);
	redraw_view(vd,True);
}

static void rotate_lock_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	vd->keep_tform=((XmToggleButtonCallbackStruct*)call)->set;
}


static void about_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	display_about_dlgbox(vd->wshell);
}

static void toolbar_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	vd->show_tbr = ((XmToggleButtonCallbackStruct*)call)->set;
	if(vd->show_tbr)
		XtManageChild(vd->wtoolbar);
	else
		XtUnmanageChild(vd->wtoolbar);

	update_controls(vd);
}

static void next_file_cb(Widget w, XtPointer client, XtPointer call)
{
	load_next_file((struct viewer_data*)client,True);
}

static void prev_file_cb(Widget w, XtPointer client, XtPointer call)
{
	load_next_file((struct viewer_data*)client,False);
}

static void next_page_cb(Widget w, XtPointer client, XtPointer call)
{
	load_next_page((struct viewer_data*)client,True);
}

static void prev_page_cb(Widget w, XtPointer client, XtPointer call)
{
	load_next_page((struct viewer_data*)client,False);
}

/*
 * Create new browser window and ask user for path to browse.
 */
static void browse_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	char *path, *home;
	Widget browser;
	
	if(!(home=getenv("HOME"))) home=".";
	path=dir_select_dlg(vd->wshell,nlstr(DLG_MSGSET,SID_OPENDIR,
		"Open Directory"),home);
	if(!path) return;
		
	browser=get_browser(&init_app_res,NULL);
	if(browser){
		browse_path(browser,path,NULL);
	}else{
		message_box(vd->wshell,MB_ERROR_NB,NULL,nlstr(APP_MSGSET,SID_ENORES,
				"Not enough resources available for this task."));
	}
	free(path);	
}

/*
 * Open a browser window in current file's location.
 */
static void browse_here_cb(Widget w, XtPointer client, XtPointer call)
{
	char *path, *token;
	struct viewer_data *vd=(struct viewer_data*)client;
	Widget browser;
	
	browser=get_browser(&init_app_res,NULL);
	if(!browser){
		message_box(vd->wshell,MB_ERROR_NB,NULL,nlstr(APP_MSGSET,SID_ENORES,
				"Not enough resources available for this task."));
		return;
	}
	path=strdup(vd->file_name);
	token=strrchr(path,'/');
	if(token){
		*token='\0';
		browse_path(browser,path,NULL);
	}else{
		free(path);
		path=realpath(".",NULL);
		browse_path(browser,path,NULL);
	}
	free(path);
}

/*
 * Create and display a new viewer window.
 */
static void new_viewer_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	struct viewer_data *new_vd;
	
	new_vd=create_viewer(&init_app_res);
	
	if(!new_vd){
		message_box(vd->wshell,MB_ERROR_NB,NULL,nlstr(APP_MSGSET,SID_ENORES,
				"Not enough resources available for this task."));
		return;
	}
	map_shell_unpositioned(new_vd->wshell);
}

/*
 * Display the file selection dialog.
 */
static void open_cb(Widget w, XtPointer client, XtPointer call)
{
	Arg arg[4];
	int i=0;

	XmString xm_path=NULL;
	struct viewer_data *vd=(struct viewer_data*)client;

	if(vd->file_name){
		char *path;
		char *token;
		path=strdup(vd->file_name);
		token=strrchr(path,'/');
		if(token){
			*token='\0';
		}else{
			free(path);
			path=realpath(".",NULL);
		}
		xm_path=XmStringCreateLocalized(path);
		free(path);
	}

	/* one time init */
	if(!vd->wfile_dlg){
		XtSetArg(arg[i],XmNfileTypeMask,XmFILE_REGULAR); i++;
		XtSetArg(arg[i],XmNresizePolicy,XmRESIZE_GROW); i++;
		XtSetArg(arg[i],XmNtitle,
			nlstr(DLG_MSGSET,SID_OPENFILE,"Open File")); i++;
		XtSetArg(arg[i],XmNdirectory,xm_path); i++;
		vd->wfile_dlg=XmCreateFileSelectionDialog(vd->wview,"fileSelect",arg,i);
		
		XtAddCallback(vd->wfile_dlg,XmNokCallback,
			file_select_cb,(XtPointer)vd);
		XtAddCallback(vd->wfile_dlg,XmNcancelCallback,
			file_select_cb,(XtPointer)vd);

		XtUnmanageChild(XmFileSelectionBoxGetChild(vd->wfile_dlg,
			XmDIALOG_HELP_BUTTON));

		XtManageChild(vd->wfile_dlg);
	}else{
		XtSetArg(arg[i],XmNdirectory,xm_path); i++;
		XtSetValues(vd->wfile_dlg,arg,i);
		XtManageChild(vd->wfile_dlg);
	}
	if(xm_path) XmStringFree(xm_path);
}

/*
 * Called from the file selection dialog.
 */
static void file_select_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	char *fname;
	XmFileSelectionBoxCallbackStruct *fscb=
		(XmFileSelectionBoxCallbackStruct*)call;

	if(fscb->reason==XmCR_CANCEL || fscb->reason==XmCR_NO_MATCH){
		XtUnmanageChild(w);
	}else{
		fname=(char*)XmStringUnparse(fscb->value,NULL,0,XmCHARSET_TEXT,
			NULL,0,XmOUTPUT_ALL);
		load_image(vd,fname);
		XtFree(fname);
		XtUnmanageChild(w);
	}
}

/*
 * File edit callback
 */
static void edit_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	char *path;
	pid_t pid;
	volatile int errv = 0;
	
	path = realpath(vd->file_name,NULL);
	if(!path){
		message_box(vd->wshell,MB_ERROR_NB,NULL,nlstr(APP_MSGSET,SID_ENORES,
			"Not enough resources available for this task."));
		return;
	}

	pid = vfork();
	
	if(pid == 0) {
		if(execlp(init_app_res.edit_cmd, init_app_res.edit_cmd,path, NULL))
			errv = errno;

		_exit(0);
	} else if(pid == (-1)) {
		errv = errno;
	}
	
	if(errv)
		errno_message_box(vd->wshell, errv, init_app_res.edit_cmd, False);

	free(path);
}

static void pass_to_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd = (struct viewer_data*)client;
	char *path;
	char *cmd;
	pid_t pid;
	volatile int errv = 0;
	
	cmd = pass_to_input_dlg(vd->wshell);
	if(!cmd) return;

	path = realpath(vd->file_name, NULL);
	if(!path){
		free(cmd);
		message_box(vd->wshell, MB_ERROR_NB, NULL, nlstr(APP_MSGSET, SID_ENORES,
			"Not enough resources available for this task."));
		return;
	}

	pid = vfork();
	
	if(pid == 0){
		if(execlp(cmd, cmd, path, NULL))
			errv = errno;

		_exit(0);
	} else if(pid == (-1)) {
		errv = errno;
	}
	
	if(errv)
		errno_message_box(vd->wshell, errv, cmd, False);
	
	free(cmd);
	free(path);
}


/*
 * File management callbacks
 */
static void copy_to_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	Boolean pinned_state=vd->pinned;
	char *dir_name;
	char *src[1];
	int ret;
	
	vd->pinned=True;
	dir_name=dir_select_dlg(vd->wshell,nlstr(DLG_MSGSET,SID_SELDESTDIR,
		"Select Destination"),vd->last_dest_dir);
	vd->pinned=pinned_state;
	
	if(!dir_name) return;
	src[0]=realpath(vd->file_name,NULL);
	ret=copy_files((char**)src,dir_name,1,NULL,NULL);
	free(src[0]);
	if(ret){
		errno_message_box(vd->wshell,ret,
			nlstr(APP_MSGSET,SID_EFAILED,
			"The action couldn't be completed."),False);
	}
	if(vd->last_dest_dir)
		free(vd->last_dest_dir);
	vd->last_dest_dir=dir_name;
}

static void move_to_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	Boolean pinned_state=vd->pinned;
	char *dir_name;
	char *src[1];
	int ret;
	
	vd->pinned=True;
	dir_name=dir_select_dlg(vd->wshell,nlstr(DLG_MSGSET,SID_SELDESTDIR,
		"Select Destination"),vd->last_dest_dir);
	vd->pinned=pinned_state;
	if(!dir_name) return;
	
	src[0]=realpath(vd->file_name,NULL);
	ret=move_files((char**)src,dir_name,1,NULL,NULL);
	free(src[0]);
	if(vd->last_dest_dir)
		free(vd->last_dest_dir);
	vd->last_dest_dir=dir_name;
	if(ret){
		errno_message_box(vd->wshell,ret,
			nlstr(APP_MSGSET,SID_EFAILED,
			"The action couldn't be completed."),False);
	}else{
		reset_viewer(vd);
	}
}

static void rename_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	char *file_path;
	char *file_title;
	char *new_title;
	char *new_name;
	int ret;
	Boolean pinned_state=vd->pinned;
	
	file_path=strdup(vd->file_name);
	file_title=strrchr(file_path,'/');
	if(file_title){
		file_title[0]='\0';
		file_title++;
	}else{
		file_path[0]='\0';
		file_title=vd->file_name;
	}
	
	vd->pinned=True;	
	new_title=rename_file_dlg(vd->wshell,file_title);
	vd->pinned=pinned_state;
	if(!new_title){
		free(file_path);
		vd->pinned=pinned_state;
		return;
	}
	new_name=malloc(strlen(file_path)+strlen(new_title)+2);
	sprintf(new_name,"%s/%s",file_path,new_title);
	ret=rename(vd->file_name,new_name);
	if(ret){
		errno_message_box(vd->wshell,ret,
			nlstr(APP_MSGSET,SID_EFAILED,
			"The action couldn't be completed."),False);
	}else{
		reset_viewer(vd);
		load_image(vd,new_name);
	}
	free(file_path);
	free(new_name);
	free(new_title);
}

static void delete_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	int ret;
	
	if(init_app_res.confirm_rm){
		Boolean pinned_state=vd->pinned;
		vd->pinned=True;
		if(MBR_CONFIRM!=message_box(vd->wshell,MB_CONFIRM,
			nlstr(DLG_MSGSET,SID_UNLINK,"Delete"),
			nlstr(APP_MSGSET,SID_CFUNLINK,
			"The file will be irrevocably deleted. Continue?"))){
			vd->pinned=pinned_state;
			return;
		}
		vd->pinned=pinned_state;
	}
	ret=unlink(vd->file_name);
	if(ret){
		errno_message_box(vd->wshell,ret,
			nlstr(APP_MSGSET,SID_EFAILED,
			"The action couldn't be completed."),False);
	}else{
		reset_viewer(vd);
	}
}

/*
 * Help/Topics menu callback. Currently it just shows the manpage.
 */
#ifdef ENABLE_CDE
static void help_topics_cb(Widget w, XtPointer client, XtPointer call)
{
	struct viewer_data *vd=(struct viewer_data*)client;
	DtActionArg args[1]={
		DtACTION_FILE,
		"ximaging"
	};
	DtActionInvoke(vd->wshell,"Dtmanpageview",args,1,
		NULL,NULL,NULL,False,NULL,NULL);
}
#endif /* ENABLE_CDE */

/*
 * Creates the menu bar widget and the pulldowns. Assigns the handle to
 * vd->wmenubar but doesn't attach it to vd->wmain yet.
 */
static void create_viewer_menubar(struct viewer_data *vd)
{
	Arg args[2];
	
	const struct menu_item file_menu[]={
		{IT_PUSH,"fileMenu","_File",SID_VMFILE,NULL},
		{IT_PUSH,"open","_Open...",SID_VMOPEN,open_cb},
		{IT_PUSH,"edit","_Edit",SID_VMFEDIT,edit_cb},
		{IT_PUSH,"passTo", "Pass _to...", SID_VMPASS, pass_to_cb},
		{IT_SEP},
		{IT_PUSH,"browse","_Browse...",SID_VMBROWSE,browse_cb},
		{IT_PUSH,"browseHere","Browse _Here",SID_VMBHERE,browse_here_cb},
		{IT_SEP},
		{IT_PUSH,"nextFile","_Next File",SID_VMNEXT,next_file_cb},
		{IT_PUSH,"previousFile","_Previous File",SID_VMPREV,prev_file_cb},
		{IT_SEP},
		{IT_PUSH,"close","_Close",SID_VMCLOSE,close_cb}
	};

	const struct menu_item edit_menu[]={
		{IT_PUSH,"editMenu","_Edit",SID_VMEDIT,NULL},
		{IT_PUSH,"copyTo","_Copy to...",SID_VMCOPYTO,copy_to_cb},
		{IT_PUSH,"moveTo","_Move to...",SID_VMMOVETO,move_to_cb},
		{IT_PUSH,"rename","_Rename...",SID_VMRENAME,rename_cb},
		{IT_SEP},
		{IT_PUSH,"delete","_Delete",SID_VMDEL,delete_cb}
	};

	const struct menu_item view_menu[]={
		{IT_PUSH,"viewMenu","_View",SID_VMVIEW},
		{IT_PUSH,"refresh","_Refresh",SID_VMREFRESH,refresh_cb},
		{IT_SEP},
		{IT_PUSH,"nextPage","_Next Page",SID_VMNEXTPAGE,next_page_cb},
		{IT_PUSH,"previousPage","Pr_evious Page",SID_VMPREVPAGE,prev_page_cb},
		{IT_SEP},
		{IT_TOGGLE,"toolbar","_Toolbar", SID_VMTOOLBAR, toolbar_cb}
	};
	
	const struct menu_item zoom_menu[]={
		{IT_PUSH,"zoomMenu","_Zoom",SID_VMZOOM},
		{IT_PUSH,"zoomIn","Zoom _In",SID_VMZOOMIN,zoom_in_cb},
		{IT_PUSH,"zoomOut","Zoom _Out",SID_VMZOOMOUT,zoom_out_cb},
		{IT_PUSH,"zoomNone","Original _Size",SID_VMNOZOOM,zoom_reset_cb},
		{IT_SEP},
		{IT_TOGGLE,"zoomFit","_Fit To Window",SID_VMZOOMFIT,zoom_fit_cb}
	};

	const struct menu_item rotate_menu[]={
		{IT_PUSH,"rotateMenu","_Rotate",SID_VMROTATE},
		{IT_PUSH,"rotateLeft","Rotate _Left",SID_VMROTATECCW,rotate_left_cb},
		{IT_PUSH,"rotateRight","Rotate _Right",SID_VMROTATECW,rotate_right_cb},
		{IT_SEP},
		{IT_PUSH,"flipVertically","Flip _Vertically",SID_VMVFLIP,vflip_cb},
		{IT_PUSH,"flipHorizontally","Flip _Horizontally",SID_VMHFLIP,hflip_cb},
		{IT_SEP},
		{IT_TOGGLE,"rotateLock","L_ock",SID_VMLOCK,rotate_lock_cb},
		{IT_PUSH,"rotateReset","R_eset",SID_VMRESET,rotate_reset_cb}
	};

	const struct menu_item window_menu[]={
		{IT_PUSH,"windowMenu","_Window",SID_VMWINDOW},
		{IT_TOGGLE,"pinThis","_Pin This",SID_VMPIN,pin_window_cb},
		{IT_PUSH,"openNew","_Open New",SID_VMNEW,new_viewer_cb}
	};


	const struct menu_item help_menu[]={
		{IT_PUSH,"helpMenu","_Help",SID_VMHELP,NULL},
		#ifdef ENABLE_CDE
		{IT_PUSH,"topics","_Manual",SID_VMTOPICS,help_topics_cb},
		#endif /* ENABLE_CDE */
		{IT_PUSH,"about","_About",SID_VMABOUT,about_cb}
	};

	XtSetArg(args[0], XmNshadowThickness, 1);	
	vd->wmenubar=XmCreateMenuBar(vd->wmain, "menuBar", args, 1);
	
	create_pulldown(vd->wmenubar,file_menu,
		(sizeof(file_menu)/sizeof(struct menu_item)),
		vd,VIEWER_MENU_MSGSET,False);
	create_pulldown(vd->wmenubar,edit_menu,
		(sizeof(edit_menu)/sizeof(struct menu_item)),
		vd,VIEWER_MENU_MSGSET,False);
	create_pulldown(vd->wmenubar,view_menu,
		(sizeof(view_menu)/sizeof(struct menu_item)),
		vd,VIEWER_MENU_MSGSET,False);
	create_pulldown(vd->wmenubar,zoom_menu,
		(sizeof(zoom_menu)/sizeof(struct menu_item)),
		vd,VIEWER_MENU_MSGSET,False);
	create_pulldown(vd->wmenubar,rotate_menu,
		(sizeof(rotate_menu)/sizeof(struct menu_item)),
		vd,VIEWER_MENU_MSGSET,False);
	create_pulldown(vd->wmenubar,window_menu,
		(sizeof(window_menu)/sizeof(struct menu_item)),
		vd,VIEWER_MENU_MSGSET,False);
	create_pulldown(vd->wmenubar,help_menu,
		(sizeof(help_menu)/sizeof(struct menu_item)),
		vd,VIEWER_MENU_MSGSET,True);

}

/*
 * Creates viewer toolbar, doesn't attach it to the main widget yet
 */
static void create_viewer_toolbar(struct viewer_data *vd)
{
	#include "bitmaps/tbbrowse.bm"
	#include "bitmaps/tbopen.bm"
	#include "bitmaps/tbprev.bm"
	#include "bitmaps/tbnext.bm"
	#include "bitmaps/tbzoomin.bm"
	#include "bitmaps/tbzoomout.bm"
	#include "bitmaps/tbzreset.bm"
	#include "bitmaps/tbhflip.bm"
	#include "bitmaps/tbvflip.bm"
	#include "bitmaps/tbrol.bm"
	#include "bitmaps/tbror.bm"
	#include "bitmaps/tbpin.bm"
	#include "bitmaps/tbzoomfit.bm"

	#include "bitmaps/tbbrowse_s.bm"
	#include "bitmaps/tbopen_s.bm"
	#include "bitmaps/tbprev_s.bm"
	#include "bitmaps/tbnext_s.bm"
	#include "bitmaps/tbzoomin_s.bm"
	#include "bitmaps/tbzoomout_s.bm"
	#include "bitmaps/tbzreset_s.bm"
	#include "bitmaps/tbhflip_s.bm"
	#include "bitmaps/tbvflip_s.bm"
	#include "bitmaps/tbrol_s.bm"
	#include "bitmaps/tbror_s.bm"
	#include "bitmaps/tbpin_s.bm"
	#include "bitmaps/tbzoomfit_s.bm"

	#define BMDATA(name) \
		vd->large_tbr?name##_bits:name##_s_bits, \
		vd->large_tbr?name##_width:name##_s_width, \
		vd->large_tbr?name##_height:name##_s_height
		
	struct toolbar_item items[]={
		{TB_PUSH,"open",BMDATA(tbopen),open_cb},
		{TB_PUSH,"browseHere",BMDATA(tbbrowse),browse_here_cb},
		{TB_SEP,NULL,NULL,0,0,NULL},
		{TB_PUSH,"previousFile",BMDATA(tbprev),prev_file_cb},
		{TB_PUSH,"nextFile",BMDATA(tbnext),next_file_cb},
		{TB_SEP,NULL,NULL,0,0,NULL},
		{TB_PUSH,"zoomIn",BMDATA(tbzoomin),zoom_in_cb},
		{TB_PUSH,"zoomNone",BMDATA(tbzreset),zoom_reset_cb},
		{TB_PUSH,"zoomOut",BMDATA(tbzoomout),zoom_out_cb},
		{TB_TOGGLE,"zoomFit",BMDATA(tbzoomfit),zoom_fit_cb},
		{TB_SEP,NULL,NULL,0,0,NULL},
		{TB_PUSH,"rotateLeft",BMDATA(tbrol),rotate_left_cb},
		{TB_PUSH,"rotateRight",BMDATA(tbror),rotate_right_cb},
		{TB_PUSH,"flipVertically",BMDATA(tbvflip),vflip_cb},
		{TB_PUSH,"flipHorizontally",BMDATA(tbhflip),hflip_cb},
		{TB_SEP,NULL,NULL,0,0,NULL},
		{TB_TOGGLE,"pinThis",BMDATA(tbpin),pin_window_cb}
	};

	#undef BMDATA
	
	vd->wtoolbar=create_toolbar(vd->wmain,items,
		(sizeof(items)/sizeof(struct toolbar_item)),(void*)vd);

}

/* Menu item widget retrieval with validation */
static Widget get_menu_item(struct viewer_data *vd, const char *name)
{
	Widget w;
	w=XtNameToWidget(vd->wmenubar,name);
	dbg_assertmsg(w!=None,"menu item %s doesn't exist\n",name);
	return w;
}

/*
 * Returns the first unpinned shell in the list, or creates a new
 * viewer if none available.
 */
Widget get_viewer(const struct app_resources *res, Tt_message req_msg)
{
	struct viewer_data *vd;

	vd=viewers;

	while(vd){
		if(!vd->pinned) break;
		vd=vd->next;
	}

	if(vd){
		if(vd->state&(ISF_OPENED|ISF_LOADING|ISF_READY))
			reset_viewer(vd);
		raise_and_focus(vd->wshell);
	}else{
		vd=create_viewer(res);
		if(vd){
			map_shell_unpositioned(vd->wshell);
		}else{
			message_box(app_inst.session_shell,MB_ERROR,BASE_NAME,
				nlstr(APP_MSGSET,SID_ENORES,
				"Not enough resources available for this task."));
			#ifdef ENABLE_CDE
			ttmedia_load_reply(req_msg,NULL,0,True);
			#endif /* ENABLE_CDE */
			return None;
		}
	}

	#ifdef ENABLE_CDE
	if(req_msg) vd->tt_disp_req=req_msg;
	#endif /* ENABLE_CDE */

	return vd->wshell;
}

/*
 * Public interface to load_image
 */
Boolean display_image(Widget wshell, const char *fname, Tt_message req_msg)
{
	struct viewer_data *vd=viewers;
	Boolean res;

	dbg_assert(fname);

	vd=get_viewer_inst_data(wshell);
	dbg_assert(vd);
	res=load_image(vd,fname);

	#ifdef ENABLE_CDE
	if(req_msg){
		if(res)
			vd->tt_disp_req=req_msg;
		else
			ttmedia_load_reply(req_msg,NULL,0,True);
	}
	#endif /* ENABLE_CDE */
	return res;
}

/*
 * Destroy viewer by a ToolTalk Quit request.
 */
#ifdef ENABLE_CDE
Boolean handle_viewer_ttquit(Tt_message req_msg)
{

	struct viewer_data *vd=viewers;
	char *orig_req_id;
	char *disp_req_id;

	dbg_assert(req_msg);
	
	orig_req_id=tt_message_arg_val(req_msg,2);
	disp_req_id=tt_message_id(vd->tt_disp_req);
	if(!strlen(orig_req_id)){
		tt_free(orig_req_id);
		tt_free(disp_req_id);
		return False;
	}
	while(vd){
		if(!strcmp(orig_req_id,disp_req_id)) break;
		vd=vd->next;
	}
	tt_free(orig_req_id);
	tt_free(disp_req_id);
	if(!vd) return False;
	
	vd->tt_quit_req=req_msg;
	destroy_viewer(vd);

	return True;
}
#endif /* ENABLE_CDE */
