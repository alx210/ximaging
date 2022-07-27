/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

/*
 * Implements the browser
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/Frame.h>
#include <Xm/DrawingA.h>
#include <Xm/LabelG.h>
#include <Xm/Form.h>
#include <Xm/Paned.h>
#include <Xm/List.h>
#include <Xm/RowColumn.h>
#include <Xm/Container.h>
#include <Xm/ScrolledW.h>
#include <Xm/ScrollBar.h>
#include <Xm/ToggleBG.h>
#include <Xm/Protocols.h>
#include <Xm/XmP.h>
#include <X11/ImUtil.h>
#include <X11/Xatom.h>
#ifdef ENABLE_CDE
#include <Tt/tt_c.h>
#include <Tt/tttk.h>
#include <Dt/Action.h>
#endif /* ENABLE_CDE */
#include "common.h"
#include "browser_i.h"
#include "menu.h"
#include "strings.h"
#include "comdlgs.h"
#include "guiutil.h"
#include "const.h"
#include "imgfile.h"
#include "imgblt.h"
#include "filemgmt.h"
#include "viewer.h"
#include "browser.h"
#include "navbar.h"
#include "bswap.h"
#include "debug.h"
#include "bitmaps/wmiconb.bm"
#include "bitmaps/wmiconb_m.bm"

/* Local prototypes */
static struct browser_data* create_browser(const struct app_resources *res);
static struct browser_data* get_browser_inst_data(Widget wshell);
static void load_path(struct browser_data *bd, const char *path);
static void destroy_browser(struct browser_data *bd);
static void parse_res_strings(const struct app_resources *res,
	struct browser_data*);
static void reset_browser(struct browser_data *bd);
static void *loader_thread(void*);
static void thread_callback_proc(XtPointer,int*,XtInputId*);
static void update_status_msg(struct browser_data*);
static void update_shell_title(struct browser_data*);
static void update_scroll_bar(struct browser_data*);
static void update_controls(struct browser_data *bd);
static Widget get_menu_item(struct browser_data *bd, const char *name);
static void view_map_cb(Widget,XtPointer,XEvent*,Boolean*);
static void close_cb(Widget,XtPointer,XtPointer);
static void create_browser_menubar(struct browser_data *bd);
static void create_tile_popup(struct browser_data *bd);
static int read_directory(struct browser_data*);
static XmString create_file_label(struct browser_data*,const char*);
static int scanline_read_cb(unsigned long,const uint8_t*,void*);
static float compute_scaling_factor(XImage *src, XImage *dest);
static void clear_selection(struct browser_data *bd);
static void set_selection(struct browser_data*,long,long,Boolean);
static void toggle_selection(struct browser_data*,long);
static void set_focus(struct browser_data*,long);
static Boolean select_tile_at(struct browser_data*,int,int,int);
static void scroll_to(struct browser_data*,long);
static void dblclk_timer_cb(XtPointer,XtIntervalId*);
static void set_vscroll(struct browser_data*,int);
static void compute_tile_dimensions(struct browser_data*,
	long*,long*,unsigned int*,unsigned int*,unsigned int*,unsigned int*);
static void compute_tile_position(struct browser_data*,
	long,int*,int*,unsigned int*,unsigned int*,Boolean*);
static long find_file_entry(const struct browser_data*,const char*);
static int file_sort_compare(const void*, const void*);
static int dir_sort_compare(const void*, const void*);
static Pixmap get_state_pixmap(struct browser_data *bd, enum file_state state,
	Boolean selected,Dimension *width, Dimension *height);
static void invoke_default_action(struct browser_data*, long);
static int exec_file_proc(struct browser_data *bd, int proc);
static void file_proc_cb(enum file_proc_id,char*,int,void*);
static int launch_reader_thread(struct browser_data *bd);
static int launch_loader_thread(struct browser_data*);
static void update_interval_cb(XtPointer,XtIntervalId*);
static void set_tile_size(struct browser_data*,enum tile_preset);
static void edit_cb(Widget,XtPointer,XtPointer);
static void scroll_cb(Widget,XtPointer,XtPointer);
static void input_cb(Widget,XtPointer,XtPointer);
static void expose_cb(Widget,XtPointer,XtPointer);
static void resize_cb(Widget,XtPointer,XtPointer);
static void copy_to_cb(Widget,XtPointer,XtPointer);
static void move_to_cb(Widget,XtPointer,XtPointer);
static void rename_cb(Widget,XtPointer,XtPointer);
static void delete_cb(Widget,XtPointer,XtPointer);
static void small_tiles_cb(Widget,XtPointer,XtPointer);
static void medium_tiles_cb(Widget,XtPointer,XtPointer);
static void large_tiles_cb(Widget,XtPointer,XtPointer);
static void show_subdirs_cb(Widget,XtPointer,XtPointer);
static void show_dotfiles_cb(Widget,XtPointer,XtPointer);
static void pin_window_cb(Widget,XtPointer,XtPointer);
static void new_window_cb(Widget,XtPointer,XtPointer);
static void navbar_change_cb(const char*, void*);
static void about_cb(Widget,XtPointer,XtPointer);
static void dirlist_cb(Widget,XtPointer,XtPointer);
static Boolean convert_selection_proc(Widget w,
	Atom*, Atom*, Atom*, XtPointer*, unsigned long*, int*);
static void lose_selection_proc(Widget, Atom*);
static void redraw_tile(struct browser_data*, long);
#ifdef ENABLE_CDE
static void help_topics_cb(Widget,XtPointer,XtPointer);
#endif /* ENABLE_CDE */

/* Linked list of browser instances */
struct browser_data *browsers=NULL;

/*
 * Create a browser widget and link it in
 */
static struct browser_data* create_browser(const struct app_resources *res)
{
	static XtTranslations view_tt=NULL;
	Colormap colormap;
	Widget wmsgbar,wframe,wview_scrl;
	struct browser_data *bd;
	Dimension line_width=0;
	XGCValues gc_values;
	static Pixmap wmicon=0;
	static Pixmap wmicon_mask=0;

	bd=calloc(1,sizeof(struct browser_data));
	if(!bd) return NULL;
	bd->ifocus=(-1);
	
	if(pipe(bd->tnfd)){
		free(bd);
		return NULL;
	}

	parse_res_strings(res,bd);
	
	bd->wshell=XtVaAppCreateShell("ximagingBrowser",APP_CLASS "Browser",
		applicationShellWidgetClass,app_inst.display,
		XmNvisual,app_inst.visual_info.visual,
		XmNcolormap,app_inst.colormap,
		XmNgeometry,res->geometry,
		XmNmappedWhenManaged,False,
		XmNdeleteResponse,XmUNMAP,NULL);

	XtVaGetValues(bd->wshell,XmNtitle,&bd->title,NULL);
	bd->title=strdup(bd->title);
	XmAddWMProtocolCallback(bd->wshell,app_inst.XaWM_DELETE_WINDOW,
		close_cb,(XtPointer)bd);
		
	bd->wmain=XmVaCreateMainWindow(bd->wshell,"main",NULL);
	create_browser_menubar(bd);

	XtVaGetValues(bd->wmain,XmNcolormap,&colormap,
		XmNbackground,&bd->bg_pixel,NULL);
	XmGetColors(XtScreen(bd->wshell),colormap,bd->bg_pixel,
		&bd->fg_pixel,&bd->ts_pixel,&bd->bs_pixel,&bd->sbg_pixel);
			
	XtVaGetValues(XmGetXmDisplay(app_inst.display),
		XmNenableThinThickness,&line_width,NULL);
	line_width=(line_width ? 1:2);
	bd->border_width=line_width;
	
	wframe = XmVaCreateManagedFrame(bd->wmain, "frame",
		XmNmarginWidth, 0, XmNmarginHeight, 0,
		XmNshadowType, XmSHADOW_OUT,
		XmNshadowThickness, 1, NULL);

	bd->wpaned = XtVaCreateManagedWidget("paned",
		xmPanedWidgetClass, wframe,
		XmNorientation, XmHORIZONTAL,
		XmNmarginWidth, 2, XmNmarginHeight, 2,
		XmNshadowThickness, 1, NULL);
	
	bd->wdlscroll = XmVaCreateScrolledWindow(bd->wpaned,
		"listScrolled", XmNshadowThickness, 0,
		XmNallowResize, True, NULL);
	bd->wdirlist = XmVaCreateManagedList(bd->wdlscroll, "directoriesList",
		XmNprimaryOwnership, XmOWN_NEVER,
		XmNselectionPolicy, XmBROWSE_SELECT,
		XmNautomaticSelection, XmNO_AUTO_SELECT, NULL);
	XtAddCallback(bd->wdirlist,
		XmNdefaultActionCallback, dirlist_cb, (XtPointer)bd);
	
	if(res->show_dirs) XtManageChild(bd->wdlscroll);
	
	wview_scrl = XmVaCreateManagedScrolledWindow(bd->wpaned, "viewScrolled",
		XmNshadowType, XmSHADOW_IN, XmNshadowThickness, 1,
		XmNscrollingPolicy, XmAPPLICATION_DEFINED,
		XmNvisualPolicy, XmVARIABLE,
		XmNpaneMaximum, 65535, NULL);
	bd->wvscroll=XmVaCreateManagedScrollBar(wview_scrl,"vscroll",
		XmNminimum,0,XmNmaximum,1,XmNincrement,8,XmNpageIncrement,32,NULL);
	XtAddCallback(bd->wvscroll,XmNdragCallback,scroll_cb,(XtPointer)bd);
	XtAddCallback(bd->wvscroll,XmNvalueChangedCallback,
		scroll_cb,(XtPointer)bd);
	
	bd->wview=XmVaCreateManagedDrawingArea(wview_scrl,"view",
		XmNbackground,bd->bg_pixel,XmNuserData,(XtPointer)bd,NULL);
	XtAddEventHandler(bd->wview,StructureNotifyMask,
		False,view_map_cb,(XtPointer)bd);
	if(!view_tt){
		view_tt=XtParseTranslationTable(
			"<Btn1Down>:DrawingAreaInput() ManagerGadgetArm()\n"
			"<Btn1Up>:DrawingAreaInput() ManagerGadgetActivate()\n"
			"<Key>osfActivate:DrawingAreaInput()\n"
			"<Key>osfUp:DrawingAreaInput()\n"
			"<Key>osfDown:DrawingAreaInput()\n"
			"<Key>osfLeft:DrawingAreaInput()\n"
			"<Key>osfRight:DrawingAreaInput()\n"
			"<Key>osfCancel:DrawingAreaInput()\n"
			"s ~m ~a <Key>Tab:DrawingAreaInput()\n"
			"~m ~a <Key>Tab:DrawingAreaInput()\n");
	}
	XtOverrideTranslations(bd->wview,view_tt);
	XtVaSetValues(wview_scrl,XmNverticalScrollBar,bd->wvscroll,
		XmNworkWindow,bd->wview,NULL);
		
	wmsgbar=XmVaCreateManagedFrame(bd->wmain,"messageArea",
		XmNshadowType,XmSHADOW_OUT,
		XmNshadowThickness, 1,NULL);
	
	bd->wmsg=XmVaCreateManagedLabelGadget(wmsgbar,"status",
		XmNmarginHeight,4,XmNalignment,XmALIGNMENT_BEGINNING,NULL);

	XtVaGetValues(bd->wmsg,XmNrenderTable,&bd->render_table,NULL);
	
	bd->wnavbar=create_navbar(bd->wmain,navbar_change_cb,bd);
	
	XtVaSetValues(bd->wmain,XmNmenuBar,bd->wmenubar,
		XmNworkWindow,wframe,XmNmessageWindow,wmsgbar,
		XmNcommandWindow,bd->wnavbar,
		XmNcommandWindowLocation,XmCOMMAND_ABOVE_WORKSPACE,NULL);

	create_tile_popup(bd);
	
	XtManageChild(bd->wnavbar);
	XtManageChild(bd->wmenubar);
	XtManageChild(bd->wmain);
	XtRealizeWidget(bd->wshell);

	XtAddCallback(bd->wview,XmNinputCallback,input_cb,(XtPointer)bd);
	XtAddCallback(bd->wview,XmNexposeCallback,expose_cb,(XtPointer)bd);
	XtAddCallback(bd->wview,XmNresizeCallback,resize_cb,(XtPointer)bd);

	if(!wmicon){
		load_icon(wmiconb_bits,wmiconb_m_bits,
			wmiconb_width,wmiconb_height,&wmicon,&wmicon_mask);
	}
	XtVaSetValues(bd->wshell,XmNiconPixmap,wmicon,
		XmNiconMask,wmicon_mask,NULL);

	memset(&gc_values,0,sizeof(XGCValues));
	gc_values.function=GXcopy;
	gc_values.foreground=bd->fg_pixel;
	gc_values.background=bd->sbg_pixel;
	gc_values.line_width=line_width;
	gc_values.line_style=LineSolid;
	gc_values.join_style=JoinMiter;
	gc_values.dashes=2;
	gc_values.dash_offset=1;
	gc_values.graphics_exposures=False;
	gc_values.plane_mask=AllPlanes;
	bd->draw_gc=XCreateGC(app_inst.display,XtWindow(bd->wshell),GCFunction|
		GCForeground|GCBackground|GCLineStyle|GCLineWidth|GCJoinStyle|
		GCGraphicsExposures|GCPlaneMask|GCDashList|GCDashOffset,
		&gc_values);
	bd->text_gc=XCreateGC(app_inst.display,XtWindow(bd->wshell),GCFunction|
		GCForeground|GCBackground|GCPlaneMask,&gc_values);

