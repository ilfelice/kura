/*
 * KuraWindow.cpp
 * Kura - Password Manager for Haiku
 * Distributed under the terms of the MIT License.
 * Copyright 2026 Il Felice.
 *
 * Main window implementation.
 */

#include "KuraWindow.h"
#include "KuraClipboard.h"
#include "KuraDefs.h"
#include "KuraUtils.h"
#include "DetailView.h"
#include "EntryEditWindow.h"
#include "EntryListView.h"
#include "GroupEditWindow.h"
#include "GroupListView.h"
#include "AboutWindow.h"
#include "KuraCsvImport.h"
#include "SettingsWindow.h"
#include "StatusBar.h"
#include "PasswordGeneratorWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <ControlLook.h>
#include <IconUtils.h>
#include <Resources.h>
#include <private/shared/ToolBar.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <FindDirectory.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <Path.h>
#include <Screen.h>
#include <SplitView.h>

#include <cstring>
#include <cstdio>


// A single 1px border line spanning the full window width below
// the toolbar. Being one view, it cannot be interrupted by the
// pane splitter - the earlier approach of joining the panes' own
// top borders across the splitter gap could not be made seamless.
class BorderView : public BView {
public:
	BorderView()
		:
		BView("toolbarBorder", B_WILL_DRAW)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	}

	// Layout sizes are BRect-style inclusive: height 0 = one pixel
	virtual BSize MinSize()
	{
		return BSize(0, 0);
	}

	virtual BSize MaxSize()
	{
		return BSize(B_SIZE_UNLIMITED, 0);
	}

	virtual BSize PreferredSize()
	{
		return BSize(B_SIZE_UNLIMITED, 0);
	}

	virtual void Draw(BRect updateRect)
	{
		rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
		SetHighColor(tint_color(base, B_DARKEN_2_TINT));
		StrokeLine(Bounds().LeftTop(),
			BPoint(Bounds().right, Bounds().top));
	}
};


// Load a named vector icon from the app resources, rendered at the
// toolbar icon size. Caller owns the returned bitmap.
static BBitmap*
_LoadToolBarIcon(const char* name)
{
	if (be_app == NULL)
		return NULL;
	BResources* resources = be_app->AppResources();
	if (resources == NULL)
		return NULL;

	size_t dataSize = 0;
	const void* data = resources->LoadResource(B_VECTOR_ICON_TYPE,
		name, &dataSize);
	if (data == NULL || dataSize == 0)
		return NULL;

	float size = be_control_look->ComposeIconSize(24).Width() + 1;
	BBitmap* bitmap = new(std::nothrow) BBitmap(
		BRect(0, 0, size - 1, size - 1), B_RGBA32);
	if (bitmap == NULL)
		return NULL;

	if (BIconUtils::GetVectorIcon((const uint8*)data, dataSize,
			bitmap) != B_OK) {
		delete bitmap;
		return NULL;
	}
	return bitmap;
}


KuraWindow::KuraWindow()
	:
	BWindow(BRect(80, 80, 980, 580), "Kura",
		B_DOCUMENT_WINDOW, B_AUTO_UPDATE_SIZE_LIMITS),
	fMainSplit(NULL),
	fRightSplit(NULL),
	fOpenPanel(NULL),
	fNewPanel(NULL),
	fSaveAsPanel(NULL),
	fImportPanel(NULL),
	fIsLocked(true),
	fIsNewDb(true),
	fUnlockShowing(false),
	fSettingsShowing(false),
	fAutoLockRunner(NULL),
	fLastActivity(system_time())
{
	// Load settings before building layout
	fSettings.Load();

	_BuildMenu();
	_BuildLayout();

	// Apply the persisted options to their consumers
	_ApplyOptions();

	// Restore window frame or center on screen
	if (fSettings.HasWindowFrame()) {
		BRect frame = fSettings.WindowFrame();
		// Validate the frame is on-screen
		BRect screenFrame = BScreen().Frame();
		if (screenFrame.Contains(BPoint(frame.left, frame.top)))
			MoveTo(frame.LeftTop());
		if (frame.Width() > 100 && frame.Height() > 100)
			ResizeTo(frame.Width(), frame.Height());
	} else {
		CenterOnScreen();
	}

	// Restore split positions and column state
	fSettings.RestoreSplitWeights("mainSplit", fMainSplit);
	fSettings.RestoreSplitWeights("rightSplit", fRightSplit);
	fSettings.RestoreColumnState("entryList",
		fEntryListView->ListView());

	// Cmd+F focuses the search field
	AddShortcut('F', B_COMMAND_KEY, new BMessage(kMsgFocusSearch));

	// Make sure the settings directory exists
	BPath settingsPath;
	find_directory(B_USER_SETTINGS_DIRECTORY, &settingsPath);
	settingsPath.Append(kSettingsDir);
	BDirectory dir;
	dir.CreateDirectory(settingsPath.Path(), NULL);

	// Offer to unlock the most recently used database if it still
	// exists. Otherwise start with no database loaded: the user
	// can open or create one from the menu or toolbar. (No dialog
	// is forced on the user, and cancelling one never quits.)
	BString lastFile = fSettings.RecentFileAt(0);
	if (lastFile.Length() > 0 && BEntry(lastFile.String()).Exists()) {
		fDbPath = lastFile;
		fIsNewDb = false;
		_ShowUnlockDialog(UNLOCK_OPEN);
	} else {
		fDbPath = "";
		fIsNewDb = true;
	}

	_UpdateTitle();
	_UpdateMenus();
	_UpdateRecentMenu();
	_UpdateStatusBar();
}


KuraWindow::~KuraWindow()
{
	_StopAutoLock();

	delete fOpenPanel;
	delete fNewPanel;
	delete fSaveAsPanel;
	delete fImportPanel;

	scrub_string(fPassword);
}


