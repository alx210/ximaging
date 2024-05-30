/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#ifndef VIEWER_H
#define VIEWER_H

#include <Xm/Xm.h>

#ifdef ENABLE_CDE
#include <Tt/tt_c.h>
#else
#ifndef TT_MESSAGE_DEFINED
#define TT_MESSAGE_DEFINED
typedef void *Tt_message;
#endif
#endif /* ENABLE_CDE */

/*
 * Retrieve the first unpinned viewer, create a new instance if none available.
 * 'req_msg' is optional and associates the instance with a ToolTalk request.
 * Returns its shell widget handle, or None on failure.
 */
Widget get_viewer(const struct app_resources*, Tt_message req_msg);

/*
 * Load and display the image specified by 'file_name'.
 * 'req_msg' is optional and associates the instance with a ToolTalk request.
 * Returns True on success.
 */
Boolean display_image(Widget, const char *file_name,
	const char *force_suffix, Tt_message req_msg);

#ifdef ENABLE_CDE
/*
 * Handle the ttmedia Quit message.
 * Destroys the instance that 'req_msg' is targeted at and returns True.
 * Returns false if no matching instance was found.
 */
Boolean handle_viewer_ttquit(Tt_message req_msg);
#endif /* ENABLE_CDE */

#endif /* VIEWER_H */