	if(pthread_mutex_init(&bd->data_mutex,NULL) ||
		pthread_mutex_init(&bd->rdr_cond_mutex,NULL) ||
		pthread_cond_init(&bd->rdr_cond,NULL) ||
		pthread_mutex_init(&bd->ldr_cond_mutex,NULL) ||
		pthread_cond_init(&bd->ldr_cond,NULL)){
		
		pthread_mutex_destroy(&bd->data_mutex);
		pthread_mutex_destroy(&bd->rdr_cond_mutex);
		pthread_cond_destroy(&bd->rdr_cond);
		pthread_mutex_destroy(&bd->ldr_cond_mutex);
		pthread_cond_destroy(&bd->ldr_cond);
		return NULL;
	}
	bd->thread_notify_input=XtAppAddInput(app_inst.context,
		bd->tnfd[TNFD_IN],(void*)XtInputReadMask,
		thread_callback_proc,(XtPointer)bd);

	XmToggleButtonGadgetSetState(
		get_menu_item(bd,"*dotFiles"), res->show_dot_files, False);
	XmToggleButtonGadgetSetState(
		get_menu_item(bd,"*directories"), res->show_dirs, False);
	XmToggleButtonGadgetSetState(
		get_menu_item(bd,"*pinThis"), res->pin_window, False);
	
	bd->pinned = res->pin_window;
	bd->show_dot_files = res->show_dot_files;
	
	set_tile_size(bd,bd->itile_size);
	update_shell_title(bd);
	update_status_msg(bd);
	update_controls(bd);
	
	/* link in the instance */
	bd->next=browsers;
	browsers=bd;
	app_inst.active_shells++;

	map_shell_unpositioned(bd->wshell);
	return bd;
}

/*
 * Load images in 'path'
 */
static void load_path(struct browser_data *bd, const char *path)
{
	char *ptr;
	
	if((bd->state&(BSF_READING|BSF_LOADING|BSF_READY))){
		reset_browser(bd);
	}
	
	dbg_assert(path);
	
	if(access(path,R_OK|X_OK)){
		errno_message_box(bd->wshell,errno,nlstr(APP_MSGSET,SID_EFAILED,
			"The action couldn't be completed."),False);
		return;
	}
	
	bd->path=realpath(path,NULL);
	
	ptr=&bd->path[strlen(bd->path)-1];
	if(*ptr=='/' && bd->path!=ptr) *ptr='\0';
	update_status_msg(bd);
	update_shell_title(bd);
	set_navbar_path(bd->wnavbar,path);
	launch_reader_thread(bd);
	XmProcessTraversal(bd->wview,XmTRAVERSE_CURRENT);
}

/*
 * Find and return browser instance data for 'wshell'.
 * Returns NULL on failure.
 */
static struct browser_data* get_browser_inst_data(Widget wshell)
{
	struct browser_data *bd=browsers;

	while(bd){
		if(bd->wshell==wshell) break;
		bd=bd->next;
	}
	return bd;
}

/*
 * Scanline read callback proc
 */
static int scanline_read_cb(unsigned long iscl,
	const uint8_t *data, void *client)
{
	struct loader_cb_data *cbd=(struct loader_cb_data*)client;
	uint8_t *ptr=(uint8_t*)&cbd->buf_image->data[iscl*
		cbd->buf_image->bytes_per_line];

	dbg_assert(iscl < cbd->img_file.height);
	
	if(app_inst.visual_info.class==PseudoColor){
		if(cbd->img_file.format==IMG_PSEUDO){
			remap_pixels(ptr,data,cbd->clut,cbd->img_file.width);
		}else{
			rgb_pixels_to_clut(ptr,data,&cbd->image_pf,cbd->img_file.width);
		}
	}else{
		if(cbd->img_file.format==IMG_PSEUDO){
			clut_to_rgb_pixels(ptr,&cbd->display_pf,data,
				cbd->clut,cbd->img_file.width);
		}else{
			convert_rgb_pixels(ptr,&cbd->display_pf,data,
				&cbd->image_pf,cbd->img_file.width);
		}
	}

	if(cbd->bd->state&BSF_LCANCEL) return IMG_READ_CANCEL;

	return IMG_READ_CONT;
}

/*
 * Compute the scaling factor for src > dest
 */
static float compute_scaling_factor(XImage *src, XImage *dest)
{
	float xs, ys;
	xs=(float)dest->width/(float)src->width;
	ys=(float)dest->height/(float)src->height;
	xs=(xs>=1.0)?1.0:xs;
	ys=(ys>=1.0)?1.0:ys;
	return (xs<ys)?xs:ys;
}

/*
 * Image loader thread entry point.
 * Load images for tiles whose FS_PENDING state is true.
 * No GUI related routines should be ever invoked from here.
 */
static void* loader_thread(void *data)
{
	struct browser_data *bd=(struct browser_data*)data;
	long i;
	unsigned int tile_width;
	unsigned int tile_height;
	struct loader_cb_data cbd;
	struct thread_msg tmsg;
	char *tmp_buf=NULL;
	size_t tmp_buf_size=0;
	char *path_buf;
	short transform;
	float scale;
	int result=0;
	
	cbd.bd=bd;

	if(app_inst.visual_info.depth>8){
		init_pixel_format(&cbd.display_pf,app_inst.pixel_size,
			app_inst.visual_info.red_mask,app_inst.visual_info.green_mask,
			app_inst.visual_info.blue_mask,0,0,IMGF_BGCOLOR);
	}

	pthread_mutex_lock(&bd->data_mutex);
	
	tile_width=bd->tile_size[bd->itile_size]-
		((TILE_PADDING*2)+bd->border_width*2);
	tile_height=(bd->tile_size[bd->itile_size]/bd->tile_asr[0])*
		bd->tile_asr[1]-((TILE_PADDING*2)+bd->border_width*2);

	path_buf=malloc(bd->path_max+1);
	cbd.buf_image=XCreateImage(app_inst.display,app_inst.visual_info.visual,
		app_inst.visual_info.depth,ZPixmap,0,NULL,1,1,app_inst.pixel_size,0);

	if(!path_buf || !cbd.buf_image)	goto exit_thread;

	cbd.buf_image->bitmap_bit_order=cbd.buf_image->byte_order=
		(is_big_endian())?MSBFirst:LSBFirst;
	_XInitImageFuncPtrs(cbd.buf_image);

	tmsg.code=TMSG_UPDATE;

	for(i=0; i<bd->nfiles && !(bd->state&BSF_LCANCEL); result=0, i++){
		size_t cur_data_size;
		if(bd->files[i].state!=FS_PENDING) continue;
		tmsg.update_data.index=i;
		
		if(!bd->files[i].image){
			char *pdata;
			pdata=malloc(app_inst.pixel_size*(tile_width*tile_height));
			
			bd->files[i].image=XCreateImage(app_inst.display,
				app_inst.visual_info.visual,
				app_inst.visual_info.depth,ZPixmap,0,NULL,
				tile_width,tile_height,app_inst.pixel_size,0);
			if(bd->files[i].image && pdata){
				bd->files[i].image->data=pdata;
				
				bd->files[i].image->bitmap_bit_order=
					bd->files[i].image->byte_order=
					(is_big_endian())?MSBFirst:LSBFirst;
				_XInitImageFuncPtrs(bd->files[i].image);
			
			}else{
				if(pdata) free(pdata);
				if(bd->files[i].image) XDestroyImage(bd->files[i].image);
				result=ENOMEM;
				break;
			}
		}
		snprintf(path_buf,bd->path_max,"%s/%s",bd->path,bd->files[i].name);
		result=img_open(path_buf,&cbd.img_file,0);

		if(result){
			dbg_trace("%s: img_open failed with %d\n",path_buf,result);
			bd->files[i].loader_result=result;
			if(result==IMG_ENOMEM)
				bd->files[i].state=FS_ERROR;
			else
				bd->files[i].state=FS_BROKEN;
			write(bd->tnfd[TNFD_OUT],&tmsg,sizeof(struct thread_msg));
			continue;
		}

		if((result=init_pixel_format(&cbd.image_pf,cbd.img_file.bpp,
			cbd.img_file.red_mask,cbd.img_file.green_mask,
			cbd.img_file.blue_mask,cbd.img_file.alpha_mask,
			cbd.img_file.bg_pixel,cbd.img_file.flags))){
			img_close(&cbd.img_file);
			dbg_trace("%s: init_pixel_format failed with %d\n",
				path_buf,result);
			bd->files[i].state=FS_ERROR;
			write(bd->tnfd[TNFD_OUT],&tmsg,sizeof(struct thread_msg));
			continue;
		}
			
		cur_data_size=((cbd.img_file.width*cbd.img_file.height)*
			app_inst.pixel_size);
		if(cur_data_size>tmp_buf_size){
			char *new_ptr;
			new_ptr=realloc(tmp_buf,cur_data_size);
			if(!new_ptr){
				img_close(&cbd.img_file);
				result=ENOMEM;
				break;
			}
			tmp_buf=new_ptr;
			tmp_buf_size=cur_data_size;
		}
		cbd.buf_image->width=cbd.img_file.width;
		cbd.buf_image->height=cbd.img_file.height;
		cbd.buf_image->bytes_per_line=0;
		cbd.buf_image->data=tmp_buf;
		XInitImage(cbd.buf_image);

		bd->files[i].xres=cbd.img_file.width;
		bd->files[i].yres=cbd.img_file.height;
		bd->files[i].bpp=cbd.img_file.orig_bpp;
		bd->files[i].time=cbd.img_file.cr_time;
		
		if(cbd.img_file.format==IMG_PSEUDO){
			if((result=img_read_cmap(&cbd.img_file,cbd.clut))){
				dbg_trace("%s: read_cmap failed with %d\n",
					path_buf,result);
				img_close(&cbd.img_file);
				bd->files[i].state=FS_BROKEN;
				write(bd->tnfd[TNFD_OUT],&tmsg,sizeof(struct thread_msg));
				continue;
			}
		}
		result=img_read_scanlines(&cbd.img_file,scanline_read_cb,(void*)&cbd);
		transform=cbd.img_file.tform;
		
		scale=compute_scaling_factor(cbd.buf_image,bd->files[i].image);
		bd->files[i].image->width = cbd.img_file.width * scale;
		bd->files[i].image->height = cbd.img_file.height * scale;
		if(!bd->files[i].image->width) bd->files[i].image->width = 1;
		if(!bd->files[i].image->height) bd->files[i].image->height = 1;
		bd->files[i].image->bytes_per_line=0;
		XInitImage(bd->files[i].image);
		
		img_close(&cbd.img_file);
		if(result==0){
			img_blt(cbd.buf_image,0,0,cbd.buf_image->width,
				cbd.buf_image->height,bd->files[i].image,
				scale,transform,BLTF_INTERPOLATE);
			bd->files[i].state=FS_VIEWABLE;
		}else{
			dbg_trace("%s: read_scanlines failed with %d\n",
				path_buf,result);
			if(result==IMG_ENOMEM){
				result=ENOMEM;
				break;
			}else{
				bd->files[i].state=FS_ERROR;
			}
		}
		bd->files[i].loader_result=result;
		write(bd->tnfd[TNFD_OUT],&tmsg,sizeof(struct thread_msg));
	}
	
	/* always go there to finish the thread */
	exit_thread:
	
	pthread_mutex_unlock(&bd->data_mutex);
	
	if(cbd.buf_image) XDestroyImage(cbd.buf_image);
	if(path_buf) free(path_buf);

	pthread_mutex_lock(&bd->ldr_cond_mutex);
	if(bd->state&BSF_LCANCEL)
		tmsg.code=TMSG_CANCELLED;
	else
		tmsg.code=TMSG_FINISHED;
	tmsg.notify_data.status=result;
	bd->state&=(~(BSF_LOADING|BSF_LCANCEL));
	pthread_cond_signal(&bd->ldr_cond);
	pthread_mutex_unlock(&bd->ldr_cond_mutex);
	
	write(bd->tnfd[TNFD_OUT],&tmsg,sizeof(struct thread_msg));
	return NULL;
}

/*
 * Find file entry by file name.
 * This routine is invoked by loader threads, so no GUI related
 * routines should be called from here.
 */
static long find_file_entry(const struct browser_data *bd, const char *name)
{
	long i;
	for(i=0; i<bd->nfiles; i++){
		if(!strcmp(bd->files[i].name,name))	return i;
	}
	return -1;
}

/*
 * Reads the current directory and puts/updates entries in 'bd'.
 * This routine is invoked by loader threads, so no GUI related
 * routines should be called from here.
 */
