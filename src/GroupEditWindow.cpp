/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Group creation/editing dialog implementation.
 */


#include "GroupEditWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <TextControl.h>

#include "KuraDefs.h"
#include "KuraUtils.h"


// Internal messages
enum {
	kMsgParentChosen	= 'prch',
};


GroupEditWindow::GroupEditWindow(BRect frame, const KuraGroup* group,
	KuraDatabase* database, BWindow* target)
	:
	BWindow(frame,
		group != NULL ? "Edit Group" : "New Group",
		B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_CLOSE_ON_ESCAPE),
	fDatabase(database),
	fTarget(target),
	fGroupId(group != NULL ? group->id : kNoId),
	fParentId(group != NULL ? group->parentId : kNoId),
	fIcon("")
{
	fNameField = new BTextControl("name", "Name:", "", NULL);

	// Parent group menu
	BMenu* parentMenu = new BMenu("Parent");
	fParentField = new BMenuField("parent", "Parent:", parentMenu);
	_PopulateParentMenu();

	// Buttons
	fCancelButton = new BButton("cancel", "Cancel",
		new BMessage(kMsgGroupCancel));
	fSaveButton = new BButton("save", "Save",
		new BMessage(kMsgGroupSave));
	fSaveButton->MakeDefault(true);

	// Populate if editing
	if (group != NULL)
		fNameField->SetText(group->name.String());

	// Layout
	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_HALF_ITEM_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGrid(B_USE_HALF_ITEM_SPACING, B_USE_HALF_ITEM_SPACING)
			.Add(fNameField->CreateLabelLayoutItem(), 0, 0)
			.Add(fNameField->CreateTextViewLayoutItem(), 1, 0)
			.Add(fParentField->CreateLabelLayoutItem(), 0, 1)
			.Add(fParentField->CreateMenuBarLayoutItem(), 1, 1)
		.End()
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fCancelButton)
			.Add(fSaveButton)
		.End()
	.End();

	fNameField->MakeFocus(true);
	center_over(this, fTarget);
}


GroupEditWindow::~GroupEditWindow()
{
}


void
GroupEditWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgGroupSave:
			_Save();
			break;

		case kMsgGroupCancel:
			Quit();
			break;

		case kMsgParentChosen:
		{
			kura_id parentId;
			if (message->FindInt32("parentId", &parentId) == B_OK)
				fParentId = parentId;
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
GroupEditWindow::_Save()
{
	BString name(fNameField->Text());
	if (name.Length() == 0) {
		fNameField->MakeFocus(true);
		return;
	}

	KuraGroup group;
	group.name = name;
	group.icon = fIcon;
	group.parentId = fParentId;

	if (fGroupId != kNoId) {
		fDatabase->UpdateGroup(fGroupId, group);
	} else {
		fGroupId = fDatabase->AddGroup(group);
	}

	BMessage msg(kMsgGroupSave);
	msg.AddInt32("groupId", fGroupId);
	fTarget->PostMessage(&msg);

	Quit();
}


void
GroupEditWindow::_PopulateParentMenu()
{
	BMenu* menu = fParentField->Menu();
	menu->SetLabelFromMarked(true);

	// Top-level option
	BMessage* noneMsg = new BMessage(kMsgParentChosen);
	noneMsg->AddInt32("parentId", kNoId);
	BMenuItem* noneItem = new BMenuItem("(top level)", noneMsg);
	menu->AddItem(noneItem);
	if (fParentId == kNoId)
		noneItem->SetMarked(true);

	menu->AddSeparatorItem();

	// Add existing groups (except self and descendants)
	for (int32 i = 0; i < fDatabase->CountGroups(); i++) {
		const KuraGroup* group = fDatabase->GroupAt(i);

		// Don't allow setting self as parent
		if (group->id == fGroupId)
			continue;

		BMessage* msg = new BMessage(kMsgParentChosen);
		msg->AddInt32("parentId", group->id);

		BString label;
		label << group->name;
		BMenuItem* item = new BMenuItem(label.String(), msg);
		menu->AddItem(item);

		if (group->id == fParentId)
			item->SetMarked(true);
	}
}
