/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

/*
 * Default XRDB configuration.
 */

#include <stdlib.h>
#include "const.h"

char *fallback_res[]={

	/* window properties */
	"ximagingBrowser.width:550",
	"ximagingBrowser.height:400",
	"ximagingViewer.width:550",
	"ximagingViewer.height:400",

	"ximagingBrowser.title:" BASE_TITLE,
	"ximagingBrowser.iconName:" BASE_TITLE,
	"ximagingViewer.title:" BASE_TITLE,
	"ximagingViewer.iconName:" BASE_TITLE,
	
	/* dialogs */
	"*XmFileSelectionBox.pathMode: PATH_MODE_RELATIVE",
	"*XmFileSelectionBox.fileFilterStyle: FILTER_HIDDEN_FILES",

	/* accelerators */
	"*fileMenu.open.accelerator: Ctrl<Key>o",
	"*fileMenu.open.acceleratorText: Ctrl+O",
	"*fileMenu.nextFile.accelerator: <Key>osfPageDown",
	"*fileMenu.nextFile.acceleratorText: PgDn",
	"*fileMenu.previousFile.accelerator: <Key>osfPageUp",
	"*fileMenu.previousFile.acceleratorText: PgUp",
	"*fileMenu.close.acceleratorText: Alt+F4",
	"*viewMenu.nextPage.accelerator: Ctrl<Key>osfPageDown",
	"*viewMenu.nextPage.acceleratorText: Ctrl+PgDn",
	"*viewMenu.previousPage.accelerator: Ctrl<Key>osfPageUp",
	"*viewMenu.previousPage.acceleratorText: Ctrl+PgUp",
	"XImagingBrowser*fileMenu.browse.accelerator: Ctrl<Key>b",
	"XImagingBrowser*fileMenu.browse.acceleratorText: Ctrl+B",
	"XImagingBrowser*fileMenu.edit.accelerator: Ctrl<Key>e",
	"XImagingBrowser*fileMenu.edit.acceleratorText: Ctrl+E",
	"XImagingViewer*fileMenu.browseHere.accelerator: <Key>Return",
	"XImagingViewer*fileMenu.browseHere.acceleratorText: Enter",
	"XImagingViewer*fileMenu.edit.accelerator: Ctrl<Key>Return",
	"XImagingViewer*fileMenu.edit.acceleratorText: Ctrl+Enter",
	"XImagingViewer*fileMenu.browse.accelerator: Ctrl<Key>B",
	"XImagingViewer*fileMenu.browse.acceleratorText: Ctrl+B",
	"*viewMenu.refresh.accelerator: <Key>F5",
	"*viewMenu.refresh.acceleratorText: F5",
	"*windowMenu.openNew.accelerator: Ctrl<Key>N",
	"*windowMenu.openNew.acceleratorText: Ctrl+N",
	"*windowMenu.pinThis.accelerator: <Key>F2",
	"*windowMenu.pinThis.acceleratorText: F2",
	"XImagingBrowser*viewMenu.goUp.acceleratorText: Backspace",
	"XImagingBrowser*viewMenu.smallTiles.accelerator: <Key>KP_Subtract",
	"XImagingBrowser*viewMenu.smallTiles.acceleratorText: Kp: Subtract",
	"XImagingBrowser*viewMenu.largeTiles.accelerator: <Key>KP_Add",
	"XImagingBrowser*viewMenu.largeTiles.acceleratorText: Kp: Add",
	"XImagingBrowser*viewMenu.mediumTiles.accelerator: <Key>KP_Enter",
	"XImagingBrowser*viewMenu.mediumTiles.acceleratorText: Kp: Enter",
	"XImagingBrowser*viewMenu.directories.accelerator: Ctrl<Key>d",
	"XImagingBrowser*viewMenu.directories.acceleratorText: Ctrl+D",
	"XImagingBrowser*viewMenu.dotFiles.accelerator: Ctrl<Key>h",
	"XImagingBrowser*viewMenu.dotFiles.acceleratorText: Ctrl+H",
	"*zoomMenu.zoomIn.accelerator: <Key>KP_Add",
	"*zoomMenu.zoomIn.acceleratorText: Kp: Add",
	"*zoomMenu.zoomOut.accelerator: <Key>KP_Subtract",
	"*zoomMenu.zoomOut.acceleratorText: Kp: Subtract",
	"*zoomMenu.zoomFit.accelerator: <Key>KP_Enter",
	"*zoomMenu.zoomFit.acceleratorText:Kp: Enter",
	"*zoomMenu.zoomNone.accelerator: <Key>KP_0",
	"*zoomMenu.zoomNone.acceleratorText: Kp: Ins",
	"*rotateMenu.flipVertically.accelerator: <Key>KP_8",
	"*rotateMenu.flipVertically.acceleratorText: Kp: Up",
	"*rotateMenu.flipHorizontally.accelerator: <Key>KP_2",
	"*rotateMenu.flipHorizontally.acceleratorText: Kp: Down",
	"*rotateMenu.rotateLeft.accelerator: <Key>KP_4",
	"*rotateMenu.rotateLeft.acceleratorText: Kp: Left",
	"*rotateMenu.rotateRight.accelerator: <Key>KP_6",
	"*rotateMenu.rotateRight.acceleratorText: Kp: Right",
	"*rotateMenu.rotateLock.accelerator: <Key>KP_1",
	"*rotateMenu.rotateLock.acceleratorText: Kp: End",
	"*rotateMenu.rotateReset.accelerator: <Key>KP_Delete",
	"*rotateMenu.rotateReset.acceleratorText: Kp: Del",
	"*editMenu.copy.accelerator: Ctrl<Key>y",
	"*editMenu.copy.acceleratorText: Ctrl+Y",
	"*editMenu.rename.accelerator: Ctrl<Key>r",
	"*editMenu.rename.acceleratorText: Ctrl+R",
	"*editMenu.copyTo.accelerator: Ctrl<Key>C",
	"*editMenu.copyTo.acceleratorText: Ctrl+C",
	"*editMenu.moveTo.accelerator: Ctrl<Key>M",
	"*editMenu.moveTo.acceleratorText: Ctrl+M",
	"*editMenu.delete.accelerator: Ctrl<Key>Delete",
	"*editMenu.delete.acceleratorText: Ctrl+Del",
	"*editMenu.selectAll.accelerator: Ctrl<Key>a",
	"*editMenu.selectAll.acceleratorText: Ctrl+A",
	"*editMenu.selectNone.accelerator: Ctrl<Key>n",
	"*editMenu.selectNone.acceleratorText: Ctrl+N",
	"*editMenu.invertSelection.accelerator: Ctrl<Key>i",
	"*editMenu.invertSelection.acceleratorText: Ctrl+I",
	"*helpMenu.topics.accelerator: <Key>F1",
	"*helpMenu.topics.acceleratorText: F1",

	NULL
};