static int read_directory(struct browser_data *bd)
{
	struct dirent *dir_ent;
	DIR *dir;
	size_t path_len;
	char *path_buf=NULL;
	size_t path_buf_size=0;
	char **new_files=NULL;
	long new_files_size=0;
	long n_new_files=0;
	char **new_dirs = NULL;
	long new_dirs_size = 0;
	long n_new_dirs = 0;
	struct stat st;
	struct thread_msg tmsg;
	time_t latest_mt=bd->latest_file_mt;
	int res = 0;
		
	dir=opendir(bd->path);
	if(!dir) return errno;

	while((dir_ent = readdir(dir)) && !(bd->state & BSF_RCANCEL)){
		
		if(!strcmp(dir_ent->d_name, ".")) continue;
		
		path_len=strlen(bd->path)+strlen(dir_ent->d_name)+2;

		if(path_len > path_buf_size){
			char *new_ptr;
			new_ptr=realloc(path_buf,path_len);
			if(!new_ptr){
				res = ENOMEM;
				break;
			}
			path_buf=new_ptr;
			path_buf_size=path_len;
		}
		
		sprintf(path_buf,"%s/%s",bd->path,dir_ent->d_name);
		
		if(stat(path_buf, &st)) continue;
		
		if(S_ISDIR(st.st_mode)) {
			long i;
			
			if(!bd->show_dot_files && dir_ent->d_name[0] == '.' &&
				strcmp(dir_ent->d_name, "..")) continue;
			
			for(i = 0; i < bd->nsubdirs; i++) {
				if(!strcmp(bd->subdirs[i], dir_ent->d_name)) break;
			}
			if(i < bd->nsubdirs) continue;
			
			if((n_new_dirs + 1) > new_dirs_size) {
				char **ptr;
				
				ptr = realloc(new_dirs, sizeof(char*) *
					(n_new_dirs + FILE_LIST_GROWBY));
				if(!ptr) {
					res = ENOMEM;
					break;
				}
				new_dirs = ptr;
				new_dirs_size += FILE_LIST_GROWBY;
			}
			new_dirs[n_new_dirs] = strdup(dir_ent->d_name);
			if(!new_dirs[n_new_dirs]) {
				res = ENOMEM;
				break;
			}
			n_new_dirs++;
			
		} else if(S_ISREG(st.st_mode)) {
			if(!bd->show_dot_files && dir_ent->d_name[0] == '.') continue;
			if(!img_get_format_info(path_buf) ||
				(bd->nfiles && (find_file_entry(bd,dir_ent->d_name)>=0)))
				continue;

			if(difftime(st.st_mtime,latest_mt)>0) latest_mt=st.st_mtime;

			if(n_new_files+1>new_files_size){
				char **new_ptr;
				new_ptr=realloc(new_files,
					sizeof(char*)*(n_new_files+FILE_LIST_GROWBY));
				if(!new_ptr){
					res = ENOMEM;
					break;
				}
				new_files=new_ptr;
				new_files_size=n_new_files+FILE_LIST_GROWBY;
			}
			new_files[n_new_files] = strdup(dir_ent->d_name);
			if(!new_files[n_new_files]) {
				res = ENOMEM;
				break;
			}
			n_new_files++;
		}
	}
	
	closedir(dir);
	if(path_buf) free(path_buf);
	
	if(res || (bd->state&BSF_RCANCEL)) {
		if(n_new_files){
			while(n_new_files--) free(new_files[n_new_files]);
			free(new_files);
		}
		if(n_new_dirs) {
			while(n_new_dirs--) free(new_dirs[n_new_dirs]);
			free(new_dirs);
		}
		return res;
	} else if(n_new_files || n_new_dirs){
		tmsg.code = TMSG_ADD;
		tmsg.change_data.files = new_files;
		tmsg.change_data.nfiles = n_new_files;
		tmsg.change_data.dirs = new_dirs;
		tmsg.change_data.ndirs = n_new_dirs;
		
		pthread_mutex_lock(&bd->data_mutex);
		bd->path_max = path_buf_size;
		bd->latest_file_mt = latest_mt;
		pthread_mutex_unlock(&bd->data_mutex);

		/* message data is freed by the handler */
		write(bd->tnfd[TNFD_OUT],&tmsg,sizeof(struct thread_msg));
	}
	return 0;
}

/*
 * Directory reader thread entry point.
 * No GUI related routines should be ever invoked from here.
 */
static void *reader_thread(void *data)
{
	struct browser_data *bd=(struct browser_data*)data;
	struct thread_msg tmsg;
	struct stat st;
	int result=0;

	if(stat(bd->path,&st)!=0){
		result=errno;
		goto exit_thread;
	}
	
	if(bd->nfiles){
		unsigned long i;
		char *path_buf=NULL;
		char **rem_files=NULL;
		long rem_files_size=0;
		long nrem_files=0;
		char **rem_dirs = NULL;
		long nrem_dirs = 0;
		long rem_dirs_size = 0;
		Boolean modified=False;
		path_buf=malloc(bd->path_max+1);

		pthread_mutex_lock(&bd->data_mutex);
		/* check for files that may have changed */
		for(i=0; i<bd->nfiles && !(bd->state&BSF_RCANCEL); i++){
			snprintf(path_buf,bd->path_max,"%s/%s",
				bd->path,bd->files[i].name);
			if(stat(path_buf,&st)!=0){
				if(nrem_files+1>rem_files_size){
					char **new_ptr;
					new_ptr=realloc(rem_files,rem_files_size+FILE_LIST_GROWBY);
					if(!new_ptr){
						result = ENOMEM;
						break;
					}
					rem_files=new_ptr;
					rem_files_size+=FILE_LIST_GROWBY;
				}
				rem_files[nrem_files] = strdup(bd->files[i].name);
				if(!rem_files[nrem_files]) {
					result = ENOMEM;
					break;
				}
				nrem_files++;
			}else if(st.st_mtime>bd->latest_file_mt){
				bd->latest_file_mt=st.st_mtime;
				bd->files[i].state=FS_PENDING;

				tmsg.code=TMSG_UPDATE;
				tmsg.update_data.index=i;
				write(bd->tnfd[TNFD_OUT],&tmsg,sizeof(struct thread_msg));
				modified=True;
			}
		}
		
		for(i = 0; (i < bd->nsubdirs) &&
			!result && !(bd->state&BSF_RCANCEL); i++) {
			
			snprintf(path_buf, bd->path_max, "%s/%s", bd->path, bd->subdirs[i]);
			if(stat(path_buf, &st) != 0){
				if((nrem_dirs + 1) > rem_dirs_size){
					char **new_ptr;
					new_ptr = realloc(rem_dirs,
						rem_dirs_size + FILE_LIST_GROWBY);
					if(!new_ptr){
						result = ENOMEM;
						break;
					}
					rem_dirs = new_ptr;
					rem_dirs_size += FILE_LIST_GROWBY;
				}
				rem_dirs[nrem_dirs] = strdup(bd->subdirs[i]);
				if(!rem_dirs[nrem_dirs]) {
					result = ENOMEM;
					break;
				}
				nrem_dirs++;
			}
		}

		free(path_buf);
		pthread_mutex_unlock(&bd->data_mutex);

		if(!result && !(bd->state & BSF_RCANCEL)) {		
			if(nrem_files || nrem_dirs){
				tmsg.code = TMSG_REMOVE;
				tmsg.change_data.files = rem_files;
				tmsg.change_data.nfiles = nrem_files;
				tmsg.change_data.dirs = rem_dirs;
				tmsg.change_data.ndirs = nrem_dirs;
				/* message data storage is freed by the handler */
				write(bd->tnfd[TNFD_OUT],&tmsg,sizeof(struct thread_msg));
			}
			if(!stat(bd->path,&st) && difftime(st.st_mtime,bd->dir_modtime)){
				result = read_directory(bd);
				bd->dir_modtime=st.st_mtime;
			}
			if(modified) {
				tmsg.code = TMSG_RELOAD;
				tmsg.notify_data.reason = 0;
				tmsg.notify_data.status = 0;
				write(bd->tnfd[TNFD_OUT], &tmsg, sizeof(struct thread_msg));
			}
		} else {
			if(rem_files) {
				while(nrem_files--) free(rem_files[nrem_files]);
				free(rem_files);
			}
			if(rem_dirs) {
				while(nrem_dirs--) free(rem_dirs[nrem_dirs]);
				free(rem_dirs);
			}
		}

	}else{
		result = read_directory(bd);
		bd->dir_modtime = st.st_mtime;
	}
	
	/* always go there to finish the thread */
	exit_thread:

	pthread_mutex_lock(&bd->rdr_cond_mutex);

	if(bd->state&BSF_RCANCEL)
		tmsg.code=TMSG_CANCELLED;
	else
		tmsg.code=TMSG_FINISHED;
	tmsg.notify_data.reason=0;
	tmsg.notify_data.status=result;
	bd->state&=(~(BSF_READING|BSF_RCANCEL));
	pthread_cond_signal(&bd->rdr_cond);
	pthread_mutex_unlock(&bd->rdr_cond_mutex);
	write(bd->tnfd[TNFD_OUT],&tmsg,sizeof(struct thread_msg));
	return NULL;
}

/*
 * Directory reader thread callback
 */
static void thread_callback_proc(XtPointer data, int *pfd, XtInputId *iid)
{
	struct browser_data *bd=(struct browser_data*)data;
	struct thread_msg msg = {0};

	if(read(bd->tnfd[TNFD_IN],&msg,sizeof(struct thread_msg))<1) return;

	/* if cancelled state, discard stale message and return */
	if(bd->state&BSF_RESET){
		if(msg.code==TMSG_ADD || msg.code==TMSG_REMOVE){
			while(msg.change_data.nfiles--)
				free(msg.change_data.files[msg.change_data.nfiles]);
			free(msg.change_data.files);
			while(msg.change_data.ndirs--)
				free(msg.change_data.dirs[msg.change_data.ndirs]);
			free(msg.change_data.dirs);
		}
		return;
	}
	
	/* process message */
	switch(msg.code){
		case TMSG_ADD:{
			struct browser_file *new_files = NULL;
			char **new_dirs = NULL;
			long i;
			
			pthread_mutex_lock(&bd->data_mutex);
			
			if(msg.change_data.nfiles) {
				new_files = realloc(bd->files, sizeof(struct browser_file) *
					(bd->nfiles + msg.change_data.nfiles));
			}
			if(msg.change_data.ndirs) {
				new_dirs = realloc(bd->subdirs, sizeof(char*) *
					(bd->nsubdirs + msg.change_data.ndirs));
			}

			if((msg.change_data.nfiles && !new_files) ||
				(msg.change_data.ndirs && !new_dirs)){
				pthread_mutex_unlock(&bd->data_mutex);

				errno_message_box(bd->wshell,ENOMEM,
					nlstr(APP_MSGSET,SID_EREADDIR,
					"Error reading directory."),False);

				if(msg.change_data.nfiles) {
					i = msg.change_data.nfiles;
					while(i--) free(msg.change_data.files[i]);
					free(msg.change_data.files);
				}

				if(msg.change_data.ndirs) {
					i = msg.change_data.ndirs;
					while(i--) free(msg.change_data.dirs[i]);
					free(msg.change_data.dirs);
				}

				reset_browser(bd);
				return;
			}

			if(msg.change_data.nfiles) {
				for(i=0; i<msg.change_data.nfiles; i++){
					XmString label;
					long di=i+bd->nfiles;

					new_files[di].selected=False;
					new_files[di].image=NULL;
					new_files[di].state=FS_PENDING;
					new_files[di].name=strdup(msg.change_data.files[i]);
					label=create_file_label(bd,new_files[di].name);
					new_files[di].label_width=XmStringWidth(
						bd->render_table,label);
					new_files[di].label=label;			
					free(msg.change_data.files[i]);
				}
				free(msg.change_data.files);
				bd->nfiles+=msg.change_data.nfiles;
				bd->files=new_files;
				qsort(bd->files,bd->nfiles,sizeof(struct browser_file),
					file_sort_compare);
			}
			
			if(msg.change_data.ndirs) {
				for(i = 0; i < msg.change_data.ndirs; i++) {
					long di = i + bd->nsubdirs;
					new_dirs[di] = msg.change_data.dirs[i];
				}
				free(msg.change_data.dirs);
			
				bd->nsubdirs += msg.change_data.ndirs;
				bd->subdirs = new_dirs;
				qsort(bd->subdirs, bd->nsubdirs,
					sizeof(char*), dir_sort_compare);
			
				XmListDeleteAllItems(bd->wdirlist);
				for(i = 0; i < bd->nsubdirs; i++) {
					XmString str = XmStringCreateLocalized(bd->subdirs[i]);
					XmListAddItem(bd->wdirlist, str, i + 1);
					XmStringFree(str);
				}
				XClearArea(XtDisplay(bd->wdirlist),
					XtWindow(bd->wdirlist),	0, 0, 0, 0, True);
			}
			
			pthread_mutex_unlock(&bd->data_mutex);
			
			update_scroll_bar(bd);
			update_status_msg(bd);
			update_controls(bd);
			XClearArea(app_inst.display,XtWindow(bd->wview),0,0,0,0,True);
			launch_loader_thread(bd);
		}break;
		
		case TMSG_REMOVE:{
			long i, j;
			
			pthread_mutex_lock(&bd->data_mutex);
			
			for(i=0; i<msg.change_data.nfiles; i++){
				dbg_assert(bd->nfiles);
				j=find_file_entry(bd,msg.change_data.files[i]);
				if(j<0) continue;
				if(bd->files[j].selected) bd->nsel_files--;
				if(bd->ifocus==j)
					set_focus(bd,(j<bd->nfiles-1)?j:(bd->nfiles-2));
				XmStringFree(bd->files[j].label);
				XDestroyImage(bd->files[j].image);
				if(j<bd->nfiles-1) memmove(&bd->files[j],&bd->files[j+1],
					sizeof(struct browser_file)*((bd->nfiles-1)-j));
				
				bd->nfiles--;
				if(!bd->nfiles){
					free(bd->files);
					bd->files=NULL;
					bd->ifocus=(-1);
					bd->nsel_files=0;
					bd->yoffset=0;
					bd->max_label_height=0;
					update_scroll_bar(bd);
					update_controls(bd);
				}
			}
			
			for(i = 0; i < msg.change_data.ndirs; i++) {
				for(j = 0; j < bd->nsubdirs; j++) {
					if(!strcmp(bd->subdirs[j], msg.change_data.dirs[i])) {
						free(bd->subdirs[j]);
						memmove(&bd->subdirs[j], &bd->subdirs[j + 1],
							(bd->nsubdirs - j) * sizeof(char*));

						bd->nsubdirs--;
						
						XmListDeletePos(bd->wdirlist, j + 1);
						
						break;
					}
				}
			}
			
			pthread_mutex_unlock(&bd->data_mutex);
			
			for(i=0; i<msg.change_data.nfiles; i++)
				free(msg.change_data.files[i]);
			free(msg.change_data.files);
			
			for(i = 0; i < msg.change_data.ndirs; i++)
				free(msg.change_data.dirs[i]);
			free(msg.change_data.dirs);
			
			update_scroll_bar(bd);
			update_status_msg(bd);
			update_controls(bd);
			XClearArea(app_inst.display,XtWindow(bd->wview),0,0,0,0,True);
		}break;
				
		case TMSG_UPDATE:
		/* NOTE: the loader thread keeps data_mutex locked while
		 *       posting TMSG_UPDATE messages */
		if(msg.update_data.index>=0){
			redraw_tile(bd, msg.update_data.index);
		}else{
			XClearArea(app_inst.display,XtWindow(bd->wview),0,0,0,0,True);
			update_scroll_bar(bd);
		}
		update_status_msg(bd);
		update_controls(bd);
		break;
		
		case TMSG_RELOAD:
		launch_loader_thread(bd);
		break;
		
		case TMSG_FINISHED:
		bd->state|=BSF_READY;
		if(msg.notify_data.status){
			errno_message_box(bd->wshell,msg.notify_data.status,
				nlstr(APP_MSGSET,SID_EREADDIR,
				"Error reading directory."),False);
			reset_browser(bd);
		}else if(!bd->update_timer){
			bd->update_timer=XtAppAddTimeOut(app_inst.context,
				bd->refresh_int,update_interval_cb,(XtPointer)bd);
			update_controls(bd);
		}
		update_status_msg(bd);
		break;
		
		case TMSG_CANCELLED:
		if(msg.notify_data.status){
			errno_message_box(bd->wshell,msg.notify_data.status,
				nlstr(APP_MSGSET,SID_EREADDIR,
				"Error reading directory."),False);
			reset_browser(bd);
		}
		update_controls(bd);
		update_status_msg(bd);
		break;
	}
	
	if(bd->ifocus<0) set_focus(bd,-1);
}