void
KuraWindow::_BuildMenu()
{
	fMenuBar = new BMenuBar("menuBar");

	// Database menu
	BMenu* dbMenu = new BMenu("Database");
	dbMenu->AddItem(new BMenuItem("New database" B_UTF8_ELLIPSIS,
		new BMessage(kMsgNewDatabase), 'N', B_SHIFT_KEY));
	dbMenu->AddItem(new BMenuItem("Open database" B_UTF8_ELLIPSIS,
		new BMessage(kMsgOpenDatabase), 'O'));
	fRecentMenu = new BMenu("Open recent");
	dbMenu->AddItem(fRecentMenu);
	fCloseItem = new BMenuItem("Close database",
		new BMessage(kMsgCloseDatabase), 'W');
	dbMenu->AddItem(fCloseItem);
	dbMenu->AddSeparatorItem();
	fSaveItem = new BMenuItem("Save",
		new BMessage(kMsgSaveDatabase), 'S');
	dbMenu->AddItem(fSaveItem);
	fSaveAsItem = new BMenuItem("Save as" B_UTF8_ELLIPSIS,
		new BMessage(kMsgSaveDatabaseAs), 'S', B_SHIFT_KEY);
	dbMenu->AddItem(fSaveAsItem);
	dbMenu->AddSeparatorItem();
	fImportItem = new BMenuItem("Import KeePass CSV" B_UTF8_ELLIPSIS,
		new BMessage(kMsgImportCsv));
	dbMenu->AddItem(fImportItem);
	dbMenu->AddSeparatorItem();
	fLockItem = new BMenuItem("Lock database",
		new BMessage(kMsgLockDatabase), 'L');
	dbMenu->AddItem(fLockItem);
	fChangePwItem = new BMenuItem("Change password" B_UTF8_ELLIPSIS,
		new BMessage(kMsgChangePassword));
	dbMenu->AddItem(fChangePwItem);
	dbMenu->AddSeparatorItem();
	dbMenu->AddItem(new BMenuItem("Quit",
		new BMessage(B_QUIT_REQUESTED), 'Q'));
	fMenuBar->AddItem(dbMenu);

	// Groups menu
	fGroupsMenu = new BMenu("Groups");
	fGroupsMenu->AddItem(new BMenuItem("New group" B_UTF8_ELLIPSIS,
		new BMessage(kMsgNewGroup)));
	fGroupsMenu->AddItem(new BMenuItem("Edit group" B_UTF8_ELLIPSIS,
		new BMessage(kMsgEditGroup)));
	fGroupsMenu->AddItem(new BMenuItem("Delete group",
		new BMessage(kMsgDeleteGroup)));
	fMenuBar->AddItem(fGroupsMenu);

	// Entries menu
	fEntriesMenu = new BMenu("Entries");
	fEntriesMenu->AddItem(new BMenuItem("New entry" B_UTF8_ELLIPSIS,
		new BMessage(kMsgNewEntry), 'N'));
	fEntriesMenu->AddItem(new BMenuItem("Edit entry" B_UTF8_ELLIPSIS,
		new BMessage(kMsgEditEntry), 'E'));
	fEntriesMenu->AddItem(new BMenuItem("Duplicate entry",
		new BMessage(kMsgDuplicateEntry), 'D'));
	fEntriesMenu->AddItem(new BMenuItem("Delete entry",
		new BMessage(kMsgDeleteEntry)));
	fEntriesMenu->AddSeparatorItem();
	fEntriesMenu->AddItem(new BMenuItem("Copy username",
		new BMessage(kMsgCopyUsername), 'U'));
	fEntriesMenu->AddItem(new BMenuItem("Copy password",
		new BMessage(kMsgCopyPassword), 'C'));
	fEntriesMenu->AddItem(new BMenuItem("Open URL",
		new BMessage(kMsgOpenUrl), 'B'));
	fMenuBar->AddItem(fEntriesMenu);

	// Tools menu
	BMenu* toolsMenu = new BMenu("Tools");
	toolsMenu->AddItem(new BMenuItem("Password generator" B_UTF8_ELLIPSIS,
		new BMessage(kMsgPasswordGenerator), 'G'));
	toolsMenu->AddSeparatorItem();
	toolsMenu->AddItem(new BMenuItem("Settings" B_UTF8_ELLIPSIS,
		new BMessage(kMsgShowSettings), ','));
	fMenuBar->AddItem(toolsMenu);

	// Help menu
	BMenu* helpMenu = new BMenu("Help");
	helpMenu->AddItem(new BMenuItem("About Kura" B_UTF8_ELLIPSIS,
		new BMessage(B_ABOUT_REQUESTED)));
	fMenuBar->AddItem(helpMenu);
}


void
KuraWindow::_BuildLayout()
{
	fGroupListView = new GroupListView();
	fEntryListView = new EntryListView();
	fDetailView = new DetailView();

	// Status bar at the bottom
	fStatusBar = new StatusBar();
	fStatusBar->SetStatusText("Database locked");

	// Set explicit min sizes so the left pane splitter can move freely.
	// DetailView keeps its natural min so right pane fields don't clip.
	fGroupListView->SetExplicitMinSize(BSize(0, 0));
	fEntryListView->SetExplicitMinSize(BSize(0, 0));

	// Right pane: entry list on top, detail view below
	fRightSplit = new BSplitView(B_VERTICAL, 0);
	fRightSplit->AddChild(fEntryListView);
	fRightSplit->AddChild(fDetailView);
	fRightSplit->SetItemWeight(0, 0.4f, false);
	fRightSplit->SetItemWeight(1, 0.6f, false);
	// Let the right pane's natural min width (from DetailView content)
	// protect fields from clipping. Set max to unlimited so it can grow.
	fRightSplit->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));

	// Main split: groups on the left, right pane on the right
	fMainSplit = new BSplitView(B_HORIZONTAL, 0);
	fMainSplit->AddChild(fGroupListView);
	fMainSplit->AddChild(fRightSplit);
	fMainSplit->SetItemWeight(0, 0.2f, false);
	fMainSplit->SetItemWeight(1, 0.8f, false);

	// Toolbar. AddAction()'s button copies the icon, so the loaded
	// bitmaps are freed right after.
	fToolBar = new BPrivate::BToolBar(B_HORIZONTAL);
	struct {
		uint32 command;
		const char* icon;
		const char* toolTip;
	} kActions[] = {
		{ kMsgNewDatabase, "newDB", "New database" },
		{ kMsgOpenDatabase, "openDB", "Open database" },
		{ kMsgSaveDatabase, "saveDB", "Save database" },
		{ kMsgCloseDatabase, "closeDB", "Close database" },
		{ kMsgLockDatabase, "lockDB", "Lock or unlock database" },
		{ 0, NULL, NULL },	// separator
		{ kMsgNewEntry, "addEntry", "Add entry" },
		{ kMsgEditEntry, "editEntry", "Edit entry" },
		{ kMsgDeleteEntry, "removeEntry", "Remove entry" },
		{ 0, NULL, NULL },	// separator
		{ kMsgShowSettings, "settings", "Settings" },
	};
	for (size_t i = 0; i < sizeof(kActions) / sizeof(kActions[0]);
			i++) {
		if (kActions[i].command == 0) {
			fToolBar->AddSeparator();
			continue;
		}
		BBitmap* icon = _LoadToolBarIcon(kActions[i].icon);
		fToolBar->AddAction(kActions[i].command, this, icon,
			kActions[i].toolTip);
		delete icon;
	}
	fToolBar->AddGlue();

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.Add(fToolBar)
		.Add(new BorderView())
		.Add(fMainSplit)
		.Add(fStatusBar)
	.End();

	SetSizeLimits(500, B_SIZE_UNLIMITED, 400, B_SIZE_UNLIMITED);
}


