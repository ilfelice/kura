/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Detail panel implementation showing entry fields.
 */


#include "DetailView.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextView.h>

#include "FieldView.h"
#include "KuraClipboard.h"
#include "KuraDefs.h"
#include "KuraUtils.h"


// Resource IDs (must match Kura.rdef)
enum {
	kIconCopy	= 10,
	kIconHide	= 11,
	kIconShow	= 12,
};

// Internal messages
enum {
	kMsgTogglePassword		= 'tpwd',
	kMsgCopyUser			= 'cpus',
	kMsgCopyPass			= 'cpps',
};


DetailView::DetailView()
	:
	BView("detailView", B_WILL_DRAW | B_FRAME_EVENTS),
	fDatabase(NULL),
	fPasswordVisible(false),
	fCurrentEntryId(kNoId)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	// Title
	fTitleView = new BStringView("title", "");
	BFont titleFont(be_bold_font);
	titleFont.SetSize(titleFont.Size() * 1.2);
	fTitleView->SetFont(&titleFont);

	fGroupPathView = new BStringView("groupPath", "");
	fGroupPathView->SetHighUIColor(B_PANEL_TEXT_COLOR, 0.6);
	BFont smallFont(be_plain_font);
	smallFont.SetSize(smallFont.Size() * 0.9);
	fGroupPathView->SetFont(&smallFont);

	// Username field with copy button inside
	fUsernameLabel = new BStringView("userLabel", "Username:");
	fUsernameLabel->SetHighUIColor(B_PANEL_TEXT_COLOR, 0.6);
	fUsernameLabel->SetExplicitAlignment(
		BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));
	fUsernameField = new FieldView("userField");
	fUsernameField->AddButton(kIconCopy, kMsgCopyUser);

	// Password field with show/hide and copy buttons inside
	fPasswordLabel = new BStringView("passLabel", "Password:");
	fPasswordLabel->SetHighUIColor(B_PANEL_TEXT_COLOR, 0.6);
	fPasswordLabel->SetExplicitAlignment(
		BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));
	fPasswordField = new FieldView("passField");
	fPasswordField->AddButton(kIconHide, kMsgTogglePassword);
	fPasswordField->AddButton(kIconCopy, kMsgCopyPass);

	// URL field - clickable, no buttons
	fUrlLabel = new BStringView("urlLabel", "URL:");
	fUrlLabel->SetHighUIColor(B_PANEL_TEXT_COLOR, 0.6);
	fUrlLabel->SetExplicitAlignment(
		BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));
	fUrlField = new FieldView("urlField");
	fUrlField->SetClickable(true);

	// Timestamps
	fCreatedView = new BStringView("created", "");
	fCreatedView->SetFont(&smallFont);
	fCreatedView->SetHighUIColor(B_PANEL_TEXT_COLOR, 0.5);
	fModifiedView = new BStringView("modified", "");
	fModifiedView->SetFont(&smallFont);
	fModifiedView->SetHighUIColor(B_PANEL_TEXT_COLOR, 0.5);

	// Notes
	fNotesLabel = new BStringView("notesLabel", "Notes:");
	fNotesLabel->SetHighUIColor(B_PANEL_TEXT_COLOR, 0.6);
	fNotesLabel->SetExplicitAlignment(
		BAlignment(B_ALIGN_LEFT, B_ALIGN_TOP));

	fNotesView = new BTextView("notes");
	fNotesView->MakeEditable(false);
	fNotesView->SetStylable(false);
	fNotesView->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	BScrollView* notesScroll = new BScrollView("notesScroll", fNotesView,
		0, false, true);

	// Layout - header (title + group path), grid with labels in the
	// left column and fields in the right column, timestamps at the
	// bottom.
	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_HALF_ITEM_SPACING)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(fTitleView)
		.Add(fGroupPathView)
		.AddStrut(B_USE_HALF_ITEM_SPACING)
		.AddGrid(B_USE_HALF_ITEM_SPACING, B_USE_HALF_ITEM_SPACING)
			.Add(fUsernameLabel, 0, 0)
			.Add(fUsernameField, 1, 0)
			.Add(fPasswordLabel, 0, 1)
			.Add(fPasswordField, 1, 1)
			.Add(fUrlLabel, 0, 2)
			.Add(fUrlField, 1, 2)
			.Add(fNotesLabel, 0, 3)
			.Add(notesScroll, 1, 3)
			.SetColumnWeight(1, 1.0)
			.SetRowWeight(3, 2.0)
		.End()
		.AddGroup(B_HORIZONTAL)
			.Add(fCreatedView)
			.AddGlue()
			.Add(fModifiedView)
		.End()
	.End();

	// Start with everything cleared
	ShowEntry(NULL);
}


DetailView::~DetailView()
{
	// Scrub password from memory
	scrub_string(fActualPassword);
}


void
DetailView::AttachedToWindow()
{
	BView::AttachedToWindow();
	fUsernameField->SetTarget(this);
	fPasswordField->SetTarget(this);
}