/*
 * qsort compare function for browser_file structs
 */
static int file_sort_compare(const void *pa, const void *pb)
{
	struct browser_file *a=(struct browser_file*)pa;
	struct browser_file *b=(struct browser_file*)pb;
	return strcmp(a->name,b->name);
}

static int dir_sort_compare(const void *pa, const void *pb)
{
	char **a = ((char**)pa);
	char **b = ((char**)pb);
	return strcmp(*a, *b);
}


/*
 * Create a file label string; clip the name if necessary.
 */
static XmString create_file_label(struct browser_data *bd, const char *name)
{
	Dimension height;
	Dimension max_width;
	Dimension str_width;
	XmString label;

	max_width=bd->tile_size[bd->itile_size]+bd->border_width*2;
	label=XmStringCreateLocalized((String)name);
	str_width=XmStringWidth(bd->render_table,label);
	if(str_width > max_width){
		char *new_name;
		size_t new_len;

		XmStringFree(label);
		new_len = (float)mb_strlen(name) * ((float)max_width / str_width);
		new_name = shorten_mb_string(name, new_len, False);
		label = XmStringCreateLocalized(new_name);
		free(new_name);
	}
	height=XmStringHeight(bd->render_table,label);
	if(height>bd->max_label_height) bd->max_label_height=height;
	return label;
}

/*
 * Launch a reader thread, wait if one is running; remove update timer if any.
 */
static int launch_reader_thread(struct browser_data *bd)
{
	int res=0;
	
	if(bd->update_timer){
		XtRemoveTimeOut(bd->update_timer);
		bd->update_timer=None;
	}
	pthread_mutex_lock(&bd->rdr_cond_mutex);
	if(bd->state&BSF_READING)
		pthread_cond_wait(&bd->ldr_cond,&bd->rdr_cond_mutex);
	pthread_mutex_unlock(&bd->rdr_cond_mutex);
	bd->state|=BSF_READING;
	if(pthread_create(&bd->rdr_thread,NULL,reader_thread,(void*)bd)){
		res=errno;
		bd->state&=(~BSF_READING);
	}
	update_status_msg(bd);
	return res;
}

/*
 * Launch a loader thread, wait if one is running; remove update timer if any.
 */
static int launch_loader_thread(struct browser_data *bd)
{
	int res=0;
	
	pthread_mutex_lock(&bd->ldr_cond_mutex);
	if(bd->state&BSF_LOADING)
		pthread_cond_wait(&bd->ldr_cond,&bd->ldr_cond_mutex);
	pthread_mutex_unlock(&bd->ldr_cond_mutex);
	bd->state|=BSF_LOADING;
	if(pthread_create(&bd->ldr_thread,NULL,loader_thread,(void*)bd)){
		res=errno;
		bd->state&=(~BSF_LOADING);
	}
	update_status_msg(bd);
	return res;
}


/*
 * Free intermediate browser data
 */
static void reset_browser(struct browser_data *bd)
{
	if(bd->update_timer){
		XtRemoveTimeOut(bd->update_timer);
		bd->update_timer=None;
	}
	/* if a thread is running set its state to cancelled
	 * and wait for it to exit */
	pthread_mutex_lock(&bd->rdr_cond_mutex);
	if(bd->state&BSF_READING){
		bd->state|=BSF_RCANCEL;
		update_status_msg(bd);
		pthread_cond_wait(&bd->rdr_cond,&bd->rdr_cond_mutex);
	}
	pthread_mutex_unlock(&bd->rdr_cond_mutex);

	pthread_mutex_lock(&bd->ldr_cond_mutex);
	if(bd->state&BSF_LOADING){
		bd->state|=BSF_LCANCEL;
		update_status_msg(bd);
		pthread_cond_wait(&bd->ldr_cond,&bd->ldr_cond_mutex);
	}
	pthread_mutex_unlock(&bd->ldr_cond_mutex);

	/* process stale thread events; they may hold pointers to
	 * dynamically allocated memory which is freed by the handler */
	bd->state|=BSF_RESET;
	while(XtAppPending(app_inst.context)&XtIMAlternateInput)
		XtAppProcessEvent(app_inst.context,XtIMAlternateInput);
	
	XClearArea(app_inst.display,XtWindow(bd->wview),0,0,0,0,False);	
	XmListDeleteAllItems(bd->wdirlist);
	
	if(bd->path) free(bd->path);
	bd->path=NULL;
	
	if(bd->nfiles){
		while(bd->nfiles--){
			free(bd->files[bd->nfiles].name);
			XmStringFree(bd->files[bd->nfiles].label);
			if(bd->files[bd->nfiles].image)
				XDestroyImage(bd->files[bd->nfiles].image);
		}
		free(bd->files);
	}

	if(bd->nsubdirs) {
		while(bd->nsubdirs--) free(bd->subdirs[bd->nsubdirs]);
		free(bd->subdirs);
	}

	bd->subdirs = NULL;
	bd->nsubdirs = 0;
	bd->files=NULL;
	bd->nfiles=0;
	bd->nsel_files=0;		
	bd->ifocus=(-1);
	bd->yoffset=0;
	bd->max_label_height=0;
	bd->state=0;
	
	#ifdef ENABLE_CDE
	/* notify the requestor if the browser was instantiated trough ToolTalk */
	if(bd->tt_disp_req){
		ttmedia_load_reply(bd->tt_disp_req,NULL,0,True);
		bd->tt_disp_req=NULL;
	}
	#endif /* ENABLE_CDE */
	
	update_shell_title(bd);
	update_controls(bd);
	update_status_msg(bd);
	update_scroll_bar(bd);
	set_navbar_path(bd->wnavbar,NULL);

	XmUpdateDisplay(bd->wshell);
}

/*
 * Destroy all browser data and widgets.
 */
static void destroy_browser(struct browser_data *bd)
{
	#ifdef ENABLE_CDE
	/* finish ToolTalk contracts if any */
	if(bd->tt_disp_req){
		ttmedia_load_reply(bd->tt_disp_req,NULL,0,True);
		bd->tt_disp_req=NULL;
	}
	if(bd->tt_quit_req){
		tt_message_reply(bd->tt_quit_req);
		tt_message_destroy(bd->tt_quit_req);
		bd->tt_quit_req=NULL;
	}
	#endif /* ENABLE_CDE */

	/* free browser_data internal stuff */
	if(bd->state&(BSF_READING|BSF_LOADING|BSF_READY))
		reset_browser(bd);

	XtRemoveInput(bd->thread_notify_input);
		
	pthread_mutex_destroy(&bd->data_mutex);
	pthread_mutex_destroy(&bd->rdr_cond_mutex);
	pthread_cond_destroy(&bd->rdr_cond);
	pthread_mutex_destroy(&bd->ldr_cond_mutex);
	pthread_cond_destroy(&bd->ldr_cond);
	
	XFreeGC(app_inst.display,bd->draw_gc);
	XFreeGC(app_inst.display,bd->text_gc);
	
	if(bd->last_dest_dir) free(bd->last_dest_dir);
	free(bd->title);
	
	close(bd->tnfd[0]);
	close(bd->tnfd[1]);
	
	/* destroy the widgets and unlink and free browser_data */
	XtDestroyWidget(bd->wshell);
	
	/* unlink and free the instance container */
	if(bd!=browsers){
		struct browser_data *prev=browsers;
		while(prev->next!=bd)
			prev=prev->next;
		prev->next=bd->next;
	}else{
		browsers=bd->next;
	}
	free(bd);
	app_inst.active_shells--;
}

/*
 * View input handler
 */
static void input_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	struct browser_data *bd=(struct browser_data*)client_data;
	XmDrawingAreaCallbackStruct *cbs=
		(XmDrawingAreaCallbackStruct*)call_data;
	Modifiers mods;
	KeySym sym;	

	if(!bd->nfiles){
		if(cbs->event->type==KeyPress){
			XmTranslateKey(app_inst.display,cbs->event->xkey.keycode,
				cbs->event->xkey.state,&mods,&sym);
			if(sym==XK_Tab)
				XmProcessTraversal(bd->wnavbar,XmTRAVERSE_CURRENT);
		}
		return;
	}
		
	switch(cbs->event->type){
		case ButtonPress:{
			XButtonEvent *e=(XButtonEvent*)cbs->event;
			enum sel_mode smode;
			Boolean hit;

			if(e->state&ShiftMask && (e->button==Button1 ||
				e->button==Button3)){
				smode=SM_EXTEND;
			}else if(e->state&ControlMask && (e->button==Button1 ||
				e->button==Button3) && bd->nsel_files){
				smode=SM_MULTI;
			}else{
				smode=SM_SINGLE;
			}
			hit=select_tile_at(bd,e->x,e->y,smode);
			if(hit && bd->nsel_files && e->button==Button3){
				Widget wrename=XtNameToWidget(bd->wpopup,"*rename");
				dbg_assert(wrename);
				XtSetSensitive(wrename,(bd->nsel_files==1)?True:False);
				XmMenuPosition(bd->wpopup,(XButtonPressedEvent*)cbs->event);
				XtManageChild(bd->wpopup);
			}
		}break; /* ButtonPress */
		
		case KeyPress:{
			XKeyEvent *e=(XKeyEvent*)cbs->event;
			long tiles_per_row;
			long new_focus=(-1);
			XmTranslateKey(app_inst.display,e->keycode,e->state,&mods,&sym);

			switch(sym){
				/* page scrolling */
				case osfXK_PageUp:{
					String p="Up";
					XEvent e={0};
					XtCallActionProc(bd->wvscroll,"PageUpOrLeft",&e,&p,1);
				}break;
				case osfXK_PageDown:{
					String p="Down";
					XEvent e={0};
					XtCallActionProc(bd->wvscroll,"PageDownOrRight",&e,&p,1);
				}break;
				
				/* actions */
				case XK_space:
				toggle_selection(bd,bd->ifocus);
				break;

				/* traversal */
				case XK_Tab:
				XmProcessTraversal(bd->wnavbar,XmTRAVERSE_CURRENT);
				break;
				case osfXK_Right:
				if(bd->ifocus+1>(bd->nfiles-1))
					new_focus=0;
				else
					new_focus=bd->ifocus+1;
				break;
				case osfXK_Left:
				if(bd->ifocus-1<0)
					new_focus=bd->nfiles-1;
				else
					new_focus=bd->ifocus-1;
				break;
				case osfXK_Up:
				compute_tile_dimensions(bd,&tiles_per_row,
					NULL,NULL,NULL,NULL,NULL);
				if(bd->ifocus-tiles_per_row<0){
					new_focus=(bd->nfiles/tiles_per_row)*
						tiles_per_row+bd->ifocus;
					if(new_focus>bd->nfiles-1) new_focus-=tiles_per_row;
				}else{
					new_focus=bd->ifocus-tiles_per_row;
				}
				break;
				case osfXK_Down:
				compute_tile_dimensions(bd,&tiles_per_row,
					NULL,NULL,NULL,NULL,NULL);
				if(bd->ifocus+tiles_per_row > bd->nfiles-1)
					new_focus=bd->ifocus-(bd->ifocus/tiles_per_row)*
						tiles_per_row;
				else
					new_focus=bd->ifocus+tiles_per_row;
				break;
				
				/* default action */
				case XK_Return:
				invoke_default_action(bd,bd->ifocus);
				break;
				
				/* set selection anchor */
				case XK_Shift_L:
				case XK_Shift_R:
				if(bd->ifocus<0) set_focus(bd,-1);
				bd->sel_start=bd->ifocus;
				break;
			}
			if(new_focus>=0){
				if(e->state&ShiftMask && bd->sel_start>=0)
					set_selection(bd,bd->sel_start,new_focus,True);
				set_focus(bd,new_focus);
				update_status_msg(bd);
				scroll_to(bd,new_focus);
			}
		}break; /* KeyPress */
		
		case KeyRelease:{
			XKeyEvent *e=(XKeyEvent*)cbs->event;
			XmTranslateKey(app_inst.display,e->keycode,e->state,&mods,&sym);
			switch(sym){
				case XK_Shift_L:
				case XK_Shift_R:
				bd->sel_start=(-1);
				break;
			}
		}break; /* KeyRelease */
	}
}

