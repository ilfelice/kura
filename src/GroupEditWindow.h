/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Dialog for creating or editing a group (folder).
 */
#ifndef GROUP_EDIT_WINDOW_H
#define GROUP_EDIT_WINDOW_H


#include <Window.h>

#include "KuraDatabase.h"

class BButton;
class BMenuField;
class BTextControl;


class GroupEditWindow : public BWindow {
public:
						GroupEditWindow(BRect frame,
							const KuraGroup* group,
							KuraDatabase* database,
							BWindow* target);
	virtual				~GroupEditWindow();

	virtual void		MessageReceived(BMessage* message);

private:
	void				_Save();
	void				_PopulateParentMenu();

	BTextControl*		fNameField;
	BMenuField*			fParentField;
	BButton*			fSaveButton;
	BButton*			fCancelButton;

	KuraDatabase*		fDatabase;
	BWindow*			fTarget;
	kura_id				fGroupId;
	kura_id				fParentId;
	BString				fIcon;
};


#endif // GROUP_EDIT_WINDOW_H