void
DetailView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgCopyUser:
			KuraClipboard::CopyWithTimedClear(fUsernameField->Text());
			break;

		case kMsgCopyPass:
			KuraClipboard::CopyWithTimedClear(fActualPassword.String());
			break;

		case kMsgTogglePassword:
			TogglePasswordVisible();
			break;

		default:
			BView::MessageReceived(message);
			break;
	}
}


void
DetailView::SetDatabase(KuraDatabase* db)
{
	fDatabase = db;
}


BString
DetailView::_GroupPath(kura_id groupId) const
{
	BString path;
	if (fDatabase == NULL)
		return path;

	kura_id current = groupId;
	for (int depth = 0; depth < 20; depth++) {
		if (current == kAllGroupId || current == kNoId)
			break;
		const KuraGroup* group = fDatabase->GroupById(current);
		if (group == NULL)
			break;
		if (path.Length() > 0)
			path.Prepend(" ▸ ");
		path.Prepend(group->name);
		current = group->parentId;
	}
	return path;
}


void
DetailView::ShowEntry(const KuraEntry* entry)
{
	// Hide password when switching entries
	fPasswordVisible = false;
	fPasswordField->SetButtonIcon(kMsgTogglePassword, kIconHide);

	if (entry == NULL) {
		fCurrentEntryId = kNoId;
		fTitleView->SetText("No entry selected");
		fGroupPathView->SetText("");
		fUsernameField->SetText("—");
		fPasswordField->SetText("—");
		fUrlField->SetText("");
		fCreatedView->SetText("");
		fModifiedView->SetText("");
		fNotesView->SetText("");

		// Scrub stored password
		scrub_string(fActualPassword);
		fActualPassword = "";

		// Disable fields and dim labels
		fUsernameField->SetEnabled(false);
		fPasswordField->SetEnabled(false);
		fUrlField->SetEnabled(false);
		fNotesView->MakeSelectable(false);
		fUsernameLabel->SetHighUIColor(B_PANEL_TEXT_COLOR, 0.6);
		fPasswordLabel->SetHighUIColor(B_PANEL_TEXT_COLOR, 0.6);
		fUrlLabel->SetHighUIColor(B_PANEL_TEXT_COLOR, 0.6);
		fNotesLabel->SetHighUIColor(B_PANEL_TEXT_COLOR, 0.6);
		return;
	}

	fCurrentEntryId = entry->id;

	fTitleView->SetText(entry->title.String());
	BString groupPath = _GroupPath(entry->groupId);
	if (groupPath.Length() == 0)
		groupPath = "Root";
	fGroupPathView->SetText(groupPath.String());

	// Use default text color for labels when entry is selected
	fUsernameLabel->SetHighUIColor(B_PANEL_TEXT_COLOR);
	fPasswordLabel->SetHighUIColor(B_PANEL_TEXT_COLOR);
	fUrlLabel->SetHighUIColor(B_PANEL_TEXT_COLOR);
	fNotesLabel->SetHighUIColor(B_PANEL_TEXT_COLOR);
	fUsernameLabel->Invalidate();
	fPasswordLabel->Invalidate();
	fUrlLabel->Invalidate();
	fNotesLabel->Invalidate();

	fUsernameField->SetText(entry->username.String());
	fUsernameField->SetEnabled(true);

	// Store actual password and show masked version
	fActualPassword = entry->password;
	BString masked;
	for (int i = 0; i < entry->password.Length() && i < 20; i++)
		masked << "•";
	fPasswordField->SetText(masked.String());
	fPasswordField->SetEnabled(true);

	fUrlField->SetText(entry->url.String());
	fUrlField->SetEnabled(true);

	// Format timestamps
	char timeStr[64];
	struct tm* tm;

	tm = localtime(&entry->createdAt);
	if (tm != NULL) {
		strftime(timeStr, sizeof(timeStr), "Created: %Y-%m-%d %H:%M", tm);
		fCreatedView->SetText(timeStr);
	}

	tm = localtime(&entry->modifiedAt);
	if (tm != NULL) {
		strftime(timeStr, sizeof(timeStr), "Modified: %Y-%m-%d %H:%M", tm);
		fModifiedView->SetText(timeStr);
	}

	fNotesView->SetText(entry->notes.String());
	fNotesView->MakeSelectable(true);
}


void
DetailView::TogglePasswordVisible()
{
	fPasswordVisible = !fPasswordVisible;

	if (fPasswordVisible) {
		fPasswordField->SetText(fActualPassword.String());
		fPasswordField->SetButtonIcon(kMsgTogglePassword, kIconShow);
	} else {
		BString masked;
		for (int i = 0; i < fActualPassword.Length() && i < 20; i++)
			masked << "•";
		fPasswordField->SetText(masked.String());
		fPasswordField->SetButtonIcon(kMsgTogglePassword, kIconHide);
	}
}


