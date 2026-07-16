/*
 * KuraWindow.h
 * Kura - Password Manager for Haiku
 * Distributed under the terms of the MIT License.
 * Copyright 2026 Il Felice.
 *
 * Main application window. Three-pane layout:
 *   Left:   GroupListView (sidebar with group/folder tree)
 *   Middle: EntryListView (ColumnListView of entries)
 *   Right:  DetailView (selected entry details)
 *
 * Manages the database lifecycle: load, save, lock/unlock.
 * Coordinates communication between the three panes.
 */
#ifndef KURA_WINDOW_H
#define KURA_WINDOW_H

#include <MenuBar.h>
#include <Window.h>

#include "KuraCrypto.h"
#include "KuraDatabase.h"
#include "KuraSettings.h"
#include "UnlockWindow.h"

class BFilePanel;
class BMenuItem;
class BMessageRunner;
class BSplitView;
class DetailView;

namespace BPrivate {
	class BToolBar;
}

class EntryListView;
class GroupListView;
class StatusBar;


class KuraWindow : public BWindow {
public:
						KuraWindow();
	virtual				~KuraWindow();

	virtual void		MessageReceived(BMessage* message);
	virtual void		Minimize(bool minimize);
	virtual void		DispatchMessage(BMessage* message,
							BHandler* handler);
	virtual bool		QuitRequested();

private:
	// UI setup
	void				_BuildMenu();
	void				_BuildLayout();
	void				_UpdateStatusBar();
	void				_UpdateTitle();
	void				_UpdateMenus();
	void				_UpdateRecentMenu();
	void				_RecordRecentFile();

	// Database operations
	void				_NewDatabase();
	void				_OpenDatabase();
	void				_ImportCsv();
	void				_SaveDatabase();
	void				_SaveDatabaseAs();
	void				_Lock(bool showUnlockDialog,
							bool saveChanges = true);
	void				_UnlockDatabase(const BString& password,
							int32 mode, const BString& current);
	void				_ShowUnlockDialog(unlock_mode mode);

	// Show the unlock dialog only when there is a database to
	// unlock (or a pending new-database path); no-op otherwise
	void				_OfferUnlock();
	enum close_choice {
		CLOSE_CANCELLED,
		CLOSE_SAVED,		// saved, or nothing to save
		CLOSE_DISCARDED,
	};
	close_choice		_ConfirmClose(const char* question);
	bool				_DatabaseFileExists() const;
	bool				_DialogWindowOpen() const;

	// Entry operations
	void				_NewEntry();
	void				_EditEntry();
	void				_DeleteEntry();
	void				_DuplicateEntry();
	void				_CopyUsername();
	void				_CopyPassword();

	// Group operations
	void				_NewGroup();
	void				_EditGroup();
	void				_DeleteGroup();

	// Refresh UI from database state
	void				_RefreshAll();
	void				_ShowSelectedEntry();

	// Auto-lock
	void				_ResetAutoLock();
	void				_StopAutoLock();
	bigtime_t			_AutoLockTimeout() const;

	// Apply the configurable options to their consumers
	void				_ApplyOptions();

	// Menu bar and toolbar
	BMenuBar*			fMenuBar;
	BPrivate::BToolBar*	fToolBar;
	BMenuItem*			fSaveItem;
	BMenuItem*			fSaveAsItem;
	BMenuItem*			fImportItem;
	BMenuItem*			fCloseItem;
	BMenuItem*			fLockItem;
	BMenuItem*			fChangePwItem;
	BMenu*				fGroupsMenu;
	BMenu*				fEntriesMenu;
	BMenu*				fRecentMenu;

	// Three panes
	GroupListView*		fGroupListView;
	EntryListView*		fEntryListView;
	DetailView*			fDetailView;

	// Split views (for persisting positions)
	BSplitView*			fMainSplit;
	BSplitView*			fRightSplit;

	// Status bar
	StatusBar*			fStatusBar;

	// File panels (created lazily)
	BFilePanel*			fOpenPanel;
	BFilePanel*			fNewPanel;
	BFilePanel*			fSaveAsPanel;
	BFilePanel*			fImportPanel;

	// Settings
	KuraSettings		fSettings;

	// Data
	KuraDatabase		fDatabase;
	KuraCrypto			fCrypto;
	BString				fDbPath;
	BString				fPassword;
	bool				fIsLocked;
	bool				fIsNewDb;
	bool				fUnlockShowing;
	bool				fSettingsShowing;
	BMessenger			fSettingsMessenger;

	// Auto-lock timer
	BMessageRunner*		fAutoLockRunner;
	bigtime_t			fLastActivity;
};

#endif // KURA_WINDOW_H
