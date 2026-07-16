/*
 * SettingsWindow.cpp
 * Kura - Password Manager for Haiku
 *
 * Settings dialog implementation.
 */

#include "SettingsWindow.h"
#include "KuraDefs.h"

#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <SpaceLayoutItem.h>
#include <StringView.h>
#include <private/interface/Spinner.h>


// Internal messages
enum {
	kMsgControlChanged	= 'ctch',
	kMsgDefaults		= 'dflt',
	kMsgRevert			= 'rvrt',
};


SettingsWindow::SettingsWindow(BRect frame, BMessenger target,
	bool backupEnabled, bool autoLockEnabled, int32 autoLockMinutes,
	bool clipClearEnabled, int32 clipClearSeconds, bool lockOnMinimize,
	bool autoSaveOnLock)
	:
	BWindow(frame, "Settings", B_TITLED_WINDOW_LOOK,
		B_NORMAL_WINDOW_FEEL,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_CLOSE_ON_ESCAPE),
	fTarget(target),
	fOriginalBackup(backupEnabled),
	fOriginalAutoLock(autoLockEnabled),
	fOriginalAutoLockMinutes(autoLockMinutes),
	fOriginalClipClear(clipClearEnabled),
	fOriginalClipClearSeconds(clipClearSeconds),
	fOriginalLockOnMinimize(lockOnMinimize),
	fOriginalAutoSaveOnLock(autoSaveOnLock)
{
	fClipClearBox = new BCheckBox("clipClear",
		"Clear clipboard after", new BMessage(kMsgControlChanged));
	fClipClearSpinner = new BSpinner("clipClearSeconds", "",
		new BMessage(kMsgControlChanged));
	fClipClearSpinner->SetRange(1, 3600);
	BStringView* clipUnit = new BStringView("clipUnit", "seconds");

	fAutoLockBox = new BCheckBox("autoLock",
		"Lock database after", new BMessage(kMsgControlChanged));
	fAutoLockSpinner = new BSpinner("autoLockMinutes", "",
		new BMessage(kMsgControlChanged));
	fAutoLockSpinner->SetRange(1, 1440);
	BStringView* autoLockUnit = new BStringView("autoLockUnit",
		"minutes of inactivity");

	fLockMinimizeBox = new BCheckBox("lockMinimize",
		"Lock database when minimizing the window",
		new BMessage(kMsgControlChanged));

	fAutoSaveBox = new BCheckBox("autoSaveLock",
		"Save automatically when locking",
		new BMessage(kMsgControlChanged));

	fBackupBox = new BCheckBox("backup",
		"Keep a backup copy (.bak) of the database when saving",
		new BMessage(kMsgControlChanged));

	fDefaultsButton = new BButton("defaults", "Defaults",
		new BMessage(kMsgDefaults));
	fRevertButton = new BButton("revert", "Revert",
		new BMessage(kMsgRevert));

	// Keep the spinners compact
	float spinnerWidth = be_plain_font->StringWidth("999999") * 2;
	fClipClearSpinner->SetExplicitMaxSize(
		BSize(spinnerWidth, B_SIZE_UNSET));
	fAutoLockSpinner->SetExplicitMaxSize(
		BSize(spinnerWidth, B_SIZE_UNSET));

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_ITEM_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGrid(B_USE_ITEM_SPACING, B_USE_ITEM_SPACING)
			.Add(fClipClearBox, 0, 0)
			.Add(fClipClearSpinner, 1, 0)
			.Add(clipUnit, 2, 0)
			.Add(BSpaceLayoutItem::CreateGlue(), 3, 0)
			.Add(fAutoLockBox, 0, 1)
			.Add(fAutoLockSpinner, 1, 1)
			.Add(autoLockUnit, 2, 1)
			.Add(BSpaceLayoutItem::CreateGlue(), 3, 1)
			.SetColumnWeight(3, 1.0f)
		.End()
		.Add(fLockMinimizeBox)
		.Add(fAutoSaveBox)
		.Add(fBackupBox)
		.AddStrut(B_USE_BIG_SPACING)
		.AddGroup(B_HORIZONTAL)
			.Add(fDefaultsButton)
			.Add(fRevertButton)
			.AddGlue()
		.End()
	.End();

	_SetValues(backupEnabled, autoLockEnabled, autoLockMinutes,
		clipClearEnabled, clipClearSeconds, lockOnMinimize,
		autoSaveOnLock);
	_UpdateEnabled();

	CenterOnScreen();
}


void
SettingsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgControlChanged:
			_UpdateEnabled();
			_Apply();
			break;

		case kMsgDefaults:
			_SetValues(true, true, kDefaultAutoLockMinutes,
				true, kDefaultClipboardClearSeconds, false, true);
			_UpdateEnabled();
			_Apply();
			break;

		case kMsgRevert:
			_SetValues(fOriginalBackup, fOriginalAutoLock,
				fOriginalAutoLockMinutes, fOriginalClipClear,
				fOriginalClipClearSeconds, fOriginalLockOnMinimize,
				fOriginalAutoSaveOnLock);
			_UpdateEnabled();
			_Apply();
			break;

		case kMsgActivateSettings:
			Activate();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
SettingsWindow::QuitRequested()
{
	fTarget.SendMessage(kMsgSettingsClosed);
	return true;
}


void
SettingsWindow::_SetValues(bool backup, bool autoLock,
	int32 autoLockMinutes, bool clipClear, int32 clipClearSeconds,
	bool lockOnMinimize, bool autoSaveOnLock)
{
	fBackupBox->SetValue(backup ? B_CONTROL_ON : B_CONTROL_OFF);
	fAutoLockBox->SetValue(autoLock ? B_CONTROL_ON : B_CONTROL_OFF);
	fClipClearBox->SetValue(clipClear ? B_CONTROL_ON : B_CONTROL_OFF);
	fLockMinimizeBox->SetValue(
		lockOnMinimize ? B_CONTROL_ON : B_CONTROL_OFF);
	fAutoSaveBox->SetValue(
		autoSaveOnLock ? B_CONTROL_ON : B_CONTROL_OFF);

	fAutoLockSpinner->SetValue(autoLockMinutes);
	fClipClearSpinner->SetValue(clipClearSeconds);
}


void
SettingsWindow::_UpdateEnabled()
{
	fAutoLockSpinner->SetEnabled(
		fAutoLockBox->Value() == B_CONTROL_ON);
	fClipClearSpinner->SetEnabled(
		fClipClearBox->Value() == B_CONTROL_ON);
}


void
SettingsWindow::_Apply()
{
	BMessage changed(kMsgSettingsChanged);
	changed.AddBool("backup", fBackupBox->Value() == B_CONTROL_ON);
	changed.AddBool("autoLock",
		fAutoLockBox->Value() == B_CONTROL_ON);
	changed.AddInt32("autoLockMinutes", fAutoLockSpinner->Value());
	changed.AddBool("clipClear",
		fClipClearBox->Value() == B_CONTROL_ON);
	changed.AddInt32("clipClearSeconds", fClipClearSpinner->Value());
	changed.AddBool("lockMinimize",
		fLockMinimizeBox->Value() == B_CONTROL_ON);
	changed.AddBool("autoSaveLock",
		fAutoSaveBox->Value() == B_CONTROL_ON);
	fTarget.SendMessage(&changed);
}