/*
 * Set selection from i to l, clear current selection if specified.
 */
static void set_selection(struct browser_data *bd,
	long i, long l, Boolean clear)
{
	long j,k;
	
	if(clear && bd->nsel_files>0){
		for(j=0; j<bd->nfiles; j++){
			if(bd->files[j].selected) {
				bd->files[j].selected=False;
				redraw_tile(bd, j);
			}
		}
		bd->nsel_files=0;
	}

	k=(l>i)?1:(-1);
	
	for(j=labs(l-i); j>=0; j--, i+=k){
		bd->files[i].selected=True;
		bd->nsel_files++;
		redraw_tile(bd, i);
	}
	update_controls(bd);
}

/*
 * Toggle tile's selected flag
 */
static void toggle_selection(struct browser_data *bd, long i)
{
	if(bd->files[i].selected){
		bd->files[i].selected=False;
		bd->nsel_files--;
	}else{
		bd->files[i].selected=True;
		bd->nsel_files++;
	}
	redraw_tile(bd, i);
	update_controls(bd);
}

/*
 * Clear current selection
 */
static void clear_selection(struct browser_data *bd)
{
	long i;
	
	if(bd->nsel_files>0){
		for(i=0; i<bd->nfiles; i++){
			if(bd->files[i].selected) {
				bd->files[i].selected = False;
				redraw_tile(bd, i);
			}
		}
	}
	bd->nsel_files=0;
	update_controls(bd);
}

/*
 * Set set focus to the specified tile, or first visible if 'i' is -1.
 */
static void set_focus(struct browser_data *bd, long i)
{
	long prev_focus=bd->ifocus;
	
	bd->owns_primary = XtOwnSelection(bd->wshell, XA_PRIMARY,
		XtLastTimestampProcessed(app_inst.display),
		convert_selection_proc, lose_selection_proc, NULL);
	
	bd->ifocus=i;
	if(!bd->nfiles) return;
	
	if(prev_focus>=0) redraw_tile(bd, prev_focus);

	if(i<0){
		unsigned int h;
		compute_tile_dimensions(bd,NULL,NULL,NULL,NULL,NULL,&h);
		bd->ifocus=(bd->yoffset/h);
	}
	redraw_tile(bd, bd->ifocus);
	update_status_msg(bd);
}

/*
 * Check if there is a tile at x,y and mark it as selected.
 * If selection mode is SM_SINGLE the fuction checks for double clicks
 * and calls the default activation handler if a double-click is detected.
 * Returns true if a tile exists at the given position, False otherwise.
 */
static Boolean select_tile_at(struct browser_data *bd,
	int x, int y, int mode)
{
	int tx, ty;
	unsigned int tw, th, tih;
	long tiles_per_row;
	long i;
	Boolean hit=False;
	
	compute_tile_dimensions(bd,&tiles_per_row,NULL,NULL,&tih,&tw,&th);

	for(i=(bd->yoffset/th)*tiles_per_row; i<bd->nfiles; i++){
		compute_tile_position(bd,i,&tx,&ty,&tw,&th,NULL);
		if(x>tx && y>ty && x<tx+tw && y <ty+th){
			if(bd->dblclk_timer && mode==SM_SINGLE){
				XtRemoveTimeOut(bd->dblclk_timer);
				bd->dblclk_timer=None;
				if(y-ty>tih){
					rename_cb(None,(XtPointer)bd,NULL);
				}else{
					invoke_default_action(bd,bd->ifocus);
				}
			}else{
				if(bd->dblclk_timer) XtRemoveTimeOut(bd->dblclk_timer);
				if(bd->files[i].selected){
					switch(mode){
						case SM_SINGLE:
						if(bd->nsel_files>1)
							set_selection(bd,i,i,True);
						break;
						case SM_MULTI:
						toggle_selection(bd,i);
						break;
						case SM_EXTEND:
						set_selection(bd,bd->sel_start,i,True);
						break;
					}
				}else{
					if(mode==SM_EXTEND){
						set_selection(bd,bd->sel_start,i,True);
						set_focus(bd,i);
					}else{
						Boolean clear=(mode==SM_SINGLE)?True:False;
						set_selection(bd,i,i,clear);
					}
				}
				if(mode==SM_SINGLE){
					bd->dblclk_timer=XtAppAddTimeOut(app_inst.context,
						XtGetMultiClickTime(app_inst.display),
						dblclk_timer_cb,(XtPointer)bd);
				}
				set_focus(bd,i);
			}
			hit=True;
			break;
		}
	}
	
	XmUpdateDisplay(bd->wview);
	return hit;
}

/*
 * Double click timeout callback
 */
static void dblclk_timer_cb(XtPointer data, XtIntervalId *id)
{
	struct browser_data *bd=(struct browser_data*)data;
	bd->dblclk_timer=None;
}

/*
 * Compute inner dimensions of tiles and their arrangement.
 * NULL may be passed for undesired parameters.
 */
static void compute_tile_dimensions(struct browser_data *bd,
	long *tiles_per_row, long *tiles_per_col,
	unsigned int *tile_iwidth, unsigned int *tile_iheight,
	unsigned int *tile_owidth, unsigned int *tile_oheight)
{
	if(tiles_per_row || tiles_per_col){
		long tpr;
		tpr=(bd->view_width-TILE_XMARGIN)/
			(bd->tile_size[bd->itile_size]+TILE_XMARGIN);
		tpr=(tpr>0)?tpr:1;
		if(tiles_per_row) *tiles_per_row=tpr;
		if(tiles_per_col)
			*tiles_per_col=ceilf((float)bd->nfiles/(*tiles_per_row));
	}
	if(tile_iwidth) *tile_iwidth=bd->tile_size[bd->itile_size];
	if(tile_owidth)	*tile_owidth=bd->tile_size[bd->itile_size]+TILE_XMARGIN;
	if(tile_iheight || tile_oheight){
		unsigned int ih;
		ih=((bd->tile_size[bd->itile_size]/bd->tile_asr[0])*bd->tile_asr[1]);
		if(tile_iheight) *tile_iheight=ih;
		if(tile_oheight)
			*tile_oheight=(ih+LABEL_MARGIN+TILE_YMARGIN+bd->max_label_height);
	}
}
	

/*
 * Compute tile's position, extents and visibility within the view.
 * NULL may be passed for undesired parameters.
 */
static void compute_tile_position(struct browser_data *bd,
	long itile, int *tx, int *ty,
	unsigned int *tw, unsigned int *th, Boolean *viewable)
{
	unsigned int tile_width, tile_height;
	unsigned int tile_outer_height;
	int xpos, ypos;
	long tiles_per_row;
	long yoff, xoff;
	
	compute_tile_dimensions(bd,&tiles_per_row,NULL,
		&tile_width,&tile_height,NULL,&tile_outer_height);
		
	yoff=itile/tiles_per_row;
	xoff=itile-yoff*tiles_per_row;
	
	xpos=TILE_XMARGIN+xoff*tile_width+xoff*TILE_XMARGIN;
	ypos=(yoff*tile_outer_height+TILE_YMARGIN)-bd->yoffset;
	if(tx) *tx=xpos;
	if(ty) *ty=ypos;
	if(tw) *tw=tile_width;
	if(th) *th=tile_outer_height;
	
	if(viewable && (ypos+tile_outer_height<0 || ypos > bd->view_height))
		*viewable=False;
	else if(viewable)
		*viewable=True;
}

/*
 * Set vertical offset so that 'i' becomes completely visible.
 */
static void scroll_to(struct browser_data *bd, long i)
{
	int y;
	unsigned int h;
	int offset=bd->yoffset;
	
	compute_tile_position(bd,i,NULL,&y,NULL,&h,NULL);
	if(y<0){
		offset+=y;
		set_vscroll(bd,(offset>0)?offset:0);
	}else if(y+h>=bd->view_height){
		offset+=(y-bd->view_height)+h;
		set_vscroll(bd,offset);
	}
}

/*
 * View area exposure handler
 */
static void expose_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	XmDrawingAreaCallbackStruct *cbs=
		(XmDrawingAreaCallbackStruct*)call;
	XExposeEvent *evt=(XExposeEvent*)cbs->event;
	Region reg;
	XRectangle rc = { evt->x, evt->y, evt->width, evt->height };
	unsigned int tile_width, tile_height;
	unsigned int tile_outer_height;
	int cx, cy;
	int offset_ntiles;
	Window wview;
	long i;
	long tiles_per_row;
	
	if(!bd->nfiles || bd->view_width<1 || bd->view_height<1) return;

	compute_tile_dimensions(bd,&tiles_per_row,NULL,
		&tile_width,&tile_height,NULL,&tile_outer_height);
	wview=XtWindow(bd->wview);
	
	reg = XCreateRegion();
	XUnionRectWithRegion(&rc, reg, reg);

	offset_ntiles=(bd->yoffset/tile_outer_height);

	for(i = offset_ntiles * tiles_per_row,
		cy = -(bd->yoffset - offset_ntiles * tile_outer_height);
		(i < bd->nfiles) && ((cy - bd->yoffset) < bd->view_height); ){

		unsigned long j;
		for(j=0, cx=0; j<tiles_per_row && i<bd->nfiles; j++, i++){
			XPoint fpts[4];
			int xpos = cx + j * (tile_width+TILE_XMARGIN) + TILE_XMARGIN;
			int ypos = cy + TILE_YMARGIN;
			
			if(! (XRectInRegion(reg, xpos, ypos, tile_width, tile_outer_height) 
				& (RectangleIn | RectangleOut | RectanglePart)) ) {
					continue;
			}
			
			if(bd->files[i].selected){
				XSetForeground(app_inst.display,bd->draw_gc,bd->sbg_pixel);
				XFillRectangle(app_inst.display,wview,bd->draw_gc,
					xpos,ypos,tile_width,tile_height);
				
				XSetForeground(app_inst.display, bd->text_gc, bd->bg_pixel);
				XSetForeground(app_inst.display,bd->draw_gc,bd->fg_pixel);
				XFillRectangle(app_inst.display,wview,bd->draw_gc,
					xpos,ypos+tile_height+LABEL_MARGIN,
					tile_width,bd->max_label_height);
			}else{
				XSetForeground(app_inst.display, bd->text_gc, bd->fg_pixel);
				XSetForeground(app_inst.display,bd->draw_gc,bd->bg_pixel);
				XFillRectangle(app_inst.display,wview,bd->draw_gc,
					xpos,ypos+tile_height+LABEL_MARGIN,
					tile_width,bd->max_label_height);
			}
			
			XSetLineAttributes(app_inst.display,bd->draw_gc,bd->border_width,
				LineSolid,CapButt,JoinMiter);
			fpts[0].x=xpos; fpts[0].y=ypos+tile_height;
			fpts[1].x=0; fpts[1].y=-tile_height;
			fpts[2].x=tile_width; fpts[2].y=0;
			XSetForeground(app_inst.display,bd->draw_gc,
				(bd->files[i].selected)?bd->bs_pixel:bd->ts_pixel);
			XDrawLines(app_inst.display,wview,bd->draw_gc,
				fpts,3,CoordModePrevious);
			fpts[0].x=xpos; fpts[0].y=ypos+tile_height;
			fpts[1].x=tile_width-1; fpts[1].y=0;
			fpts[2].x=0; fpts[2].y=-tile_height;
			XSetForeground(app_inst.display,bd->draw_gc,
				(bd->files[i].selected)?bd->ts_pixel:bd->bs_pixel);
			XDrawLines(app_inst.display,wview,bd->draw_gc,
				fpts,3,CoordModePrevious);
	
			if(bd->files[i].state==FS_VIEWABLE){
				int im_x=0, im_y=0;
				if(bd->files[i].image->width < tile_width)
					im_x=(tile_width - bd->files[i].image->width)/2;
				if(bd->files[i].image->height < tile_height)
					im_y=(tile_height - bd->files[i].image->height)/2;
				XPutImage(app_inst.display,wview,bd->draw_gc,
					bd->files[i].image,0,0,xpos+im_x,ypos+im_y,
					bd->files[i].image->width,
					bd->files[i].image->height);
			}else{
				Dimension pm_width, pm_height;
				int pm_x=0, pm_y=0;
				Pixmap state_pixmap=get_state_pixmap(bd,bd->files[i].state,
					bd->files[i].selected,&pm_width,&pm_height);
				pm_x=(tile_width-pm_width)/2;
				pm_y=(tile_height-pm_height)/2;
				XCopyArea(app_inst.display,state_pixmap,wview,
					bd->draw_gc,0,0,pm_width,pm_height,xpos+pm_x,ypos+pm_y);
			}
			
			XmStringDraw(app_inst.display,wview,bd->render_table,
				bd->files[i].label,bd->text_gc,xpos,
				ypos+tile_height+LABEL_MARGIN,
				tile_width,XmALIGNMENT_CENTER,
				XmSTRING_DIRECTION_DEFAULT,NULL);
				
			if(i==bd->ifocus){
				XSetLineAttributes(app_inst.display,bd->draw_gc,
					bd->border_width,LineOnOffDash,CapButt,JoinMiter);
				XDrawRectangle(app_inst.display,wview,
					bd->draw_gc,xpos+2,ypos+2,
					tile_width-5,tile_height-5);
				if(bd->owns_primary)
					XDrawRectangle(app_inst.display,wview,bd->draw_gc,
						xpos,ypos+tile_height+LABEL_MARGIN,
						tile_width - 1, bd->max_label_height - 1);
			}
		}
		cy+=tile_outer_height;
	}
	XDestroyRegion(reg);
	XFlush(app_inst.display);
}