void
KuraWindow::Minimize(bool minimize)
{
	if (minimize && !fIsLocked && fSettings.LockOnMinimize()) {
		if (fSettings.AutoSaveOnLock()) {
			_Lock(false);
		} else {
			close_choice choice = _ConfirmClose(
				"Save changes before locking?");
			if (choice != CLOSE_CANCELLED)
				_Lock(false, choice != CLOSE_DISCARDED);
			// Cancelled: minimize without locking
		}
	}

	BWindow::Minimize(minimize);

	// Restoring a locked window implies the intent to use it, so
	// offer to unlock right away rather than requiring a trip to
	// the menu. (This covers locks from any source: minimize,
	// auto-lock while minimized, or a manual lock. Cancelling just
	// leaves the window locked as before.)
	if (!minimize && fIsLocked) {
		_OfferUnlock();
	}
}


void
KuraWindow::DispatchMessage(BMessage* message, BHandler* handler)
{
	// Real user input resets the auto-lock timer
	switch (message->what) {
		case B_KEY_DOWN:
		case B_UNMAPPED_KEY_DOWN:
		case B_MOUSE_DOWN:
		case B_MOUSE_WHEEL_CHANGED:
			fLastActivity = system_time();
			break;
	}

	BWindow::DispatchMessage(message, handler);
}


