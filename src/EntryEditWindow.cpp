/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Entry creation/editing dialog implementation.
 */


#include "EntryEditWindow.h"
#include "KuraUtils.h"
#include "FieldView.h"
#include "KuraDefs.h"
#include "PasswordGeneratorWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Resources.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#include <cstring>


// Resource IDs (must match Kura.rdef)
enum {
	kIconHide	= 11,
	kIconShow	= 12,
};

// Internal messages
enum {
	kMsgGroupChosen		= 'grch',
	kMsgTogglePassword	= 'tpwd',
	kMsgOpenGenerator	= 'opgn',
};


EntryEditWindow::EntryEditWindow(BRect frame, const KuraEntry* entry,
	KuraDatabase* database, BWindow* target)
	:
	BWindow(frame,
		entry != NULL ? "Edit Entry" : "New Entry",
		B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_CLOSE_ON_ESCAPE),
	fDatabase(database),
	fTarget(target),
	fEntryId(entry != NULL ? entry->id : kNoId),
	fGroupId(entry != NULL ? entry->groupId : kAllGroupId)
{
	fTitleField = new BTextControl("title", "Title:", "", NULL);
	fUsernameField = new BTextControl("username", "Username:", "", NULL);
	fUrlField = new BTextControl("url", "URL:", "", NULL);

	// Password field using FieldView with hide-typing and show/hide button
	fPasswordField = new FieldView("password");
	fPasswordField->SetEditable(true);
	fPasswordField->SetHideTyping(true);
	fPasswordField->AddButton(kIconHide, kMsgTogglePassword);

	fGenerateButton = new BButton("generate",
		"Generate" B_UTF8_ELLIPSIS, new BMessage(kMsgOpenGenerator));

	// Group menu
	BMenu* groupMenu = new BMenu("Group");
	fGroupField = new BMenuField("group", "Group:", groupMenu);
	_PopulateGroupMenu();

	// Notes
	BStringView* notesLabel = new BStringView("notesLabel", "Notes:");
	fNotesView = new BTextView("notes");
	fNotesView->SetStylable(false);
	BScrollView* notesScroll = new BScrollView("notesScroll", fNotesView,
		0, false, true);
	notesScroll->SetExplicitMinSize(BSize(B_SIZE_UNSET, 80));

	// Buttons
	fCancelButton = new BButton("cancel", "Cancel",
		new BMessage(kMsgEntryCancel));
	fSaveButton = new BButton("save", "Save",
		new BMessage(kMsgEntrySave));
	fSaveButton->MakeDefault(true);

	// Populate fields if editing
	if (entry != NULL) {
		fTitleField->SetText(entry->title.String());
		fUsernameField->SetText(entry->username.String());
		fPasswordField->SetText(entry->password.String());
		fUrlField->SetText(entry->url.String());
		fNotesView->SetText(entry->notes.String());
	}

	fPasswordField->SetTarget(this);

	// Password label (FieldView doesn't have CreateLabelLayoutItem)
	BStringView* passwordLabel = new BStringView("passLabel", "Password:");

	// Layout
	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_HALF_ITEM_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGrid(B_USE_HALF_ITEM_SPACING, B_USE_HALF_ITEM_SPACING)
			.Add(fTitleField->CreateLabelLayoutItem(), 0, 0)
			.Add(fTitleField->CreateTextViewLayoutItem(), 1, 0, 2, 1)
			.Add(fGroupField->CreateLabelLayoutItem(), 0, 1)
			.Add(fGroupField->CreateMenuBarLayoutItem(), 1, 1, 2, 1)
			.Add(fUsernameField->CreateLabelLayoutItem(), 0, 2)
			.Add(fUsernameField->CreateTextViewLayoutItem(), 1, 2, 2, 1)
			.Add(passwordLabel, 0, 3)
			.Add(fPasswordField, 1, 3)
			.Add(fGenerateButton, 2, 3)
			.Add(fUrlField->CreateLabelLayoutItem(), 0, 4)
			.Add(fUrlField->CreateTextViewLayoutItem(), 1, 4, 2, 1)
			.SetColumnWeight(1, 1.0)
		.End()
		.Add(notesLabel)
		.Add(notesScroll, 2.0)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fCancelButton)
			.Add(fSaveButton)
		.End()
	.End();

	fTitleField->MakeFocus(true);
	center_over(this, fTarget);
}


EntryEditWindow::~EntryEditWindow()
{
}


void
EntryEditWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgEntrySave:
			_Save();
			break;

		case kMsgEntryCancel:
			Quit();
			break;

		case kMsgTogglePassword:
		{
			bool hidden = fPasswordField->IsHideTyping();
			fPasswordField->SetHideTyping(!hidden);
			fPasswordField->SetButtonIcon(kMsgTogglePassword,
				hidden ? kIconShow : kIconHide);
			break;
		}

		case kMsgGroupChosen:
		{
			kura_id groupId;
			if (message->FindInt32("groupId", &groupId) == B_OK)
				fGroupId = groupId;
			break;
		}

		case kMsgOpenGenerator:
		{
			PasswordGeneratorWindow* generator
				= new PasswordGeneratorWindow(
					BRect(0, 0, 380, 300), BMessenger(this));
			center_over(generator, this);
			generator->Show();
			break;
		}

		case kMsgUsePassword:
		{
			const char* password;
			if (message->FindString("password", &password) == B_OK)
				fPasswordField->SetText(password);
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
EntryEditWindow::QuitRequested()
{
	return true;
}


void
EntryEditWindow::_Save()
{
	BString title(fTitleField->Text());
	if (title.Length() == 0) {
		fTitleField->MakeFocus(true);
		return;
	}

	KuraEntry entry;
	entry.title = title;
	entry.groupId = fGroupId;
	entry.username = fUsernameField->Text();
	entry.password = fPasswordField->Text();
	entry.url = fUrlField->Text();
	entry.notes = fNotesView->Text();

	if (fEntryId != kNoId) {
		// Update existing
		fDatabase->UpdateEntry(fEntryId, entry);
	} else {
		// Add new
		fEntryId = fDatabase->AddEntry(entry);
	}

	// Notify the main window to refresh
	BMessage msg(kMsgEntrySave);
	msg.AddInt32("entryId", fEntryId);
	fTarget->PostMessage(&msg);

	Quit();
}


void
EntryEditWindow::_PopulateGroupMenu()
{
	BMenu* menu = fGroupField->Menu();
	menu->SetLabelFromMarked(true);

	// Entries without a subgroup live directly in "Root"
	BMessage* noneMsg = new BMessage(kMsgGroupChosen);
	noneMsg->AddInt32("groupId", kAllGroupId);
	BMenuItem* noneItem = new BMenuItem("Root", noneMsg);
	menu->AddItem(noneItem);

	if (fGroupId == kAllGroupId)
		noneItem->SetMarked(true);

	menu->AddSeparatorItem();

	// Add all groups
	for (int32 i = 0; i < fDatabase->CountGroups(); i++) {
		const KuraGroup* group = fDatabase->GroupAt(i);

		BMessage* msg = new BMessage(kMsgGroupChosen);
		msg->AddInt32("groupId", group->id);

		BString label;
		label << group->name;
		BMenuItem* item = new BMenuItem(label.String(), msg);
		menu->AddItem(item);

		if (group->id == fGroupId)
			item->SetMarked(true);
	}
}
