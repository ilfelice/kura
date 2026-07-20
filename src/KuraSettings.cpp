/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Persists UI state as a flattened BMessage.
 */

#include "KuraSettings.h"
#include "KuraDefs.h"

#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <File.h>
#include <ObjectList.h>
#include <FindDirectory.h>
#include <Path.h>
#include <SplitView.h>

#include <cstdio>


static const char* kSettingsFileName = "settings";


static BPath
_SettingsFilePath()
{
	BPath path;
	find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	path.Append(kSettingsDir);
	path.Append(kSettingsFileName);
	return path;
}


KuraSettings::KuraSettings()
{
}


KuraSettings::~KuraSettings()
{
}


void
KuraSettings::Load()
{
	BPath path = _SettingsFilePath();
	BFile file(path.Path(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return;

	fSettings.Unflatten(&file);
}


void
KuraSettings::Save()
{
	BPath path = _SettingsFilePath();
	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return;

	fSettings.Flatten(&file);
}


// --- Window frame ---


bool
KuraSettings::HasWindowFrame() const
{
	BRect frame;
	return fSettings.FindRect("windowFrame", &frame) == B_OK;
}


BRect
KuraSettings::WindowFrame() const
{
	BRect frame;
	fSettings.FindRect("windowFrame", &frame);
	return frame;
}


void
KuraSettings::SetWindowFrame(BRect frame)
{
	fSettings.RemoveName("windowFrame");
	fSettings.AddRect("windowFrame", frame);
}


// --- Options ---


static int32
_Clamp(int32 value, int32 min, int32 max)
{
	if (value < min)
		return min;
	if (value > max)
		return max;
	return value;
}


bool
KuraSettings::BackupEnabled() const
{
	return fSettings.GetBool("backupEnabled", true);
}


void
KuraSettings::SetBackupEnabled(bool enabled)
{
	fSettings.SetBool("backupEnabled", enabled);
}


bool
KuraSettings::AutoLockEnabled() const
{
	return fSettings.GetBool("autoLockEnabled", true);
}


void
KuraSettings::SetAutoLockEnabled(bool enabled)
{
	fSettings.SetBool("autoLockEnabled", enabled);
}


int32
KuraSettings::AutoLockMinutes() const
{
	return _Clamp(fSettings.GetInt32("autoLockMinutes",
		kDefaultAutoLockMinutes), 1, 1440);
}


void
KuraSettings::SetAutoLockMinutes(int32 minutes)
{
	fSettings.SetInt32("autoLockMinutes", _Clamp(minutes, 1, 1440));
}


bool
KuraSettings::ClipboardClearEnabled() const
{
	return fSettings.GetBool("clipClearEnabled", true);
}


void
KuraSettings::SetClipboardClearEnabled(bool enabled)
{
	fSettings.SetBool("clipClearEnabled", enabled);
}


int32
KuraSettings::ClipboardClearSeconds() const
{
	return _Clamp(fSettings.GetInt32("clipClearSeconds",
		kDefaultClipboardClearSeconds), 1, 3600);
}


void
KuraSettings::SetClipboardClearSeconds(int32 seconds)
{
	fSettings.SetInt32("clipClearSeconds", _Clamp(seconds, 1, 3600));
}


bool
KuraSettings::LockOnMinimize() const
{
	return fSettings.GetBool("lockOnMinimize", false);
}


void
KuraSettings::SetLockOnMinimize(bool enabled)
{
	fSettings.SetBool("lockOnMinimize", enabled);
}


bool
KuraSettings::AutoSaveOnLock() const
{
	return fSettings.GetBool("autoSaveOnLock", true);
}


void
KuraSettings::SetAutoSaveOnLock(bool enabled)
{
	fSettings.SetBool("autoSaveOnLock", enabled);
}


// --- Recent files ---


static const char* kRecentFileField = "recentFile";
static const int32 kMaxRecentFiles = 10;


int32
KuraSettings::CountRecentFiles() const
{
	type_code type;
	int32 count = 0;
	if (fSettings.GetInfo(kRecentFileField, &type, &count) != B_OK)
		return 0;
	return count;
}


BString
KuraSettings::RecentFileAt(int32 index) const
{
	const char* path;
	if (fSettings.FindString(kRecentFileField, index, &path) == B_OK)
		return BString(path);
	return BString();
}


void
KuraSettings::AddRecentFile(const char* path)
{
	if (path == NULL || path[0] == '\0')
		return;

	// Rebuild the list with this path in front, duplicates removed
	BObjectList<BString, true> list(kMaxRecentFiles);
	list.AddItem(new BString(path));

	int32 count = CountRecentFiles();
	for (int32 i = 0; i < count; i++) {
		BString existing = RecentFileAt(i);
		if (existing == path)
			continue;
		if (list.CountItems() >= kMaxRecentFiles)
			break;
		list.AddItem(new BString(existing));
	}

	fSettings.RemoveName(kRecentFileField);
	for (int32 i = 0; i < list.CountItems(); i++)
		fSettings.AddString(kRecentFileField, list.ItemAt(i)->String());
}


void
KuraSettings::RemoveRecentFile(const char* path)
{
	if (path == NULL)
		return;

	BObjectList<BString, true> list(kMaxRecentFiles);
	int32 count = CountRecentFiles();
	for (int32 i = 0; i < count; i++) {
		BString existing = RecentFileAt(i);
		if (existing != path)
			list.AddItem(new BString(existing));
	}

	fSettings.RemoveName(kRecentFileField);
	for (int32 i = 0; i < list.CountItems(); i++)
		fSettings.AddString(kRecentFileField, list.ItemAt(i)->String());
}


void
KuraSettings::ClearRecentFiles()
{
	fSettings.RemoveName(kRecentFileField);
}


// --- Split view weights ---


void
KuraSettings::SaveSplitWeights(const char* name, BSplitView* split)
{
	BString fieldName;

	// Remove old data
	for (int32 i = 0; i < split->CountItems(); i++) {
		fieldName.SetToFormat("%s_weight_%d", name, (int)i);
		fSettings.RemoveName(fieldName.String());
	}

	// Save current weights
	for (int32 i = 0; i < split->CountItems(); i++) {
		fieldName.SetToFormat("%s_weight_%d", name, (int)i);
		fSettings.AddFloat(fieldName.String(), split->ItemWeight(i));
	}
}


void
KuraSettings::RestoreSplitWeights(const char* name, BSplitView* split)
{
	BString fieldName;

	for (int32 i = 0; i < split->CountItems(); i++) {
		fieldName.SetToFormat("%s_weight_%d", name, (int)i);
		float weight;
		if (fSettings.FindFloat(fieldName.String(), &weight) == B_OK)
			split->SetItemWeight(i, weight, true);
	}
}


// --- Column list view state ---


void
KuraSettings::SaveColumnState(const char* name, BColumnListView* list)
{
	// Remove old column data
	BString prefix;
	prefix.SetToFormat("%s_col_count", name);
	fSettings.RemoveName(prefix.String());

	int32 colCount = list->CountColumns();
	fSettings.AddInt32(prefix.String(), colCount);

	for (int32 i = 0; i < colCount; i++) {
		BColumn* col = list->ColumnAt(i);
		if (col == NULL)
			continue;

		BString field;

		// Width
		field.SetToFormat("%s_col_%d_width", name, (int)i);
		fSettings.RemoveName(field.String());
		fSettings.AddFloat(field.String(), col->Width());

		// Visibility
		field.SetToFormat("%s_col_%d_visible", name, (int)i);
		fSettings.RemoveName(field.String());
		fSettings.AddBool(field.String(), col->IsVisible());
	}
}


void
KuraSettings::RestoreColumnState(const char* name, BColumnListView* list)
{
	BString prefix;
	prefix.SetToFormat("%s_col_count", name);

	int32 savedCount;
	if (fSettings.FindInt32(prefix.String(), &savedCount) != B_OK)
		return;

	int32 colCount = list->CountColumns();
	int32 count = savedCount < colCount ? savedCount : colCount;

	for (int32 i = 0; i < count; i++) {
		BColumn* col = list->ColumnAt(i);
		if (col == NULL)
			continue;

		BString field;
		float width;
		bool visible;

		// Width
		field.SetToFormat("%s_col_%d_width", name, (int)i);
		if (fSettings.FindFloat(field.String(), &width) == B_OK)
			col->SetWidth(width);

		// Visibility
		field.SetToFormat("%s_col_%d_visible", name, (int)i);
		if (fSettings.FindBool(field.String(), &visible) == B_OK)
			col->SetVisible(visible);
	}
}