/*
 * Erases a tile and generates exposure event if it's visible
 */
static void redraw_tile(struct browser_data *bd, long i)
{
	int x, y;
	unsigned int w, h;
	Boolean visible;

	compute_tile_position(bd, i, &x, &y, &w, &h, &visible);
	if(visible) {
		XClearArea(app_inst.display, XtWindow(bd->wview),
			x - TILE_XMARGIN, y - TILE_YMARGIN,
			w + TILE_XMARGIN * 2, h + TILE_YMARGIN * 2, True);
	}
}

/*
 * View widget sizing handler
 */
static void resize_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	Arg args[2]={
		{XmNwidth,(XtArgVal)&bd->view_width},
		{XmNheight,(XtArgVal)&bd->view_height}
	};
	XtGetValues(bd->wview,args,2);
	XClearArea(app_inst.display,XtWindow(w),0,0,0,0,True);
	update_scroll_bar(bd);
}

/*
 * View widget mapping handler
 */
static void view_map_cb(Widget w, XtPointer data, XEvent *evt, Boolean *cont)
{
	struct browser_data *bd=(struct browser_data*)data;
	Arg args[2]={
		{XmNwidth,(XtArgVal)&bd->view_width},
		{XmNheight,(XtArgVal)&bd->view_height}
	};
	if(evt->type!=MapNotify) return;
	XtGetValues(bd->wview,args,2);
	update_scroll_bar(bd);
	XClearArea(app_inst.display,XtWindow(bd->wview),0,0,0,0,True);
}

/*
 * Vertical scroll callback
 */
static void scroll_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	XmScrollBarCallbackStruct *sbcs=(XmScrollBarCallbackStruct*)call;
	set_vscroll(bd,sbcs->value);
}

/*
 * Update vertical offset value and scroll the view
 */
static void set_vscroll(struct browser_data *bd, int new_offset)
{
	int delta;
	Window view=XtWindow(bd->wview);
	Arg args[1]={{XmNvalue,new_offset}};
	
	if(new_offset==bd->yoffset)	return;

	XtSetValues(bd->wvscroll,args,1);

	delta = new_offset - bd->yoffset;

	bd->yoffset = new_offset;
	
	if(abs(delta) >= bd->view_height) {
		XClearArea(app_inst.display, view, 0, 0,
			bd->view_width, bd->view_height, True);
		return;
	}
	
	if(delta>0){
		XCopyArea(app_inst.display,view,view,bd->draw_gc,0,delta,
			bd->view_width,bd->view_height-delta,0,0);
		XClearArea(app_inst.display,view,0,bd->view_height-delta,
			bd->view_width,delta,True);
	}else{
		delta=abs(delta);
		XCopyArea(app_inst.display,view,view,bd->draw_gc,0,0,
			bd->view_width,bd->view_height-delta,0,delta);
		XClearArea(app_inst.display,view,0,0,bd->view_width,delta,True);
	}
}

/*
 * Update scroll bar min/max values and the current position
 */
static void update_scroll_bar(struct browser_data *bd)
{
	int max=1, cur=0, slider=1, page=10;
	Arg args[5];
	int i=0;
	
	if(bd->nfiles && bd->view_width>0 && bd->view_height>0){
		unsigned int tile_height;
		long tiles_per_row;
		long tiles_per_col;
		int delta;

		i=0;
		XtSetArg(args[i],XmNsliderSize,&slider); i++;
		XtSetArg(args[i],XmNvalue,&cur); i++;
		XtGetValues(bd->wvscroll,args,i);
		if(cur>1 && (delta=slider-bd->view_height)<0){
			bd->yoffset+=delta;
			dbg_assert(bd->yoffset>=0);
		}
		compute_tile_dimensions(bd,&tiles_per_row,&tiles_per_col,
			NULL,NULL,NULL,&tile_height);
		max=(tile_height*tiles_per_col+TILE_YMARGIN)-bd->view_height;
		max=(max>0)?max+bd->view_height:bd->view_height;
		slider=bd->view_height;
		bd->yoffset=(bd->yoffset>max-bd->view_height)?0:bd->yoffset;
		cur=bd->yoffset;
		page=tile_height;
	}
	
	i=0;
	XtSetArg(args[i],XmNminimum,0); i++;
	XtSetArg(args[i],XmNmaximum,max); i++;
	XtSetArg(args[i],XmNvalue,cur); i++;
	XtSetArg(args[i],XmNsliderSize,slider); i++;
	XtSetArg(args[i],XmNpageIncrement,page); i++;
	XtSetValues(bd->wvscroll,args,i);
}

/*
 * Update input widgets to reflect the current browser state
 */
static void update_controls(struct browser_data *bd)
{
	Boolean ready=((bd->state&BSF_READY)?True:False);
	
	XtSetSensitive(get_menu_item(bd,"*selectNone"),bd->nsel_files);
	XtSetSensitive(get_menu_item(bd,"*invertSelection"),bd->nsel_files);
	XtSetSensitive(get_menu_item(bd,"*copyTo"),bd->nsel_files);
	XtSetSensitive(get_menu_item(bd,"*moveTo"),bd->nsel_files);
	XtSetSensitive(get_menu_item(bd,"*delete"),bd->nsel_files);
	XtSetSensitive(get_menu_item(bd,"*rename"),bd->nsel_files==1);
	XtSetSensitive(get_menu_item(bd,"*refresh"),ready);
	XtSetSensitive(get_menu_item(bd,"*display"),(bd->ifocus>=0));
	XtSetSensitive(get_menu_item(bd,"*edit"),
		((bd->ifocus>=0)&&(init_app_res.edit_cmd)));
	XmUpdateDisplay(bd->wmenubar);
}

/*
 * Set shell and icon title to reflect the current state of the browser.
 */
static void update_shell_title(struct browser_data *bd)
{
	if(bd->path){
		char *buffer;
		char *dir;
		dir=strrchr(bd->path,'/');
		if(!dir)
			dir=bd->path;
		else if(dir[1]!='\0') dir++;
		buffer=malloc(strlen(bd->title)+strlen(dir)+4);
		sprintf(buffer,"%s - %s",dir,bd->title);
		XtVaSetValues(bd->wshell,XmNtitle,buffer,XmNiconName,buffer,NULL);
		free(buffer);
	}else{
		XtVaSetValues(bd->wshell,XmNtitle,bd->title,
			XmNiconName,bd->title,NULL);
	}
}

/*
 * Display a message summarizing the current browser state in the status area
 */
static void update_status_msg(struct browser_data *bd)
{
	XmString xm_str;
	Arg args[1];
	
	if(bd->state&BSF_READING && !(bd->state&BSF_READY)){
		if(bd->state&(BSF_LCANCEL|BSF_RCANCEL))
			xm_str=XmStringCreateLocalized(nlstr(APP_MSGSET,
				SID_BUSY,"Processing..."));
		else
			xm_str=XmStringCreateLocalized(nlstr(APP_MSGSET,
				SID_READDIRPG,"Reading directory..."));
	}else{
		if(!bd->nfiles){
			xm_str=XmStringCreateLocalized(
				nlstr(APP_MSGSET,SID_NOITEMS,"No items to display"));
		}else{
			char *stat_str;
			size_t str_len;
			char *files_str=nlstr(APP_MSGSET,SID_FILES,"Files");
			char *sel_str=nlstr(APP_MSGSET,SID_SELECTED,"Selected");
			
			if(bd->ifocus>=0){
				if(bd->files[bd->ifocus].state==FS_PENDING){
					char *loading_str=nlstr(
						APP_MSGSET,SID_LOADING,"Loading...");
					str_len=strlen(files_str)+strlen(sel_str)+
							strlen(bd->files[bd->ifocus].name)+
							strlen(loading_str)+20;
					stat_str=malloc(str_len+1);
					snprintf(stat_str,str_len,
						"%ld %s, %ld %s; %s: %s",
						bd->nfiles,files_str,bd->nsel_files,sel_str,
						bd->files[bd->ifocus].name,loading_str);
				}else{
					if(bd->files[bd->ifocus].loader_result==0){
						str_len=strlen(files_str)+strlen(sel_str)+
							strlen(bd->files[bd->ifocus].name)+46;
						stat_str=malloc(str_len+1);
						snprintf(stat_str,str_len,
							"%ld %s, %ld %s; %s: %ldx%ld, %d BPP",
							bd->nfiles,files_str,bd->nsel_files,sel_str,
							bd->files[bd->ifocus].name,
							bd->files[bd->ifocus].xres,
							bd->files[bd->ifocus].yres,
							bd->files[bd->ifocus].bpp);
					}else{
						char *loader_msg=img_strerror(
							bd->files[bd->ifocus].loader_result);
						str_len=strlen(files_str)+strlen(sel_str)+
							strlen(bd->files[bd->ifocus].name)+
							strlen(loader_msg)+32;
						stat_str=malloc(str_len+1);
						snprintf(stat_str,str_len,
							"%ld %s, %ld %s; %s: %s",
							bd->nfiles,files_str,bd->nsel_files,sel_str,
							bd->files[bd->ifocus].name,loader_msg);
					}
				}
			}else{
				str_len=strlen(files_str)+strlen(sel_str)+26;
				stat_str=malloc(str_len+1);
				snprintf(stat_str,str_len,"%ld %s, %ld %s",
					bd->nfiles,files_str,bd->nsel_files,sel_str);
			}
			xm_str=XmStringCreateLocalized(stat_str);
			free(stat_str);
		}
	}
	XtSetArg(args[0],XmNlabelString,xm_str);
	XtSetValues(bd->wmsg,args,1);
	XmUpdateDisplay(bd->wmsg);
	XmStringFree(xm_str);
}

/*
 * Invoke default action for a tile.
 */
static void invoke_default_action(struct browser_data *bd, long i)
{
	char *full_path;
	Widget viewer;
	
	switch(bd->files[i].state){
		case FS_PENDING:
		case FS_VIEWABLE:
		full_path=malloc(bd->path_max+1);
		snprintf(full_path,bd->path_max,"%s/%s",bd->path,bd->files[i].name);
		viewer=get_viewer(&init_app_res,NULL);
		if(!viewer){
			free(full_path);
			break;
		}
		display_image(viewer,full_path,NULL);
		free(full_path);
		break;
		case FS_BROKEN:
		case FS_ERROR:
		message_box(bd->wshell,MB_NOTIFY,bd->title,
			img_strerror(bd->files[bd->ifocus].loader_result));
		break;
		case _NUM_FS_VALUES:
		break;
	}
}

/*
 * Set current tile preset
 */
static void set_tile_size(struct browser_data *bd, enum tile_preset preset)
{
	long i;
	/* NOTE: these must be the same order as enum tile_presets */
	char *menu_items[]={"*smallTiles","*mediumTiles","*largeTiles"};
	
	XmToggleButtonGadgetSetState(
		get_menu_item(bd,menu_items[preset]),True,False);
	if(preset==bd->itile_size) return;
	XmToggleButtonGadgetSetState(
		get_menu_item(bd,menu_items[bd->itile_size]),False,False);
	bd->itile_size=preset;

	if(!bd->nfiles) return;
	
	pthread_mutex_lock(&bd->ldr_cond_mutex);
	if(bd->state&BSF_LOADING){
		bd->state|=BSF_LCANCEL;
		pthread_cond_wait(&bd->ldr_cond,&bd->ldr_cond_mutex);
	}
	pthread_mutex_unlock(&bd->ldr_cond_mutex);
	
	pthread_mutex_lock(&bd->data_mutex);
	for(i=0; i<bd->nfiles; i++){
		if(bd->files[i].state==FS_VIEWABLE)
			XDestroyImage(bd->files[i].image);
		bd->files[i].state=FS_PENDING;
		bd->files[i].image=NULL;
		XmStringFree(bd->files[i].label);
		bd->files[i].label=create_file_label(bd,bd->files[i].name);
		bd->files[i].label_width=
			XmStringWidth(bd->render_table,bd->files[i].label);
	}
	pthread_mutex_unlock(&bd->data_mutex);
	update_scroll_bar(bd);
	XClearArea(app_inst.display,XtWindow(bd->wview),0,0,0,0,True);
	launch_loader_thread(bd);
}

/*
 * Parse aspect ratio and tile size presets strings
 */
static void parse_res_strings(const struct app_resources *res,
	struct browser_data *bd)
{
	char *tmp;
	char *token;
	char seps[]=", ";
	short size[_NUM_TS_PRESETS];
	short asr[2];
	short i=0;
	short itile_size=0;
	char *preset_strings[]={"small","medium","large"};
	
	tmp=strdup(res->tile_presets);
	token=strtok(tmp,seps);
	do{
		size[i]=atoi(token);
		if(size[i]<MIN_TILE_SIZE || size[i]>MAX_TILE_SIZE){
			break;
		}
		i++;
		if(i == _NUM_TS_PRESETS) break;
	}while((token=strtok(NULL,seps)));

