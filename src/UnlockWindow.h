/*
 * UnlockWindow.h
 * Kura - Password Manager for Haiku
 *
 * Modal dialog for entering the master password to unlock a database,
 * to set a password when creating a new database, or to change the
 * master password (current + new + confirm).
 */
#ifndef UNLOCK_WINDOW_H
#define UNLOCK_WINDOW_H

#include <String.h>
#include <Window.h>

class BButton;
class BStringView;
class BTextControl;


enum unlock_mode {
	UNLOCK_OPEN,		// Opening an existing database
	UNLOCK_NEW,			// Creating a new database (confirm password)
	UNLOCK_CHANGE,		// Changing the master password
};


class UnlockWindow : public BWindow {
public:
						UnlockWindow(BRect frame, unlock_mode mode,
							const char* dbPath, BWindow* target);
	virtual				~UnlockWindow();

	virtual void		MessageReceived(BMessage* message);
	virtual bool		QuitRequested();

private:
	void				_Submit();

	BTextControl*		fCurrentField;	// Only for CHANGE mode
	BTextControl*		fPasswordField;	// The (new) master password
	BTextControl*		fConfirmField;	// Only for NEW/CHANGE modes
	BButton*			fUnlockButton;
	BButton*			fCancelButton;
	BStringView*		fStatusView;
	BWindow*			fTarget;
	unlock_mode			fMode;
	BString				fDbPath;
	bool				fSubmitted;
};

#endif // UNLOCK_WINDOW_H
