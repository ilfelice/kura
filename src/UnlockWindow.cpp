/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Modal unlock dialog implementation.
 */


#include "UnlockWindow.h"

#include <string.h>

#include <Button.h>
#include <GridLayout.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TextControl.h>

#include "KuraDefs.h"
#include "KuraUtils.h"


UnlockWindow::UnlockWindow(BRect frame, unlock_mode mode,
	const char* dbPath, BWindow* target)
	:
	BWindow(frame,
		mode == UNLOCK_NEW ? "New Database" :
		mode == UNLOCK_CHANGE ? "Change password" :
		"Unlock Database",
		B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_CLOSE_ON_ESCAPE),
	fCurrentField(NULL),
	fConfirmField(NULL),
	fTarget(target),
	fMode(mode),
	fDbPath(dbPath),
	fSubmitted(false)
{
	// Current password field - only for change mode
	if (mode == UNLOCK_CHANGE) {
		fCurrentField = new BTextControl("current",
			"Current password:", "", NULL);
		fCurrentField->TextView()->HideTyping(true);
	}

	// (New) master password field with hidden text
	fPasswordField = new BTextControl("password",
		mode == UNLOCK_CHANGE ? "New password:" : "Master password:",
		"", NULL);
	fPasswordField->TextView()->HideTyping(true);

	// Confirm field - only for new/change modes
	if (mode == UNLOCK_NEW || mode == UNLOCK_CHANGE) {
		fConfirmField = new BTextControl("confirm",
			mode == UNLOCK_CHANGE ? "Confirm:" : "Confirm password:",
			"", NULL);
		fConfirmField->TextView()->HideTyping(true);
	}

	// Status text for errors
	fStatusView = new BStringView("status", "");
	fStatusView->SetHighUIColor(B_FAILURE_COLOR);

	// Path info
	BString pathInfo;
	if (dbPath != NULL && dbPath[0] != '\0')
		pathInfo << dbPath;
	else
		pathInfo << "(new database)";

	BStringView* pathView = new BStringView("path", pathInfo.String());
	pathView->SetHighUIColor(B_PANEL_TEXT_COLOR, 0.7);
	pathView->SetFontSize(10.0);

	// Buttons
	fCancelButton = new BButton("cancel", "Cancel",
		new BMessage(kMsgUnlockCancel));
	fUnlockButton = new BButton("unlock",
		mode == UNLOCK_NEW ? "Create" :
		mode == UNLOCK_CHANGE ? "Change" : "Unlock",
		new BMessage(kMsgUnlock));
	fUnlockButton->MakeDefault(true);

	// Layout. Labels and fields are placed in a grid so the text
	// controls align regardless of which fields the mode shows.
	// The path line and status line span both columns.
	BGridLayout* grid = new BGridLayout(B_USE_SMALL_SPACING,
		B_USE_SMALL_SPACING);
	BView* gridView = new BView("fields", 0);
	gridView->SetLayout(grid);

	int32 row = 0;
	grid->AddView(pathView, 0, row, 2, 1);
	row++;

	if (fCurrentField != NULL) {
		grid->AddItem(fCurrentField->CreateLabelLayoutItem(),
			0, row);
		grid->AddItem(fCurrentField->CreateTextViewLayoutItem(),
			1, row);
		row++;
	}

	grid->AddItem(fPasswordField->CreateLabelLayoutItem(), 0, row);
	grid->AddItem(fPasswordField->CreateTextViewLayoutItem(),
		1, row);
	row++;

	if (fConfirmField != NULL) {
		grid->AddItem(fConfirmField->CreateLabelLayoutItem(),
			0, row);
		grid->AddItem(fConfirmField->CreateTextViewLayoutItem(),
			1, row);
		row++;
	}

	grid->AddView(fStatusView, 0, row, 2, 1);

	// Labels column stays tight; the field column takes the slack
	grid->SetColumnWeight(0, 0.0f);
	grid->SetColumnWeight(1, 1.0f);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(gridView)
		.AddStrut(B_USE_ITEM_SPACING)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fCancelButton)
			.Add(fUnlockButton)
		.End()
	.End();

	if (fCurrentField != NULL)
		fCurrentField->MakeFocus(true);
	else
		fPasswordField->MakeFocus(true);

	center_over(this, fTarget);
}


UnlockWindow::~UnlockWindow()
{
}


void
UnlockWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgUnlock:
			_Submit();
			break;

		case kMsgUnlockCancel:
		{
			fSubmitted = true;	// don't cancel twice via QuitRequested
			BMessage cancel(kMsgUnlockCancel);
			fTarget->PostMessage(&cancel);
			Quit();
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


bool
UnlockWindow::QuitRequested()
{
	if (!fSubmitted) {
		BMessage cancel(kMsgUnlockCancel);
		fTarget->PostMessage(&cancel);
	}
	return true;
}


void
UnlockWindow::_Submit()
{
	BString password(fPasswordField->Text());

	if (password.Length() == 0) {
		fStatusView->SetText(fMode == UNLOCK_CHANGE
			? "Please enter a new password."
			: "Please enter a password.");
		fPasswordField->MakeFocus(true);
		return;
	}

	BString current;
	if (fMode == UNLOCK_CHANGE) {
		current = fCurrentField->Text();
		if (current.Length() == 0) {
			fStatusView->SetText("Please enter your current password.");
			fCurrentField->MakeFocus(true);
			return;
		}
	}

	if (fMode == UNLOCK_NEW || fMode == UNLOCK_CHANGE) {
		BString confirm(fConfirmField->Text());
		if (password != confirm) {
			fStatusView->SetText("Passwords do not match.");
			fConfirmField->SetText("");
			fConfirmField->MakeFocus(true);
			scrub_string(confirm);
			return;
		}
		scrub_string(confirm);

		if (password.Length() < 4) {
			fStatusView->SetText(
				"Password must be at least 4 characters.");
			return;
		}
	}

	// Send the password to the target window
	fSubmitted = true;
	BMessage msg(kMsgUnlock);
	msg.AddString("password", password.String());
	if (fMode == UNLOCK_CHANGE)
		msg.AddString("current", current.String());
	msg.AddInt32("mode", (int32)fMode);
	fTarget->PostMessage(&msg);

	// Scrub passwords from memory
	scrub_string(password);
	scrub_string(current);

	Quit();
}