	if(i != _NUM_TS_PRESETS){
		warning_msg("Illegal tile size preset string, using defaults.");
		for(i=0; i<_NUM_TS_PRESETS; i++) size[i]=(MIN_TILE_SIZE*2)*(i+1);
	}
	free(tmp);
	
	if(sscanf(res->tile_asr,"%hd:%hd",&asr[0],&asr[1])<2){
		warning_msg("Illegal aspect ratio string, using default.");
		asr[0]=4; asr[1]=3;
	}
	for(i=0; i<_NUM_TS_PRESETS; i++){
		if(!strcasecmp(preset_strings[i],res->tile_size)){
			itile_size=i;
			break;
		}
	}
	bd->itile_size=itile_size;
	memcpy(bd->tile_asr,asr,sizeof(short)*2);
	memcpy(bd->tile_size,size,sizeof(short)*_NUM_TS_PRESETS);
	
	if(res->refresh_int < 3 || res->refresh_int > 60){
		warning_msg("Illegal refresh interval, using default.");
		bd->refresh_int=DEF_REFRESH_INT*1000;
	}else{
		bd->refresh_int=res->refresh_int*1000;
	}
}

/*
 * Returns a cached file state pixmap
 */
static Pixmap get_state_pixmap(struct browser_data *bd,
	enum file_state state, Boolean selected,
	Dimension *width, Dimension *height)
{
	#include "bitmaps/hglass.bm"
	#include "bitmaps/error.bm"
	#include "bitmaps/broken.bm"
	static Pixmap npm[_NUM_FS_VALUES]={0};
	static Pixmap spm[_NUM_FS_VALUES]={0};
	static Dimension widths[_NUM_FS_VALUES];
	static Dimension heights[_NUM_FS_VALUES];
		
	if(!npm[state]){
		switch(state){
			case FS_PENDING:
			widths[state]=hglass_width;
			heights[state]=hglass_height;
			npm[state]=load_bitmap(hglass_bits,hglass_width,
				hglass_height,bd->fg_pixel,bd->bg_pixel);
			spm[state]=load_bitmap(hglass_bits,hglass_width,
				hglass_height,bd->fg_pixel,bd->sbg_pixel);
			break;
			case FS_BROKEN:
			widths[state]=broken_width;
			heights[state]=broken_height;
			npm[state]=load_bitmap(broken_bits,broken_width,
				broken_height,bd->fg_pixel,bd->bg_pixel);
			spm[state]=load_bitmap(broken_bits,broken_width,
				broken_height,bd->fg_pixel,bd->sbg_pixel);
			break;
			case FS_ERROR:
			widths[state]=error_width;
			heights[state]=error_height;
			npm[state]=load_bitmap(error_bits,error_width,
				error_height,bd->fg_pixel,bd->bg_pixel);
			spm[state]=load_bitmap(error_bits,error_width,
				error_height,bd->fg_pixel,bd->sbg_pixel);
			break;
			case FS_VIEWABLE:
			case _NUM_FS_VALUES:
			dbg_trap("illegal file_state value");
			break;
		}
	}
	if(width) *width=widths[state];
	if(height) *height=heights[state];
	return selected?spm[state]:npm[state];
}

/*
 * Execute a file management action on the selected files.
 */
static int exec_file_proc(struct browser_data *bd, int proc)
{
	int res=0;
	long i;
	char *dest;
	char **names;
	long nfiles;
	char *path;
	
	dbg_assert(bd->nsel_files>0);
	names=calloc(bd->nsel_files,sizeof(char*));
	if(!names){
		reset_browser(bd);
		return errno;
	}
	
	for(i=0, nfiles=0; i<bd->nfiles; i++){
		if(!bd->files[i].selected) continue;
		names[nfiles]=malloc(bd->path_max+1);
		if(!names[nfiles]){
			while(nfiles--) free(names[nfiles]);
			free(names);
			reset_browser(bd);
			return ENOMEM;
		}
		snprintf(names[nfiles],bd->path_max,"%s/%s",bd->path,bd->files[i].name);
		nfiles++;
	}
	
	/* a copy of path is passed as client data to the file_proc_cb callback
	 * to determine which browser needs to be updated, since the caller may
	 * get destroyed after initiating a file proc */
	path=strdup(bd->path);
	
	switch(proc){
		case FPROC_COPY:
		dest=dir_select_dlg(bd->wshell,nlstr(DLG_MSGSET,SID_SELDESTDIR,
			"Select Destination"),bd->last_dest_dir);
		if(dest){
			if(bd->last_dest_dir) free(bd->last_dest_dir);
			bd->last_dest_dir=dest;
			res=copy_files(names,dest,nfiles,file_proc_cb,path);
		}
		break;
		
		case FPROC_MOVE:
		dest=dir_select_dlg(bd->wshell,nlstr(DLG_MSGSET,SID_SELDESTDIR,
			"Select Destination"),bd->last_dest_dir);
		if(dest){
			if(bd->last_dest_dir) free(bd->last_dest_dir);
			bd->last_dest_dir=dest;
			res=move_files(names,dest,nfiles,file_proc_cb,path);
		}
		break;
		
		case FPROC_DELETE:
		if(MBR_CONFIRM!=message_box(bd->wshell,MB_CONFIRM,
			nlstr(DLG_MSGSET,SID_UNLINK,"Delete"),
			nlstr(APP_MSGSET,SID_CMFUNLINK,
			"Selected files will be irrevocably deleted. Continue?"))){
			break;
		}
		res=delete_files(names,nfiles,file_proc_cb,path);
		break;
	}
	while(nfiles--) free(names[nfiles]);
	free(names);
	return res;			
}

/*
 * File management notification callback
 */
static void file_proc_cb(enum file_proc_id fpid, char *file_name,
	int status, void *client_data)
{
	char *path=(char*)client_data;
	struct browser_data *bd=browsers;

	/* check for browser instances displaying 'path' and update them */
	while(bd){
		if(!strcmp(path,bd->path)){
			switch(fpid){
				case FPROC_MOVE:
				case FPROC_DELETE:{
				long i;
				char *file_title;

				pthread_mutex_lock(&bd->data_mutex);
				file_title=strrchr(file_name,'/');
				file_title=(file_title)?file_title+1:file_name;
				if((i=find_file_entry(bd,file_title))<0){
					pthread_mutex_unlock(&bd->data_mutex);
					break;
				}
				if(bd->files[i].selected) bd->nsel_files--;
				if(bd->ifocus==i)
					set_focus(bd,(i<bd->nfiles-1)?i:(bd->nfiles-2));
				XmStringFree(bd->files[i].label);
				XDestroyImage(bd->files[i].image);
				if(i<bd->nfiles-1) memmove(&bd->files[i],&bd->files[i+1],
					sizeof(struct browser_file)*((bd->nfiles-1)-i));
				bd->nfiles--;
				if(!bd->nfiles){
					free(bd->files);
					bd->files=NULL;
					bd->ifocus=(-1);
					bd->nsel_files=0;
					bd->yoffset=0;
					bd->max_label_height=0;
					update_scroll_bar(bd);
					update_controls(bd);
				}
				pthread_mutex_unlock(&bd->data_mutex);
				
				update_scroll_bar(bd);
				update_status_msg(bd);
				XClearArea(app_inst.display,XtWindow(bd->wview),0,0,0,0,True);
				} break;
				
				case FPROC_FINISHED:
				free(path); /* allocated in exec_file_proc */
				break;
				
				case FPROC_COPY: /* not of interest */ break;
			};
		}
		bd=bd->next;
	}
}

/*
 * Update interval callback
 */
static void update_interval_cb(XtPointer client, XtIntervalId *iid)
{
	struct browser_data *bd=(struct browser_data*)client;
	bd->update_timer=None;
	
	dbg_assert(bd->path);
	
	if(XtAppPending(app_inst.context)&XtIMAlternateInput){
		bd->update_timer=XtAppAddTimeOut(
			app_inst.context,bd->refresh_int,
			update_interval_cb,(XtPointer)bd);
	}else{
		struct stat st;

		if(stat(bd->path,&st)){
			errno_message_box(bd->wshell,errno,
				nlstr(APP_MSGSET,SID_EREADDIR,
				"Error reading directory."),False);
			reset_browser(bd);
			return;
		}

		if(difftime(st.st_mtime,bd->dir_modtime)){
			launch_reader_thread(bd);
		}else{
			bd->update_timer=XtAppAddTimeOut(
				app_inst.context,bd->refresh_int,
				update_interval_cb,(XtPointer)bd);
		}
	}
}

/*
 * The window manager 'close' callback
 */
static void close_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
	struct browser_data *bd=(struct browser_data*)client_data;
	
	destroy_browser(bd);
	if(!app_inst.active_shells){
		dbg_printf("exit flag set in %s: %s()\n",__FILE__,__FUNCTION__);
		set_exit_flag(EXIT_SUCCESS);
	}
}

/*
 * 'File' menu callbacks
 */
static void browse_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	char *path, *home=bd->path;
	Widget browser=bd->wshell;
	
	if(!home){
		if(!(home=getenv("HOME"))) home=".";
	}
	path=dir_select_dlg(bd->wshell,nlstr(DLG_MSGSET,SID_OPENDIR,
		"Open Directory"),home);
	if(!path) return;
		
	if(bd->pinned) browser=get_browser(&init_app_res,NULL);
	if(browser){
		browse_path(browser,path,NULL);
	}else{
		message_box(bd->wshell,MB_ERROR_NB,NULL,nlstr(APP_MSGSET,SID_ENORES,
				"Not enough resources available for this task."));
	}
	free(path);	
}

/*
 * 'File' menu callbacks
 */
static void display_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	invoke_default_action(bd,bd->ifocus);
}

static void edit_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	char *path;
	
	dbg_assert(init_app_res.edit_cmd);
	dbg_assert(bd->ifocus>(-1));

	pthread_mutex_lock(&bd->data_mutex);	
	path=malloc(bd->path_max+2);
	if(!path){
		pthread_mutex_unlock(&bd->data_mutex);
		message_box(bd->wshell,MB_ERROR_NB,NULL,nlstr(APP_MSGSET,SID_ENORES,
			"Not enough resources available for this task."));
		return;
	}
	snprintf(path,bd->path_max,"%s/%s",bd->path,bd->files[bd->ifocus].name);
	pthread_mutex_unlock(&bd->data_mutex);

	if(!vfork()){
		if(execlp(init_app_res.edit_cmd,init_app_res.edit_cmd,path,NULL)){
			errno_message_box(bd->wshell,errno,init_app_res.edit_cmd,False);
		}
		_exit(0);
	}
	free(path);
}

/*
 * 'Edit' menu callbacks
 */

static void select_all_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	if(bd->nfiles) set_selection(bd,0,bd->nfiles-1,True);
}

static void select_none_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	if(bd->nfiles) clear_selection(bd);
}

static void invert_selection_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	if(bd->nfiles){
		long n=bd->nfiles;
		while(n--) toggle_selection(bd,n);
	}
}

static void copy_to_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	int res;
	res=exec_file_proc(bd,FPROC_COPY);
	if(res){
		errno_message_box(bd->wshell,res,nlstr(APP_MSGSET,SID_EFAILED,
			"The action couldn't be completed."),False);
	}
}

static void move_to_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	int res;
	res=exec_file_proc(bd,FPROC_MOVE);
	if(res){
		errno_message_box(bd->wshell,res,nlstr(APP_MSGSET,SID_EFAILED,
			"The action couldn't be completed."),False);
	}
}

static void rename_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	char *file_title;
	char *file_name;
	char *new_title;
	char *new_name;
	long ifile;
	int ret;
	struct stat st;
	Boolean pinned_state=bd->pinned;
	
	dbg_assert(bd->nsel_files==1);
	
	for(ifile=0; !bd->files[ifile].selected; ifile++);
	file_title=strdup(bd->files[ifile].name);
	file_name=malloc(bd->path_max+1);
	snprintf(file_name,bd->path_max,"%s/%s",bd->path,file_title);
	
	bd->pinned=True;
	new_title=rename_file_dlg(bd->wshell,file_title);
	bd->pinned=pinned_state;
	if(!new_title){
		free(file_title);
		free(file_name);
		bd->pinned=pinned_state;
		return;
	}
	new_name=malloc(strlen(bd->path)+strlen(new_title)+2);
	sprintf(new_name,"%s/%s",bd->path,new_title);
	
	if(stat(new_name,&st)==0){
		char *msg;
		char *prompt=nlstr(APP_MSGSET,SID_EFEXIST,"File already exists.");
		size_t msglen;
		msglen=strlen(prompt)+strlen(new_name)+2;
		msg=malloc(msglen);
		snprintf(msg,msglen,"%s\n%s",new_name,prompt);
		message_box(bd->wshell,MB_ERROR_NB,bd->title,msg);
		free(msg);
		bd->pinned=pinned_state;
		free(file_name);
		free(file_title);
		free(new_name);
		free(new_title);
		return;
	}
	ret=rename(file_name,new_name);
	if(ret){
		errno_message_box(bd->wshell,ret,nlstr(APP_MSGSET,SID_EFAILED,
			"The action couldn't be completed."),False);
	}else{
		if(bd->path_max<strlen(new_name)) bd->path_max=strlen(new_name);
		pthread_mutex_lock(&bd->data_mutex);
		free(bd->files[ifile].name);
		bd->files[ifile].name=strdup(new_title);
		XmStringFree(bd->files[ifile].label);
		bd->files[ifile].label=create_file_label(bd,new_title);
		bd->files[ifile].label_width=
			XmStringWidth(bd->render_table,bd->files[ifile].label);
		qsort(bd->files,bd->nfiles,
			sizeof(struct browser_file),file_sort_compare);
		pthread_mutex_unlock(&bd->data_mutex);
		XClearArea(app_inst.display,XtWindow(bd->wview),0,0,0,0,True);
	}
	free(file_name);
	free(file_title);
	free(new_name);
	free(new_title);
}

