/*
 * EntryEditWindow.h
 * Kura - Password Manager for Haiku
 * Distributed under the terms of the MIT License.
 * Copyright 2026 Il Felice.
 *
 * Dialog for creating or editing a password entry.
 * Fields: title, group, username, password, URL, notes.
 */
#ifndef ENTRY_EDIT_WINDOW_H
#define ENTRY_EDIT_WINDOW_H

#include <Window.h>

#include "KuraDatabase.h"

class BButton;
class BMenuField;
class BTextControl;
class BTextView;
class FieldView;


class EntryEditWindow : public BWindow {
public:
	// Create for a new entry (entry == NULL) or edit existing
						EntryEditWindow(BRect frame,
							const KuraEntry* entry,
							KuraDatabase* database,
							BWindow* target);
	virtual				~EntryEditWindow();

	virtual void		MessageReceived(BMessage* message);
	virtual bool		QuitRequested();

private:
	void				_Save();
	void				_PopulateGroupMenu();

	BTextControl*		fTitleField;
	BButton*			fGenerateButton;
	BMenuField*			fGroupField;
	BTextControl*		fUsernameField;
	FieldView*			fPasswordField;
	BTextControl*		fUrlField;
	BTextView*			fNotesView;
	BButton*			fSaveButton;
	BButton*			fCancelButton;

	KuraDatabase*		fDatabase;
	BWindow*			fTarget;
	kura_id				fEntryId;	// kNoId for new entries
	kura_id				fGroupId;
};

#endif // ENTRY_EDIT_WINDOW_H
