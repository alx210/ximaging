/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
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
	"browser.width:550",
	"browser.height:400",
	"viewer.width:550",
	"viewer.height:400",

	"browser.title:" BASE_TITLE,
	"browser.iconName:" BASE_TITLE,
	"viewer.title:" BASE_TITLE,
	"viewer.iconName:" BASE_TITLE,
	
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
	"browser*fileMenu.browse.accelerator: Ctrl<Key>b",
	"browser*fileMenu.browse.acceleratorText: Ctrl+B",
	"browser*fileMenu.edit.accelerator: Ctrl<Key>e",
	"browser*fileMenu.edit.acceleratorText: Ctrl+E",
	"viewer*fileMenu.browseHere.accelerator: <Key>Return",
	"viewer*fileMenu.browseHere.acceleratorText: Enter",
	"viewer*fileMenu.edit.accelerator: Ctrl<Key>Return",
	"viewer*fileMenu.edit.acceleratorText: Ctrl+Enter",
	"viewer*fileMenu.browse.accelerator: Ctrl<Key>B",
	"viewer*fileMenu.browse.acceleratorText: Ctrl+B",
	"*viewMenu.refresh.accelerator: <Key>F5",
	"*viewMenu.refresh.acceleratorText: F5",
	"*viewMenu.newWindow.accelerator: Ctrl<Key>N",
	"*viewMenu.newWindow.acceleratorText: Ctrl+N",
	"*viewMenu.pinWindow.accelerator: <Key>F2",
	"*viewMenu.pinWindow.acceleratorText: F2",
	"browser*viewMenu.smallTiles.accelerator: <Key>KP_Subtract",
	"browser*viewMenu.smallTiles.acceleratorText: Kp: Subtract",
	"browser*viewMenu.largeTiles.accelerator: <Key>KP_Add",
	"browser*viewMenu.largeTiles.acceleratorText: Kp: Add",
	"browser*viewMenu.mediumTiles.accelerator: <Key>KP_Enter",
	"browser*viewMenu.mediumTiles.acceleratorText: Kp: Enter",
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
	"*editMenu.delete.accelerator: <Key>Delete",
	"*editMenu.delete.acceleratorText: Del",
	"*editMenu.selectAll.accelerator: Ctrl<Key>a",
	"*editMenu.selectAll.acceleratorText: Ctrl+A",
	"*editMenu.selectNone.accelerator: Ctrl<Key>n",
	"*editMenu.selectNone.acceleratorText: Ctrl+N",
	"*editMenu.invertSelection.accelerator: Ctrl<Key>i",
	"*editMenu.invertSelection.acceleratorText: Ctrl+I",
	"*helpMenu.topics.accelerator: <Key>F1",
	"*helpMenu.topics.acceleratorText: F1",
	
	/* If built without CDE support, set some Motif resources that
	 * are set by default on CDE */
	#ifndef ENABLE_CDE
	"*XmFileSelectionBox.fileFilterStyle: XmFILTER_HIDDEN_FILES",
	"*XmFileSelectionBox.pathMode: XmPATH_MODE_RELATIVE",
	"*XmFileSelectionBox.resizePolicy: XmRESIZE_GROW",
	"*enableThinThickness: True",
	"*enableBtn1Transfer: button2_transfer",
	"*enableButtonTab: True",
	"*enableDefaultButton: True",
	"*enableEtchedInMenu: True",
	"*enableMenuInCascade: True",
	"*enableMultiKeyBindings: True",
	"*enableToggleColor: True",
	"*enableToggleVisual: True",
	"*enableDragIcon: True",
	"*enableCDEColorFactors: True",
	"*XmText*Translations:#override\n "
		"Ctrl<Key>u:delete-to-start-of-line()\n "
		"Ctrl<Key>k:delete-to-end-of-line()\n "
		"Ctrl<Key>a:beginning-of-line()\n "
		"Ctrl<Key>e:end-of-line()\n "
		"Ctrl<Key>p:process-up()\n "
		"Ctrl<Key>b:backward-character()\n "
		"Ctrl<Key>n:process-down()\n "
		"Ctrl<Key>f:forward-character()",
	"*XmTextField*Translations:#override\n "
		"Ctrl<Key>u:delete-to-start-of-line()\n "
		"Ctrl<Key>k:delete-to-end-of-line()\n "
		"Ctrl<Key>a:beginning-of-line()\n "
		"Ctrl<Key>e:end-of-line()\n "
		"Ctrl<Key>b:backward-character()\n "
		"Ctrl<Key>f:forward-character()",
	"*renderTable: variable",
	"*renderTable.variable.fontType: FONT_IS_XFT",
	"*renderTable.variable.fontName: Sans",
	"*renderTable.variable.fontSize: 10",
	#endif /* ENABLE_CDE */

	NULL
};
