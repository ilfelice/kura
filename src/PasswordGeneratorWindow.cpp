/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Password generator implementation.
 */


#include "PasswordGeneratorWindow.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <openssl/rand.h>

#include <Button.h>
#include <CheckBox.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <Slider.h>
#include <SpaceLayoutItem.h>
#include <StringView.h>
#include <TextControl.h>
#include <UTF8.h>

#include "KuraClipboard.h"
#include "KuraDefs.h"
#include "KuraUtils.h"


// Internal messages
enum {
	kMsgOptionsChanged	= 'popt',
	kMsgGenerate		= 'pgen',
	kMsgCopy			= 'pcpy',
	kMsgUse				= 'puse',
};

static const char* kLowerChars   = "abcdefghijklmnopqrstuvwxyz";
static const char* kUpperChars   = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char* kDigitChars   = "0123456789";
static const char* kSymbolChars  = "!@#$%^&*()-_=+[]{};:,.<>?/~";
static const char* kAmbiguousChars = "l1IO0o|";

static const int32 kMinLength = 4;
static const int32 kMaxLength = 64;
static const int32 kDefaultLength = 20;


PasswordGeneratorWindow::PasswordGeneratorWindow(BRect frame,
	BMessenger target)
	:
	BWindow(frame, "Password generator",
		target.IsValid() ? B_FLOATING_WINDOW_LOOK : B_TITLED_WINDOW_LOOK,
		target.IsValid() ? B_MODAL_APP_WINDOW_FEEL : B_NORMAL_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
	fTarget(target),
	fHasTarget(target.IsValid())
{
	// Length slider
	BString lengthLabel;
	lengthLabel.SetToFormat("Length: %d", static_cast<int>(kDefaultLength));
	fLengthSlider = new BSlider("length", lengthLabel.String(),
		new BMessage(kMsgOptionsChanged), kMinLength, kMaxLength,
		B_HORIZONTAL);
	fLengthSlider->SetValue(kDefaultLength);
	fLengthSlider->SetModificationMessage(
		new BMessage(kMsgOptionsChanged));
	fLengthSlider->SetLimitLabels("4", "64");
	fLengthSlider->SetExplicitMinSize(BSize(320, B_SIZE_UNSET));

	// Character class checkboxes
	fLowerBox = new BCheckBox("lower", "Lowercase (a-z)",
		new BMessage(kMsgOptionsChanged));
	fLowerBox->SetValue(B_CONTROL_ON);
	fUpperBox = new BCheckBox("upper", "Uppercase (A-Z)",
		new BMessage(kMsgOptionsChanged));
	fUpperBox->SetValue(B_CONTROL_ON);
	fDigitsBox = new BCheckBox("digits", "Digits (0-9)",
		new BMessage(kMsgOptionsChanged));
	fDigitsBox->SetValue(B_CONTROL_ON);
	fSymbolsBox = new BCheckBox("symbols", "Symbols (!@#$" B_UTF8_ELLIPSIS ")",
		new BMessage(kMsgOptionsChanged));
	fSymbolsBox->SetValue(B_CONTROL_ON);
	fAmbiguousBox = new BCheckBox("ambiguous",
		"Exclude look-alikes (l1IO0o|)",
		new BMessage(kMsgOptionsChanged));

	// Result
	fResultField = new BTextControl("result", NULL, "", NULL);

	fEntropyView = new BStringView("entropy", "");
	BFont smallFont(be_plain_font);
	smallFont.SetSize(smallFont.Size() * 0.9);
	fEntropyView->SetFont(&smallFont);
	fEntropyView->SetHighUIColor(B_PANEL_TEXT_COLOR, 0.6);

	// Buttons
	fGenerateButton = new BButton("generate", "Generate",
		new BMessage(kMsgGenerate));
	fCopyButton = new BButton("copy", "Copy",
		new BMessage(kMsgCopy));
	fUseButton = NULL;
	if (fHasTarget) {
		fUseButton = new BButton("use", "Use",
			new BMessage(kMsgUse));
		fUseButton->MakeDefault(true);
	} else
		fGenerateButton->MakeDefault(true);

	// Button row - the "Use" button only exists in target mode, so
	// this row is built explicitly.
	BGroupView* buttonRow = new BGroupView(B_HORIZONTAL);
	BGroupLayout* buttons = buttonRow->GroupLayout();
	buttons->AddView(fGenerateButton);
	buttons->AddItem(BSpaceLayoutItem::CreateGlue());
	buttons->AddView(fCopyButton);
	if (fUseButton != NULL)
		buttons->AddView(fUseButton);

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_HALF_ITEM_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(fLengthSlider)
		.AddGrid(B_USE_ITEM_SPACING, 0)
			.Add(fLowerBox, 0, 0)
			.Add(fUpperBox, 1, 0)
			.Add(fDigitsBox, 0, 1)
			.Add(fSymbolsBox, 1, 1)
		.End()
		.Add(fAmbiguousBox)
		.AddStrut(B_USE_HALF_ITEM_SPACING)
		.Add(fResultField)
		.Add(fEntropyView)
		.AddStrut(B_USE_HALF_ITEM_SPACING)
		.Add(buttonRow)
	.End();

	_Generate();

	CenterOnScreen();
}


PasswordGeneratorWindow::~PasswordGeneratorWindow()
{
	// Best-effort scrub of the last generated password
	BString last(fResultField->Text());
	scrub_string(last);
	fResultField->SetText("");
}


void
PasswordGeneratorWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgOptionsChanged:
		{
			// Keep at least one character class enabled
			if (fLowerBox->Value() == B_CONTROL_OFF
				&& fUpperBox->Value() == B_CONTROL_OFF
				&& fDigitsBox->Value() == B_CONTROL_OFF
				&& fSymbolsBox->Value() == B_CONTROL_OFF) {
				fLowerBox->SetValue(B_CONTROL_ON);
			}

			BString label;
			label.SetToFormat("Length: %d",
				static_cast<int>(fLengthSlider->Value()));
			fLengthSlider->SetLabel(label.String());

			_Generate();
			break;
		}

		case kMsgGenerate:
			_Generate();
			break;

		case kMsgCopy:
			KuraClipboard::CopyWithTimedClear(fResultField->Text());
			fEntropyView->SetText("Copied to clipboard.");
			break;

		case kMsgUse:
		{
			BMessage use(kMsgUsePassword);
			use.AddString("password", fResultField->Text());
			fTarget.SendMessage(&use);
			Quit();
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


BString
PasswordGeneratorWindow::_BuildPool() const
{
	BString pool;
	if (fLowerBox->Value() == B_CONTROL_ON)
		pool << kLowerChars;
	if (fUpperBox->Value() == B_CONTROL_ON)
		pool << kUpperChars;
	if (fDigitsBox->Value() == B_CONTROL_ON)
		pool << kDigitChars;
	if (fSymbolsBox->Value() == B_CONTROL_ON)
		pool << kSymbolChars;

	if (fAmbiguousBox->Value() == B_CONTROL_ON) {
		for (const char* c = kAmbiguousChars; *c != '\0'; c++)
			pool.RemoveAll(BString(c, 1));
	}

	return pool;
}


status_t
PasswordGeneratorWindow::_RandomIndex(uint32 range, uint32& out) const
{
	if (range == 0)
		return B_BAD_VALUE;

	// Rejection sampling: discard values that would introduce
	// modulo bias.
	uint32 limit = UINT32_MAX - (UINT32_MAX % range);
	uint32 r;
	do {
		if (RAND_bytes((uint8*)&r, sizeof(r)) != 1)
			return B_ERROR;
	} while (r >= limit);

	out = r % range;
	return B_OK;
}


void
PasswordGeneratorWindow::_Generate()
{
	BString pool = _BuildPool();
	int32 length = fLengthSlider->Value();

	if (pool.Length() == 0) {
		fResultField->SetText("");
		fEntropyView->SetText("Select at least one character class.");
		return;
	}

	// Which classes must appear at least once?
	struct ClassCheck {
		bool wanted;
		const char* chars;
	};
	ClassCheck classes[] = {
		{ fLowerBox->Value() == B_CONTROL_ON, kLowerChars },
		{ fUpperBox->Value() == B_CONTROL_ON, kUpperChars },
		{ fDigitsBox->Value() == B_CONTROL_ON, kDigitChars },
		{ fSymbolsBox->Value() == B_CONTROL_ON, kSymbolChars },
	};
	int32 wantedClasses = 0;
	for (int i = 0; i < 4; i++) {
		if (classes[i].wanted)
			wantedClasses++;
	}

	BString password;
	bool valid = false;

	// Regenerate until every selected class is represented
	// (only enforceable when the password is long enough).
	for (int attempt = 0; attempt < 128 && !valid; attempt++) {
		password = "";
		for (int32 i = 0; i < length; i++) {
			uint32 index;
			if (_RandomIndex(static_cast<uint32>(pool.Length()), index) != B_OK) {
				fResultField->SetText("");
				fEntropyView->SetText("Random generator failure.");
				return;
			}
			password << pool[index];
		}

		if (length < wantedClasses) {
			valid = true;
			break;
		}

		valid = true;
		for (int i = 0; i < 4 && valid; i++) {
			if (!classes[i].wanted)
				continue;
			bool found = false;
			for (int32 j = 0; j < password.Length() && !found; j++) {
				if (strchr(classes[i].chars, password[j]) != NULL)
					found = true;
			}
			if (!found)
				valid = false;
		}
	}

	fResultField->SetText(password.String());
	scrub_string(password);

	_UpdateEntropyLabel();
}


void
PasswordGeneratorWindow::_UpdateEntropyLabel()
{
	BString pool = _BuildPool();
	int32 length = fLengthSlider->Value();

	if (pool.Length() == 0) {
		fEntropyView->SetText("");
		return;
	}

	double entropy = length * log2((double)pool.Length());

	BString text;
	const char* rating;
	if (entropy < 50)
		rating = "weak";
	else if (entropy < 80)
		rating = "reasonable";
	else if (entropy < 112)
		rating = "strong";
	else
		rating = "excellent";

	text.SetToFormat("Entropy: ~%.0f bits (%s)", entropy, rating);
	fEntropyView->SetText(text.String());
}
