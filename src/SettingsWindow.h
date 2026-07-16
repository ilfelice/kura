/*
 * SettingsWindow.h
 * Kura - Password Manager for Haiku
 *
 * Application settings dialog. Follows the Haiku preferences
 * convention: changes apply instantly (no OK/Cancel), with
 * "Defaults" and "Revert" buttons. Every change is sent to the
 * target window as a kMsgSettingsChanged message carrying the full
 * option state; the target persists and applies it.
 */
#ifndef SETTINGS_WINDOW_H
#define SETTINGS_WINDOW_H

#include <Messenger.h>
#include <Window.h>

class BButton;
class BCheckBox;
class BSpinner;


class SettingsWindow : public BWindow {
public:
						SettingsWindow(BRect frame, BMessenger target,
							bool backupEnabled,
							bool autoLockEnabled,
							int32 autoLockMinutes,
							bool clipClearEnabled,
							int32 clipClearSeconds,
							bool lockOnMinimize,
							bool autoSaveOnLock);

	virtual void		MessageReceived(BMessage* message);
	virtual bool		QuitRequested();

private:
	void				_SetValues(bool backup, bool autoLock,
							int32 autoLockMinutes, bool clipClear,
							int32 clipClearSeconds,
							bool lockOnMinimize,
							bool autoSaveOnLock);
	void				_UpdateEnabled();
	void				_Apply();

	BCheckBox*			fClipClearBox;
	BSpinner*			fClipClearSpinner;
	BCheckBox*			fAutoLockBox;
	BSpinner*			fAutoLockSpinner;
	BCheckBox*			fLockMinimizeBox;
	BCheckBox*			fAutoSaveBox;
	BCheckBox*			fBackupBox;
	BButton*			fDefaultsButton;
	BButton*			fRevertButton;

	BMessenger			fTarget;

	// Snapshot at open time, for Revert
	bool				fOriginalBackup;
	bool				fOriginalAutoLock;
	int32				fOriginalAutoLockMinutes;
	bool				fOriginalClipClear;
	int32				fOriginalClipClearSeconds;
	bool				fOriginalLockOnMinimize;
	bool				fOriginalAutoSaveOnLock;
};

#endif // SETTINGS_WINDOW_H