void
KuraWindow::MessageReceived(BMessage* message)
{
	// Reset the auto-lock timer on user-driven messages, but never on
	// the timer's own tick (that would keep auto-lock from ever firing).
	switch (message->what) {
		case kMsgAutoLockTick:
		case kMsgClearClipboard:
			break;
		default:
			fLastActivity = system_time();
			break;
	}

	switch (message->what) {
		// --- Unlock ---
		case kMsgUnlock:
		{
			fUnlockShowing = false;
			BString password;
			BString current;
			int32 mode;
			if (message->FindString("password", &password) == B_OK
				&& message->FindInt32("mode", &mode) == B_OK) {
				message->FindString("current", &current);
				_UnlockDatabase(password, mode, current);
			}
			scrub_string(password);
			scrub_string(current);
			break;
		}

		case kMsgUnlockCancel:
		{
			fUnlockShowing = false;
			if (fIsLocked && fIsNewDb) {
				// Cancelled creating a database: back to the
				// no-database state
				fDbPath = "";
				_UpdateTitle();
				_UpdateMenus();
				_UpdateStatusBar();
			}
			break;
		}

		// --- Database ---
		case kMsgNewDatabase:
			_NewDatabase();
			break;

		case kMsgOpenDatabase:
			_OpenDatabase();
			break;

		case kMsgSaveDatabase:
			_SaveDatabase();
			break;

		case kMsgSaveDatabaseAs:
			_SaveDatabaseAs();
			break;

		case kMsgImportCsv:
			_ImportCsv();
			break;

		case kMsgOpenRecent:
		{
			const char* pathStr;
			if (message->FindString("path", &pathStr) != B_OK)
				break;
			BString path(pathStr);

			// Already open?
			if (!fIsLocked && path == fDbPath) {
				fStatusBar->SetStatusText("This database is already open.");
				break;
			}

			if (!BEntry(path.String()).Exists()) {
				BString error("The file no longer exists:\n");
				error << path;
				BAlert* alert = new BAlert("Error", error.String(),
					"OK", NULL, NULL, B_WIDTH_AS_USUAL,
					B_WARNING_ALERT);
				alert->Go();
				fSettings.RemoveRecentFile(path.String());
				fSettings.Save();
				_UpdateRecentMenu();
				break;
			}

			close_choice choice = _ConfirmClose(
				"Save changes before opening another database?");
			if (choice == CLOSE_CANCELLED)
				break;

			_Lock(false, choice != CLOSE_DISCARDED);
			fDbPath = path;
			fIsNewDb = false;
			_UpdateTitle();
			_ShowUnlockDialog(UNLOCK_OPEN);
			break;
		}

		case kMsgClearRecent:
			fSettings.ClearRecentFiles();
			fSettings.Save();
			_UpdateRecentMenu();
			break;

		case kMsgCloseDatabase:
		{
			if (fIsLocked)
				break;

			close_choice choice = _ConfirmClose(
				"Save changes before closing the database?");
			if (choice == CLOSE_CANCELLED)
				break;

			_Lock(false, choice != CLOSE_DISCARDED);
			fStatusBar->SetStatusText(choice == CLOSE_DISCARDED
				? "Database closed, changes discarded"
				: "Database closed");
			break;
		}

		case kMsgLockDatabase:
		{
			if (fIsLocked) {
				_OfferUnlock();
				break;
			}

			if (fSettings.AutoSaveOnLock()) {
				_Lock(true);
			} else {
				close_choice choice = _ConfirmClose(
					"Save changes before locking?");
				if (choice == CLOSE_CANCELLED)
					break;
				_Lock(true, choice != CLOSE_DISCARDED);
			}
			break;
		}

		case kMsgChangePassword:
			if (!fIsLocked)
				_ShowUnlockDialog(UNLOCK_CHANGE);
			break;

		// --- File panel results ---
		case kMsgImportCsvRef:
		{
			if (fIsLocked)
				break;

			entry_ref ref;
			if (message->FindRef("refs", 0, &ref) != B_OK)
				break;
			BEntry entry(&ref);
			BPath path;
			if (entry.GetPath(&path) != B_OK)
				break;

			KuraCsvImport importer;
			status_t result = importer.Import(path.Path(),
				&fDatabase);

			if (result != B_OK) {
				BString error("Import failed:\n");
				error << importer.ErrorString();
				BAlert* alert = new BAlert("Error", error.String(),
					"OK", NULL, NULL, B_WIDTH_AS_USUAL,
					B_STOP_ALERT);
				alert->Go();
				break;
			}

			_RefreshAll();

			BString summary;
			summary.SetToFormat("Imported %d entries",
				(int)importer.ImportedEntries());
			if (importer.CreatedGroups() > 0) {
				BString g;
				g.SetToFormat(" and created %d groups",
					(int)importer.CreatedGroups());
				summary << g;
			}
			summary << ".";
			if (importer.SkippedRows() > 0) {
				BString s;
				s.SetToFormat("\n%d empty rows were skipped.",
					(int)importer.SkippedRows());
				summary << s;
			}
			summary << "\n\nThe database has not been saved yet - "
				"review the imported entries and save.\n\n"
				"Note: the CSV file still contains your passwords "
				"in plain text. Consider deleting it after "
				"verifying the import.";

			BAlert* alert = new BAlert("Import", summary.String(),
				"OK", NULL, NULL, B_WIDTH_AS_USUAL, B_INFO_ALERT);
			alert->Go();

			fStatusBar->SetStatusText("Import complete - unsaved changes");
			break;
		}

		case kMsgOpenDbRef:
		{
			entry_ref ref;
			if (message->FindRef("refs", 0, &ref) != B_OK)
				break;
			BEntry entry(&ref);
			BPath path;
			if (entry.GetPath(&path) != B_OK)
				break;

			close_choice choice = _ConfirmClose(
				"Save changes before opening another database?");
			if (choice == CLOSE_CANCELLED)
				break;

			_Lock(false, choice != CLOSE_DISCARDED);
			fDbPath = path.Path();
			fIsNewDb = false;
			_UpdateTitle();
			_ShowUnlockDialog(UNLOCK_OPEN);
			break;
		}

		case kMsgNewDbRef:
		case kMsgSaveAsRef:
		{
			entry_ref dirRef;
			BString name;
			if (message->FindRef("directory", &dirRef) != B_OK
				|| message->FindString("name", &name) != B_OK)
				break;

			BEntry dirEntry(&dirRef);
			BPath path;
			if (dirEntry.GetPath(&path) != B_OK)
				break;

			if (name.FindLast('.') < 0)
				name << ".kvdb";
			path.Append(name.String());

			if (message->what == kMsgNewDbRef) {
				close_choice choice = _ConfirmClose(
					"Save changes before creating a new database?");
				if (choice == CLOSE_CANCELLED)
					break;

				_Lock(false, choice != CLOSE_DISCARDED);
				fDbPath = path.Path();
				fIsNewDb = true;
				_UpdateTitle();
				_ShowUnlockDialog(UNLOCK_NEW);
			} else {
				// Save as: keep the database open, just retarget
				if (fIsLocked)
					break;
				fDbPath = path.Path();
				_SaveDatabase();
				_UpdateTitle();
				fStatusBar->SetStatusText("Database saved.");
			}
			break;
		}

		// --- Entry dropped onto a group ---
		case kMsgEntryDropped:
		{
			if (fIsLocked)
				break;

			int32 entryId;
			int32 groupId;
			if (message->FindInt32("entryId", &entryId) != B_OK
				|| message->FindInt32("groupId", &groupId) != B_OK)
				break;

			const KuraEntry* entry = fDatabase.EntryById(entryId);
			if (entry == NULL || entry->groupId == groupId)
				break;

			KuraEntry moved(*entry);
			moved.groupId = groupId;
			fDatabase.UpdateEntry(entryId, moved);

			_RefreshAll();
			fEntryListView->SelectEntry(entryId);
			_ShowSelectedEntry();

			BString groupName("Root");
			const KuraGroup* group = fDatabase.GroupById(groupId);
			if (group != NULL)
				groupName = group->name;
			BString status;
			status.SetToFormat("Moved \"%s\" to \"%s\"",
				moved.title.String(), groupName.String());
			fStatusBar->SetStatusText(status.String());
			break;
		}

		// --- Group selection ---
		case kMsgGroupSelected:
		{
			kura_id groupId = fGroupListView->SelectedGroupId();
			fEntryListView->ShowGroup(groupId);
			fDetailView->ShowEntry(NULL);
			break;
		}

		// --- Entry selection ---
		case kMsgEntrySelected:
			_ShowSelectedEntry();
			break;

		// --- Entry operations ---
		case kMsgNewEntry:
			_NewEntry();
			break;

		case kMsgEditEntry:
			_EditEntry();
			break;

		case kMsgDeleteEntry:
			_DeleteEntry();
			break;

		case kMsgDuplicateEntry:
			_DuplicateEntry();
			break;

		case kMsgCopyUsername:
			_CopyUsername();
			break;

		case kMsgCopyPassword:
			_CopyPassword();
			break;

		case kMsgOpenUrl:
		{
			kura_id entryId = fEntryListView->SelectedEntryId();
			if (entryId != kNoId) {
				const KuraEntry* entry = fDatabase.EntryById(entryId);
				if (entry != NULL && entry->url.Length() > 0)
					open_url(entry->url.String());
			}
			break;
		}

		// --- Entry/Group save callbacks ---
		case kMsgEntrySave:
		{
			kura_id entryId;
			if (message->FindInt32("entryId", &entryId) == B_OK) {
				_RefreshAll();
				fEntryListView->SelectEntry(entryId);
				_ShowSelectedEntry();
			}
			break;
		}

		case kMsgGroupSave:
			_RefreshAll();
			break;

		// --- Group operations ---
		case kMsgNewGroup:
			_NewGroup();
			break;

		case kMsgEditGroup:
			_EditGroup();
			break;

		case kMsgDeleteGroup:
			_DeleteGroup();
			break;

		// --- Search ---
		case kMsgFocusSearch:
			if (!fIsLocked)
				fEntryListView->FocusSearch();
			break;

		// --- Settings ---
		case kMsgShowSettings:
		{
			if (fSettingsShowing) {
				fSettingsMessenger.SendMessage(kMsgActivateSettings);
				break;
			}
			SettingsWindow* settings = new SettingsWindow(
				BRect(0, 0, 400, 260), BMessenger(this),
				fSettings.BackupEnabled(),
				fSettings.AutoLockEnabled(),
				fSettings.AutoLockMinutes(),
				fSettings.ClipboardClearEnabled(),
				fSettings.ClipboardClearSeconds(),
				fSettings.LockOnMinimize(),
				fSettings.AutoSaveOnLock());
			fSettingsMessenger = BMessenger(settings);
			fSettingsShowing = true;
			center_over(settings, this);
			settings->Show();
			break;
		}

		case kMsgSettingsChanged:
		{
			bool backup;
			bool autoLock;
			int32 autoLockMinutes;
			bool clipClear;
			int32 clipClearSeconds;
			bool lockMinimize;
			bool autoSaveLock;
			if (message->FindBool("backup", &backup) != B_OK
				|| message->FindBool("autoLock", &autoLock) != B_OK
				|| message->FindInt32("autoLockMinutes",
					&autoLockMinutes) != B_OK
				|| message->FindBool("clipClear", &clipClear) != B_OK
				|| message->FindInt32("clipClearSeconds",
					&clipClearSeconds) != B_OK
				|| message->FindBool("lockMinimize",
					&lockMinimize) != B_OK
				|| message->FindBool("autoSaveLock",
					&autoSaveLock) != B_OK)
				break;

			fSettings.SetBackupEnabled(backup);
			fSettings.SetAutoLockEnabled(autoLock);
			fSettings.SetAutoLockMinutes(autoLockMinutes);
			fSettings.SetClipboardClearEnabled(clipClear);
			fSettings.SetClipboardClearSeconds(clipClearSeconds);
			fSettings.SetLockOnMinimize(lockMinimize);
			fSettings.SetAutoSaveOnLock(autoSaveLock);
			fSettings.Save();

			_ApplyOptions();
			break;
		}

		case kMsgSettingsClosed:
			fSettingsShowing = false;
			break;

		// --- Tools ---
		case kMsgPasswordGenerator:
		{
			PasswordGeneratorWindow* generator
				= new PasswordGeneratorWindow(
					BRect(0, 0, 380, 300), BMessenger());
			center_over(generator, this);
			generator->Show();
			break;
		}

		// --- Auto-lock ---
		case kMsgAutoLockTick:
		{
			bigtime_t timeout = _AutoLockTimeout();
			if (timeout <= 0) {
				_StopAutoLock();
				_UpdateStatusBar();
				break;
			}

			// With auto-save disabled, the unattended timer can
			// neither save silently nor discard - so it defers,
			// restarting the countdown once the changes are saved
			if (!fIsLocked && fDatabase.IsModified()
				&& !fSettings.AutoSaveOnLock()) {
				fLastActivity = system_time();
				_UpdateStatusBar();
				break;
			}

			bigtime_t elapsed = system_time() - fLastActivity;
			if (elapsed >= timeout && !fIsLocked) {
				// Never lock while a dialog (entry editor, generator,
				// file panel) is open - locking would clear the
				// database out from under it. Lock as soon as the
				// dialog closes instead.
				if (!_DialogWindowOpen())
					_Lock(false);
			} else {
				_UpdateStatusBar();
			}
			break;
		}

		// --- Database change notifications ---
		case kMsgDatabaseLoaded:
		case kMsgDatabaseModified:
		case kMsgEntryAdded:
		case kMsgEntryUpdated:
		case kMsgEntryRemoved:
		case kMsgGroupAdded:
		case kMsgGroupUpdated:
		case kMsgGroupRemoved:
			_UpdateStatusBar();
			_UpdateTitle();
			break;

		// --- About ---
		case B_ABOUT_REQUESTED:
		{
			AboutWindow* about = new AboutWindow(this);
			about->Show();
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
KuraWindow::QuitRequested()
{
	// Save UI state
	fSettings.SetWindowFrame(Frame());
	fSettings.SaveSplitWeights("mainSplit", fMainSplit);
	fSettings.SaveSplitWeights("rightSplit", fRightSplit);
	fSettings.SaveColumnState("entryList",
		fEntryListView->ListView());
	fSettings.Save();

	// Unsaved changes: same prompt as Close/Open (discarding just
	// means quitting without saving; memory is scrubbed on exit)
	if (_ConfirmClose("Save changes before quitting?")
			== CLOSE_CANCELLED)
		return false;

	// Don't leave secrets on the clipboard after quitting
	KuraClipboard::ClearIfOurs();

	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}


// --- Database operations ---

void
KuraWindow::_OfferUnlock()
{
	if (fDbPath.Length() == 0)
		return;
	_ShowUnlockDialog(_DatabaseFileExists()
		? UNLOCK_OPEN : UNLOCK_NEW);
}


bool
KuraWindow::_DatabaseFileExists() const
{
	return BEntry(fDbPath.String()).Exists();
}


bool
KuraWindow::_DialogWindowOpen() const
{
	for (int32 i = 0;; i++) {
		BWindow* window = be_app->WindowAt(i);
		if (window == NULL)
			break;
		if (window == this)
			continue;

		if (window->LockWithTimeout(50000) == B_OK) {
			bool hidden = window->IsHidden();
			window->Unlock();
			if (!hidden)
				return true;
		} else {
			// Couldn't lock: assume the window is busy/open
			return true;
		}
	}
	return false;
}


// Ask what to do with unsaved changes before an operation that
// locks or leaves the database. Called at the moment the operation
// actually happens, so the answer cannot be overridden later.
KuraWindow::close_choice
KuraWindow::_ConfirmClose(const char* question)
{
	if (fIsLocked || !fDatabase.IsModified())
		return CLOSE_SAVED;

	BAlert* alert = new BAlert("Save?", question,
		"Don't save", "Cancel", "Save",
		B_WIDTH_AS_USUAL, B_OFFSET_SPACING, B_WARNING_ALERT);
	int32 result = alert->Go();
	if (result == 1)
		return CLOSE_CANCELLED;
	if (result == 0)
		return CLOSE_DISCARDED;

	_SaveDatabase();
	if (fDatabase.IsModified()) {
		// The save failed (an error alert was already shown);
		// don't proceed with an operation that would drop the data
		return CLOSE_CANCELLED;
	}
	return CLOSE_SAVED;
}


void
KuraWindow::_ShowUnlockDialog(unlock_mode mode)
{
	if (fUnlockShowing)
		return;

	fUnlockShowing = true;
	UnlockWindow* unlock = new UnlockWindow(BRect(0, 0, 420, 160),
		mode, fDbPath.String(), this);
	unlock->Show();
}


void
KuraWindow::_NewDatabase()
{
	if (fNewPanel == NULL) {
		BMessenger target(this);
		BMessage msg(kMsgNewDbRef);
		fNewPanel = new BFilePanel(B_SAVE_PANEL, &target, NULL, 0,
			false, &msg);
		if (fNewPanel->Window()->Lock()) {
			fNewPanel->Window()->SetTitle("Kura: New database");
			fNewPanel->Window()->Unlock();
		}
	}
	fNewPanel->SetSaveText("passwords.kvdb");
	fNewPanel->Show();
}


void
KuraWindow::_OpenDatabase()
{
	if (fOpenPanel == NULL) {
		BMessenger target(this);
		BMessage msg(kMsgOpenDbRef);
		fOpenPanel = new BFilePanel(B_OPEN_PANEL, &target, NULL, 0,
			false, &msg);
		if (fOpenPanel->Window()->Lock()) {
			fOpenPanel->Window()->SetTitle("Kura: Open database");
			fOpenPanel->Window()->Unlock();
		}
	}
	fOpenPanel->Show();
}


void
KuraWindow::_SaveDatabaseAs()
{
	if (fIsLocked)
		return;

	if (fSaveAsPanel == NULL) {
		BMessenger target(this);
		BMessage msg(kMsgSaveAsRef);
		fSaveAsPanel = new BFilePanel(B_SAVE_PANEL, &target, NULL, 0,
			false, &msg);
		if (fSaveAsPanel->Window()->Lock()) {
			fSaveAsPanel->Window()->SetTitle("Kura: Save database as");
			fSaveAsPanel->Window()->Unlock();
		}
	}

	BPath current(fDbPath.String());
	fSaveAsPanel->SetSaveText(current.Leaf() != NULL
		? current.Leaf() : "passwords.kvdb");
	fSaveAsPanel->Show();
}


void
KuraWindow::_ImportCsv()
{
	if (fIsLocked)
		return;

	if (fImportPanel == NULL) {
		BMessenger target(this);
		BMessage msg(kMsgImportCsvRef);
		fImportPanel = new BFilePanel(B_OPEN_PANEL, &target, NULL, 0,
			false, &msg);
		if (fImportPanel->Window()->Lock()) {
			fImportPanel->Window()->SetTitle(
				"Kura: Import KeePass CSV");
			fImportPanel->Window()->Unlock();
		}
	}
	fImportPanel->Show();
}


void
KuraWindow::_SaveDatabase()
{
	if (fIsLocked || fPassword.Length() == 0)
		return;

	BString json;
	fDatabase.SerializeToJson(json);

	status_t result = fCrypto.EncryptToFile(fDbPath.String(),
		fPassword, json);

	// Scrub JSON from memory (it contains plaintext passwords)
	scrub_string(json);

	if (result != B_OK) {
		BString error("Failed to save database:\n");
		error << fCrypto.ErrorString();
		BAlert* alert = new BAlert("Error", error.String(), "OK",
			NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
		alert->Go();
		return;
	}

	fDatabase.SetModified(false);
	_UpdateTitle();
	_UpdateStatusBar();
}


void
KuraWindow::_Lock(bool showUnlockDialog, bool saveChanges)
{
	if (fIsLocked) {
		if (showUnlockDialog) {
			_OfferUnlock();
		}
		return;
	}

	// Auto-save before locking. saveChanges is false only when the
	// user explicitly chose to discard in a _ConfirmClose() prompt.
	bool savedChanges = false;
	if (saveChanges && fDatabase.IsModified()) {
		_SaveDatabase();
		if (fDatabase.IsModified()) {
			// The save failed (an error alert was already shown).
			// Locking now would destroy the unsaved data, so abort.
			return;
		}
		savedChanges = true;
	}

	fIsLocked = true;
	fDatabase.Clear();
	scrub_string(fPassword);
	fPassword = "";

	// Clear the UI
	fDetailView->ShowEntry(NULL);
	fDetailView->SetDatabase(NULL);
	fGroupListView->SetDatabase(NULL);
	fEntryListView->SetDatabase(NULL);

	// Don't leave secrets on the clipboard while locked
	KuraClipboard::ClearIfOurs();

	_StopAutoLock();
	_UpdateTitle();
	_UpdateStatusBar();
	_UpdateMenus();

	// Transparency: never save silently without saying so
	if (savedChanges) {
		fStatusBar->SetStatusText(
			"Changes saved \xc2\xb7 database locked");
	}

	if (showUnlockDialog)
		_ShowUnlockDialog(UNLOCK_OPEN);
}


void
KuraWindow::_UnlockDatabase(const BString& password, int32 mode,
	const BString& current)
{
	if (mode == UNLOCK_CHANGE) {
		if (fIsLocked)
			return;

		// Verify the current master password before changing it
		if (current != fPassword) {
			BAlert* alert = new BAlert("Error",
				"The current password is incorrect.", "OK",
				NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
			alert->Go();
			_ShowUnlockDialog(UNLOCK_CHANGE);
			return;
		}

		scrub_string(fPassword);
		fPassword = password;
		_SaveDatabase();
		fStatusBar->SetStatusText("Master password changed.");
		return;
	}

	scrub_string(fPassword);
	fPassword = password;

	if (fIsNewDb) {
		// Create a new database with defaults
		fDatabase.InitDefaults();
		fDatabase.SetTarget(BMessenger(this));
		fIsLocked = false;
		fIsNewDb = false;
		_SaveDatabase();
	} else {
		// Decrypt existing database
		BString json;
		status_t result = fCrypto.DecryptFromFile(fDbPath.String(),
			fPassword, json);

		if (result != B_OK) {
			scrub_string(fPassword);
			fPassword = "";

			BString error;
			if (result == B_PERMISSION_DENIED)
				error = "Wrong password. Please try again.";
			else
				error << "Failed to open database:\n"
					<< fCrypto.ErrorString();

			BAlert* alert = new BAlert("Error", error.String(), "OK",
				NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
			alert->Go();

			_ShowUnlockDialog(UNLOCK_OPEN);
			return;
		}

		fDatabase.DeserializeFromJson(json);
		fDatabase.SetTarget(BMessenger(this));

		// Scrub JSON from memory
		scrub_string(json);

		fIsLocked = false;
	}

	// Update UI - the sidebar root item shows the database name
	BPath dbPath(fDbPath.String());
	BString rootLabel(dbPath.Leaf() != NULL ? dbPath.Leaf() : "Root");
	int32 dot = rootLabel.FindLast('.');
	if (dot > 0)
		rootLabel.Truncate(dot);
	fGroupListView->SetRootLabel(rootLabel.String());

	fGroupListView->SetDatabase(&fDatabase);
	fEntryListView->SetDatabase(&fDatabase);
	fDetailView->SetDatabase(&fDatabase);

	_RecordRecentFile();

	_RefreshAll();
	_ResetAutoLock();
	_UpdateTitle();
	_UpdateStatusBar();
	_UpdateMenus();
}


// --- Entry operations ---

void
KuraWindow::_NewEntry()
{
	if (fIsLocked)
		return;

	BRect frame(Frame());
	BRect dlgFrame(frame.left + 40, frame.top + 40,
		frame.left + 560, frame.top + 440);
	EntryEditWindow* edit = new EntryEditWindow(dlgFrame, NULL,
		&fDatabase, this);
	edit->Show();
}


void
KuraWindow::_EditEntry()
{
	if (fIsLocked)
		return;

	kura_id entryId = fEntryListView->SelectedEntryId();
	if (entryId == kNoId)
		return;

	const KuraEntry* entry = fDatabase.EntryById(entryId);
	if (entry == NULL)
		return;

	BRect frame(Frame());
	BRect dlgFrame(frame.left + 40, frame.top + 40,
		frame.left + 560, frame.top + 440);
	EntryEditWindow* edit = new EntryEditWindow(dlgFrame, entry,
		&fDatabase, this);
	edit->Show();
}


void
KuraWindow::_DeleteEntry()
{
	if (fIsLocked)
		return;

	kura_id entryId = fEntryListView->SelectedEntryId();
	if (entryId == kNoId)
		return;

	const KuraEntry* entry = fDatabase.EntryById(entryId);
	if (entry == NULL)
		return;

	BString text("Delete entry \"");
	text << entry->title << "\"?";

	BAlert* alert = new BAlert("Delete", text.String(),
		"Cancel", "Delete", NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	if (alert->Go() != 1)
		return;

	fDatabase.RemoveEntry(entryId);
	fDetailView->ShowEntry(NULL);
	_RefreshAll();
}


void
KuraWindow::_DuplicateEntry()
{
	if (fIsLocked)
		return;

	kura_id entryId = fEntryListView->SelectedEntryId();
	if (entryId == kNoId)
		return;

	const KuraEntry* entry = fDatabase.EntryById(entryId);
	if (entry == NULL)
		return;

	KuraEntry copy(*entry);
	copy.title << " (copy)";
	kura_id newId = fDatabase.AddEntry(copy);

	_RefreshAll();
	fEntryListView->SelectEntry(newId);
	_ShowSelectedEntry();
}


void
KuraWindow::_CopyUsername()
{
	if (fIsLocked)
		return;

	kura_id entryId = fEntryListView->SelectedEntryId();
	if (entryId == kNoId)
		return;

	const KuraEntry* entry = fDatabase.EntryById(entryId);
	if (entry == NULL)
		return;

	KuraClipboard::CopyWithTimedClear(entry->username.String());
	BString status("Username copied");
	if (fSettings.ClipboardClearEnabled()) {
		BString suffix;
		suffix.SetToFormat(" (clears in %ds)",
			(int)fSettings.ClipboardClearSeconds());
		status << suffix;
	}
	fStatusBar->SetStatusText(status.String());
}


void
KuraWindow::_CopyPassword()
{
	if (fIsLocked)
		return;

	kura_id entryId = fEntryListView->SelectedEntryId();
	if (entryId == kNoId)
		return;

	const KuraEntry* entry = fDatabase.EntryById(entryId);
	if (entry == NULL)
		return;

	KuraClipboard::CopyWithTimedClear(entry->password.String());
	BString status("Password copied");
	if (fSettings.ClipboardClearEnabled()) {
		BString suffix;
		suffix.SetToFormat(" (clears in %ds)",
			(int)fSettings.ClipboardClearSeconds());
		status << suffix;
	}
	fStatusBar->SetStatusText(status.String());
}


// --- Group operations ---

void
KuraWindow::_NewGroup()
{
	if (fIsLocked)
		return;

	BRect frame(Frame());
	BRect dlgFrame(frame.left + 60, frame.top + 60,
		frame.left + 480, frame.top + 280);
	GroupEditWindow* edit = new GroupEditWindow(dlgFrame, NULL,
		&fDatabase, this);
	edit->Show();
}


void
KuraWindow::_EditGroup()
{
	if (fIsLocked)
		return;

	kura_id groupId = fGroupListView->SelectedGroupId();
	if (groupId == kAllGroupId)
		return;

	const KuraGroup* group = fDatabase.GroupById(groupId);
	if (group == NULL)
		return;

	BRect frame(Frame());
	BRect dlgFrame(frame.left + 60, frame.top + 60,
		frame.left + 480, frame.top + 280);
	GroupEditWindow* edit = new GroupEditWindow(dlgFrame, group,
		&fDatabase, this);
	edit->Show();
}


void
KuraWindow::_DeleteGroup()
{
	if (fIsLocked)
		return;

	kura_id groupId = fGroupListView->SelectedGroupId();
	if (groupId == kAllGroupId)
		return;

	const KuraGroup* group = fDatabase.GroupById(groupId);
	if (group == NULL)
		return;

	int32 entryCount = fDatabase.CountEntriesInGroup(groupId, true);
	BString text("Delete group \"");
	text << group->name << "\"?";
	if (entryCount > 0)
		text << "\n\n" << entryCount
			<< " entries will be moved to Root.";

	BAlert* alert = new BAlert("Delete", text.String(),
		"Cancel", "Delete", NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	if (alert->Go() != 1)
		return;

	fDatabase.RemoveGroup(groupId);
	_RefreshAll();
}


// --- UI helpers ---

void
KuraWindow::_RefreshAll()
{
	fGroupListView->Refresh();
	fEntryListView->Refresh();
	_UpdateStatusBar();
	_UpdateTitle();
}


void
KuraWindow::_ShowSelectedEntry()
{
	kura_id entryId = fEntryListView->SelectedEntryId();
	if (entryId == kNoId) {
		fDetailView->ShowEntry(NULL);
		return;
	}

	const KuraEntry* entry = fDatabase.EntryById(entryId);
	fDetailView->ShowEntry(entry);
}


void
KuraWindow::_UpdateTitle()
{
	BString title("Kura");
	if (!fIsLocked) {
		BPath path(fDbPath.String());
		title << " \xe2\x80\x94 " << path.Leaf();
		if (fDatabase.IsModified())
			title << " *";
	}
	SetTitle(title.String());
}


void
KuraWindow::_UpdateMenus()
{
	fSaveItem->SetEnabled(!fIsLocked);
	fSaveAsItem->SetEnabled(!fIsLocked);
	fImportItem->SetEnabled(!fIsLocked);
	fCloseItem->SetEnabled(!fIsLocked);
	fToolBar->SetActionEnabled(kMsgSaveDatabase, !fIsLocked);
	fToolBar->SetActionEnabled(kMsgCloseDatabase, !fIsLocked);
	fToolBar->SetActionEnabled(kMsgNewEntry, !fIsLocked);
	fToolBar->SetActionEnabled(kMsgEditEntry, !fIsLocked);
	fToolBar->SetActionEnabled(kMsgDeleteEntry, !fIsLocked);
	fChangePwItem->SetEnabled(!fIsLocked);
	fLockItem->SetLabel(fIsLocked
		? "Unlock database" B_UTF8_ELLIPSIS : "Lock database");
	fGroupsMenu->SetEnabled(!fIsLocked);
	fEntriesMenu->SetEnabled(!fIsLocked);
	fEntryListView->SetSearchEnabled(!fIsLocked);
}


void
KuraWindow::_RecordRecentFile()
{
	fSettings.AddRecentFile(fDbPath.String());
	fSettings.Save();
	_UpdateRecentMenu();
}


void
KuraWindow::_UpdateRecentMenu()
{
	// Rebuild the submenu from the settings
	while (BMenuItem* item = fRecentMenu->RemoveItem((int32)0))
		delete item;

	int32 count = fSettings.CountRecentFiles();
	if (count == 0) {
		BMenuItem* empty = new BMenuItem("(no recent files)",
			NULL);
		empty->SetEnabled(false);
		fRecentMenu->AddItem(empty);
		return;
	}

	for (int32 i = 0; i < count; i++) {
		BString path = fSettings.RecentFileAt(i);
		if (path.Length() == 0)
			continue;

		BMessage* msg = new BMessage(kMsgOpenRecent);
		msg->AddString("path", path.String());
		fRecentMenu->AddItem(new BMenuItem(path.String(), msg));
	}

	fRecentMenu->AddSeparatorItem();
	fRecentMenu->AddItem(new BMenuItem("Clear recent files",
		new BMessage(kMsgClearRecent)));

	// Items added to an already-attached menu need an explicit target
	fRecentMenu->SetTargetForItems(this);
}


void
KuraWindow::_UpdateStatusBar()
{
	if (fIsLocked) {
		fStatusBar->SetStatusText(fDbPath.Length() > 0
			? "Database locked" : "No database open");
		fStatusBar->SetCountText("");
		return;
	}

	bigtime_t timeout = _AutoLockTimeout();
	if (timeout <= 0) {
		fStatusBar->SetStatusText("Unlocked");
	} else if (fDatabase.IsModified()
		&& !fSettings.AutoSaveOnLock()) {
		fStatusBar->SetStatusText("Unlocked \xc2\xb7 auto-lock "
			"paused (unsaved changes)");
	} else {
		bigtime_t remaining = timeout
			- (system_time() - fLastActivity);
		if (remaining < 0)
			remaining = 0;

		int seconds = (int)(remaining / 1000000);
		int minutes = seconds / 60;
		seconds %= 60;

		BString status;
		status.SetToFormat("Unlocked \xc2\xb7 Auto-lock in %d:%02d",
			minutes, seconds);
		fStatusBar->SetStatusText(status.String());
	}

	BString count;
	count.SetToFormat("%d entries", (int)fDatabase.CountEntries());
	fStatusBar->SetCountText(count.String());
}


void
KuraWindow::_ResetAutoLock()
{
	_StopAutoLock();
	fLastActivity = system_time();

	if (_AutoLockTimeout() <= 0) {
		_UpdateStatusBar();
		return;
	}

	// Tick every second for the countdown display
	BMessage tickMsg(kMsgAutoLockTick);
	fAutoLockRunner = new BMessageRunner(BMessenger(this),
		&tickMsg, 1000000);
}


void
KuraWindow::_StopAutoLock()
{
	delete fAutoLockRunner;
	fAutoLockRunner = NULL;
}


bigtime_t
KuraWindow::_AutoLockTimeout() const
{
	if (!fSettings.AutoLockEnabled())
		return 0;
	return (bigtime_t)fSettings.AutoLockMinutes() * 60 * 1000000;
}


void
KuraWindow::_ApplyOptions()
{
	fCrypto.SetBackupEnabled(fSettings.BackupEnabled());
	KuraClipboard::SetClearDelay(fSettings.ClipboardClearEnabled()
		? (bigtime_t)fSettings.ClipboardClearSeconds() * 1000000
		: 0);

	if (!fIsLocked)
		_ResetAutoLock();
	_UpdateStatusBar();
}
