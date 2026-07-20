/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Persists UI state: window frame, split positions, column state.
 * Stored as a flattened BMessage in the user settings directory.
 */

#ifndef KURA_SETTINGS_H
#define KURA_SETTINGS_H

#include <Message.h>
#include <Rect.h>
#include <String.h>

class BColumnListView;
class BSplitView;


class KuraSettings {
public:
						KuraSettings();
						~KuraSettings();

	void				Load();
	void				Save();

	// Window frame
	bool				HasWindowFrame() const;
	BRect				WindowFrame() const;
	void				SetWindowFrame(BRect frame);

	// Options (user-configurable in the settings window)
	bool				BackupEnabled() const;
	void				SetBackupEnabled(bool enabled);
	bool				AutoLockEnabled() const;
	void				SetAutoLockEnabled(bool enabled);
	int32				AutoLockMinutes() const;
	void				SetAutoLockMinutes(int32 minutes);
	bool				ClipboardClearEnabled() const;
	void				SetClipboardClearEnabled(bool enabled);
	int32				ClipboardClearSeconds() const;
	void				SetClipboardClearSeconds(int32 seconds);
	bool				LockOnMinimize() const;
	void				SetLockOnMinimize(bool enabled);
	bool				AutoSaveOnLock() const;
	void				SetAutoSaveOnLock(bool enabled);

	// Recently opened database files, most recent first
	int32				CountRecentFiles() const;
	BString				RecentFileAt(int32 index) const;
	void				AddRecentFile(const char* path);
	void				RemoveRecentFile(const char* path);
	void				ClearRecentFiles();

	// Split view weights
	void				SaveSplitWeights(const char* name,
							BSplitView* split);
	void				RestoreSplitWeights(const char* name,
							BSplitView* split);

	// Column list view state
	void				SaveColumnState(const char* name,
							BColumnListView* list);
	void				RestoreColumnState(const char* name,
							BColumnListView* list);

private:
	BMessage			fSettings;
};

#endif // KURA_SETTINGS_H