static void delete_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	int res;
	res=exec_file_proc(bd,FPROC_DELETE);
	if(res){
		errno_message_box(bd->wshell,res,nlstr(APP_MSGSET,SID_EFAILED,
			"The action couldn't be completed."),False);
	}
}

/*
 * View menu callbacks
 */
static void refresh_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	char *path = strdup(bd->path);
	
	load_path(bd, path);
	free(path);
}

static void pin_window_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	bd->pinned=((XmToggleButtonCallbackStruct*)call)->set;
}

static void new_window_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	bd=create_browser(&init_app_res);
	if(!bd) message_box(app_inst.session_shell,MB_ERROR,BASE_NAME,
				nlstr(APP_MSGSET,SID_ENORES,
				"Not enough resources available for this task."));
	map_shell_unpositioned(bd->wshell);
}

static void small_tiles_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	if(((XmToggleButtonCallbackStruct*)call)->set)
		set_tile_size(bd,TS_SMALL);
	XmToggleButtonGadgetSetState(w,True,False);
}

static void medium_tiles_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	if(((XmToggleButtonCallbackStruct*)call)->set)
		set_tile_size(bd,TS_MEDIUM);
	XmToggleButtonGadgetSetState(w,True,False);
}

static void large_tiles_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	if(((XmToggleButtonCallbackStruct*)call)->set)
		set_tile_size(bd,TS_LARGE);
	XmToggleButtonGadgetSetState(w,True,False);
}


static void show_subdirs_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	if(((XmToggleButtonCallbackStruct*)call)->set)
		XtManageChild(bd->wdlscroll);
	else
		XtUnmanageChild(bd->wdlscroll);
}

static void dirlist_cb(Widget w, XtPointer client, XtPointer call)
{
	XmListCallbackStruct *cbs = (XmListCallbackStruct*)call;
	struct browser_data *bd=(struct browser_data*)client;
	unsigned int pos = cbs->item_position - 1;
	char *new_path;
	
	if(cbs->reason != XmCR_DEFAULT_ACTION) return;
	
	new_path = malloc(strlen(bd->path) +
		strlen(bd->subdirs[pos]) + 2);
	if(!new_path) {
		message_box(app_inst.session_shell, MB_ERROR, BASE_NAME,
				nlstr(APP_MSGSET,SID_ENORES,
				"Not enough resources available for this task."));
		return;
	}
	sprintf(new_path, "%s/%s", bd->path, bd->subdirs[pos]);
	load_path(bd, new_path);
	free(new_path);
	
	XmProcessTraversal(bd->wdirlist, XmTRAVERSE_CURRENT);
}

static void show_dotfiles_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	bd->show_dot_files = (((XmToggleButtonCallbackStruct*)call)->set);

	if(bd->show_dot_files && bd->path) {
		launch_reader_thread(bd);
	} else if(bd->path) {
		char *tmp = strdup(bd->path);
		load_path(bd, tmp);
		free(tmp);
	}
}

/*
 * Help/Topics menu callback. Currently it just shows the manpage.
 */
#ifdef ENABLE_CDE
static void help_topics_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	DtActionArg args[1]={
		DtACTION_FILE,
		"ximaging"
	};
	DtActionInvoke(bd->wshell,"Dtmanpageview",args,1,
		NULL,NULL,NULL,False,NULL,NULL);
}
#endif /* ENABLE_CDE */

static void about_cb(Widget w, XtPointer client, XtPointer call)
{
	struct browser_data *bd=(struct browser_data*)client;
	display_about_dlgbox(bd->wshell);
}

/*
 * Create the menu bar
 */
static void create_browser_menubar(struct browser_data *bd)
{
	Arg args[2];
	
	static struct menu_item file_menu[]={
		{IT_PUSH,"fileMenu","_File",SID_BMFILE},
		{IT_PUSH,"browse","_Browse...",SID_BMBROWSE,browse_cb},
		{IT_SEP},
		{IT_PUSH,"display","_Display",SID_BMDISPLAY,display_cb},
		{IT_PUSH,"edit","_Edit",SID_BMFEDIT,edit_cb},
		{IT_SEP},
		{IT_PUSH,"close","_Close",SID_BMCLOSE,close_cb}
	};
	
	static struct menu_item edit_menu[]={
		{IT_PUSH,"editMenu","_Edit",SID_BMEDIT,NULL},
		{IT_PUSH,"selectAll","Select _All",SID_BMSELECTALL,select_all_cb},
		{IT_PUSH,"selectNone","Select _None",SID_BMSELECTNONE,select_none_cb},
		{IT_PUSH,"invertSelection","_Invert Selection",SID_BMINVERTSEL,
			invert_selection_cb},
		{IT_SEP},
		{IT_PUSH,"copyTo","_Copy to ...",SID_BMCOPYTO,copy_to_cb},
		{IT_PUSH,"moveTo","_Move to ...",SID_BMMOVETO,move_to_cb},
		{IT_PUSH,"rename","_Rename",SID_BMRENAME,rename_cb},
		{IT_SEP},
		{IT_PUSH,"delete","_Delete",SID_BMDELETE,delete_cb}
	};
	
	static struct menu_item view_menu[]={
		{IT_PUSH,"viewMenu","_View",SID_BMVIEW},
		{IT_PUSH,"refresh","_Refresh",SID_BMREFRESH,refresh_cb},
		{IT_SEP},
		{IT_RADIO,"smallTiles","_Small Tiles",SID_BMSMALL,small_tiles_cb},
		{IT_RADIO,"mediumTiles","_Medium Tiles",SID_BMMEDIUM,medium_tiles_cb},
		{IT_RADIO,"largeTiles","_Large Tiles",SID_BMLARGE,large_tiles_cb},
		{IT_SEP},
		{IT_TOGGLE,"directories","_Directories",SID_BMPIN,show_subdirs_cb},
		{IT_TOGGLE,"dotFiles","Dot _Files",SID_BMDOTFILES,show_dotfiles_cb}
	};
	
	static struct menu_item window_menu[] = {
		{IT_PUSH,"windowMenu","_Window",SID_BMWINDOW},
		{IT_TOGGLE,"pinThis","_Pin This",SID_BMPIN,pin_window_cb},
		{IT_PUSH,"openNew","_Open New",SID_BMNEWWINDOW,new_window_cb}
	};

	static struct menu_item help_menu[]={
		{IT_PUSH,"helpMenu","_Help",SID_BMHELP,NULL},
		#ifdef ENABLE_CDE
		{IT_PUSH,"topics","_Manual",SID_BMTOPICS,help_topics_cb},
		#endif /* ENABLE_CDE */
		{IT_PUSH,"about","_About",SID_BMABOUT,about_cb}
	};
	
	XtSetArg(args[0], XmNshadowThickness, 1);
	bd->wmenubar=XmCreateMenuBar(bd->wmain, "menuBar", args, 1);
	
	create_pulldown(bd->wmenubar,file_menu,
		(sizeof(file_menu)/sizeof(struct menu_item)),
		bd,BROWSER_MENU_MSGSET,False);
	create_pulldown(bd->wmenubar,edit_menu,
		(sizeof(edit_menu)/sizeof(struct menu_item)),
		bd,BROWSER_MENU_MSGSET,False);
	create_pulldown(bd->wmenubar,view_menu,
		(sizeof(view_menu)/sizeof(struct menu_item)),
		bd,BROWSER_MENU_MSGSET,False);
	create_pulldown(bd->wmenubar,window_menu,
		(sizeof(window_menu)/sizeof(struct menu_item)),
		bd,BROWSER_MENU_MSGSET,False);
	create_pulldown(bd->wmenubar,help_menu,
		(sizeof(help_menu)/sizeof(struct menu_item)),
		bd,BROWSER_MENU_MSGSET,True);
}

/*
 * Create the popup menu for tile right click. Basically a miniature
 * version of the 'Edit' pulldown.
 */
static void create_tile_popup(struct browser_data *bd)
{
	static struct menu_item items[]={
		{IT_PUSH,"copyTo","_Copy to ...",SID_BMCOPYTO,copy_to_cb},
		{IT_PUSH,"moveTo","_Move to ...",SID_BMMOVETO,move_to_cb},
		{IT_PUSH,"rename","_Rename",SID_BMRENAME,rename_cb},
		{IT_SEP},
		{IT_PUSH,"delete","_Delete",SID_BMDELETE,delete_cb}
	};
	bd->wpopup=create_popup(bd->wview,"browserPopup",items,
		(sizeof(items)/sizeof(struct menu_item)),bd,BROWSER_MENU_MSGSET);
}

/* Menu item widget retrieval with validation */
static Widget get_menu_item(struct browser_data *bd, const char *name)
{
	Widget w;
	w=XtNameToWidget(bd->wmenubar,name);
	dbg_assertmsg(w!=None,"menu item %s doesn't exist\n",name);
	return w;
}

/*
 * Callback invoked by the path box widget on path change
 */
static void navbar_change_cb(const char *path, void *cb_data)
{
	struct browser_data *bd=(struct browser_data*)cb_data;
	char *tmp=strdup(path); /* load_path may call reset_browser, which
							 * in turn resets the path box, so we want
							 * to keep a local copy of the path string */
	load_path(bd,tmp);
	free(tmp);
}

/*
 * Primary selection converter.
 * Returns full path to the focused file, if any, to the requestor.
 */
static Boolean convert_selection_proc(Widget w,
	Atom *sel, Atom *tgt, Atom *type_ret, XtPointer *val_ret,
	unsigned long *len_ret, int *fmt_ret)
{
	struct browser_data *bd = browsers;
	
	while(bd) {
		if(bd->wshell == w) break;
		bd = bd->next;
	}
	if(!bd || !bd->nfiles || (bd->ifocus < 0)) return False;
	
	if(*tgt == app_inst.XaTEXT || *tgt == XA_STRING) {
		unsigned long len;
		char *data;
		
		len = strlen(bd->path) + strlen(bd->files[bd->ifocus].name) + 2;
		
		data = XtMalloc(len); /* Xt will XtFree it when done */
		if(!data) {
			warning_msg("Failed to allocate selection data buffer");
			return False;
		}

		sprintf(data, "%s/%s", bd->path, bd->files[bd->ifocus].name);
		
		*type_ret = *tgt;
		*fmt_ret = 8;
		*len_ret = len - 1;
		*val_ret = data;
		return True;		
	}
	return False;
}

/*
 * Called on selection ownership change.
 * Redraws selected tile, if any, and resets PRIMARY ownership state.
 */
static void lose_selection_proc(Widget wshell, Atom *sel)
{
	struct browser_data *bd = browsers;

	while(bd) {
		if(bd->wshell == wshell) break;
		bd = bd->next;
	}
	if(!bd || !bd->nfiles || (bd->ifocus < 0)) return;

	bd->owns_primary = False;
	redraw_tile(bd, bd->ifocus);
}

/*
 * Retrieve a browser instance, create one if necessary.
 */
Widget get_browser(const struct app_resources *res, Tt_message req_msg)
{
	struct browser_data *bd=browsers;
	
	while(bd){
		if(!bd->pinned) break;
		bd=bd->next;
	}
	
	if(bd){
		if(bd->state&(BSF_READING|BSF_LOADING|BSF_READY))
			reset_browser(bd);
		raise_and_focus(bd->wshell);
	}else{
		bd=create_browser(res);
		if(!bd){
			message_box(app_inst.session_shell,MB_ERROR,BASE_NAME,
				nlstr(APP_MSGSET,SID_ENORES,
				"Not enough resources available for this task."));
			#ifdef ENABLE_CDE
			if(req_msg) ttmedia_load_reply(req_msg,NULL,0,True);
			#endif /* ENABLE_CDE */
			return None;
		}
	}
	#ifdef ENABLE_CDE
	if(req_msg) bd->tt_disp_req=req_msg;
	#endif /* ENABLE_CDE */
	return bd->wshell;
}

/*
 * Public interface to load_path
 */
Boolean browse_path(Widget wshell, const char *path, Tt_message req_msg)
{
	struct browser_data *bd;
	
	bd=get_browser_inst_data(wshell);
	dbg_assert(bd);
	
	#ifdef ENABLE_CDE
	if(req_msg) bd->tt_disp_req=req_msg;
	#endif /* ENABLE_CDE */

	load_path(bd,path);
	
	return True;
}

/*
 * Destroy browser by ToolTalk Quit request message.
 */
#ifdef ENABLE_CDE
Boolean handle_browser_ttquit(Tt_message req_msg)
{
	struct browser_data *bd=browsers;
	char *orig_req_id;
	char *disp_req_id;
		
	dbg_assert(req_msg);
	disp_req_id=tt_message_id(req_msg);
	orig_req_id=tt_message_arg_val(req_msg,2);
	if(!strlen(orig_req_id)){
		tt_free(orig_req_id);
		return False;
	}
	while(bd){
		if(!strcmp(orig_req_id,disp_req_id)) break;
		bd=bd->next;
	}
	tt_free(orig_req_id);
	tt_free(disp_req_id);
	if(!bd) return False;
	bd->tt_quit_req=req_msg;
	destroy_browser(bd);
	return True;
}
#endif /* ENABLE_CDE */
