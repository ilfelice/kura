/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Custom composite field view implementation.
 * Draws icons manually with alpha compositing.
 */


#include "FieldView.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <Application.h>
#include <Bitmap.h>
#include <ControlLook.h>
#include <Cursor.h>
#include <IconUtils.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <Region.h>
#include <Resources.h>
#include <Roster.h>
#include <TextView.h>
#include <Window.h>

#include "KuraUtils.h"


static const float kButtonPad = 4.0;
static const float kTextInset = 4.0;
static const float kFieldVPad = 4.0;	// vertical padding above and below text

static const uint32 kMsgFlashDone = '_fld';
static const bigtime_t kFlashDuration = 150000; // 150ms

// Bullet character for password masking (UTF-8 BULLET U+2022)
static const char* kBullet = "•";


// Inner BTextView subclass that notifies the parent FieldView on focus changes
class FieldTextView : public BTextView {
public:
	FieldTextView(BRect frame, const char* name, BRect textRect,
		uint32 resizeMask, uint32 flags)
		:
		BTextView(frame, name, textRect, resizeMask, flags)
	{
	}

	virtual void MakeFocus(bool focus = true)
	{
		BTextView::MakeFocus(focus);
		// Redraw parent so it can update the focus ring
		if (Parent() != NULL)
			Parent()->Invalidate();
	}

	virtual void KeyDown(const char* bytes, int32 numBytes)
	{
		if (numBytes == 1 && bytes[0] == B_TAB) {
			// Pass Tab to the parent view for navigation
			BView* parent = Parent();
			if (parent != NULL)
				parent->KeyDown(bytes, numBytes);
			return;
		}
		BTextView::KeyDown(bytes, numBytes);
	}
};


FieldView::FieldView(const char* name)
	:
	BView(name, B_WILL_DRAW | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE),
	fEnabled(true),
	fClickable(false),
	fEditable(false),
	fHideTyping(false),
	fTarget(NULL),
	fTextView(NULL),
	fButtonCount(0),
	fFlashButton(-1),
	fRenderedIconSize(0)
{
	memset(fButtons, 0, sizeof(fButtons));
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
}


FieldView::~FieldView()
{
	for (int32 i = 0; i < fButtonCount; i++)
		delete fButtons[i].bitmap;
}


void
FieldView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	BRect fieldRect(bounds.left, bounds.top,
		bounds.right, bounds.top + _FieldHeight() - 1);

	// Draw field background
	rgb_color bgColor = ui_color(B_DOCUMENT_BACKGROUND_COLOR);
	if (!fEnabled) {
		bgColor = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_LIGHTEN_1_TINT);
	}

	// Draw the border using the system control look
	rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
	uint32 flags = 0;
	if (!fEnabled)
		flags |= BControlLook::B_DISABLED;
	if (fEditable && fTextView != NULL && fTextView->IsFocus())
		flags |= BControlLook::B_FOCUSED;

	SetDrawingMode(B_OP_COPY);
	be_control_look->DrawTextControlBorder(this, fieldRect, updateRect,
		base, flags);

	// Fill inside with background (fieldRect was modified by DrawTextControlBorder)
	SetHighColor(bgColor);
	FillRect(fieldRect);

	// Draw text (only in read-only mode; in editable mode the BTextView handles it)
	if (!fEditable) {
		BRect textRect = _TextRect();
		BFont font;
		GetFont(&font);

		font_height fh;
		font.GetHeight(&fh);
		float textY = textRect.top
			+ (textRect.Height() + fh.ascent - fh.descent) / 2.0;

		if (fClickable && fText.Length() > 0) {
			SetHighUIColor(B_LINK_TEXT_COLOR);
		} else {
			SetHighColor(fEnabled ? ui_color(B_DOCUMENT_TEXT_COLOR)
				: tint_color(ui_color(B_PANEL_TEXT_COLOR), B_DISABLED_LABEL_TINT));
		}

		// Clip text to text rect
		BRegion clipping;
		clipping.Include(textRect);
		ConstrainClippingRegion(&clipping);
		DrawString(fText.String(), BPoint(textRect.left, textY));
		ConstrainClippingRegion(NULL);
	}

	// Re-render icons if font size changed
	if (fButtonCount > 0 && fRenderedIconSize != _IconSize())
		const_cast<FieldView*>(this)->_RenderIcons();

	// Draw icon buttons with alpha compositing
	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	for (int32 i = 0; i < fButtonCount; i++) {
		BRect btnRect = _ButtonRect(i);

		// Draw flash highlight behind the icon
		if (i == fFlashButton) {
			rgb_color highlight = tint_color(
				ui_color(B_DOCUMENT_BACKGROUND_COLOR),
				B_DARKEN_2_TINT);
			SetDrawingMode(B_OP_COPY);
			SetHighColor(highlight);
			BRect hlRect = btnRect;
			hlRect.InsetBy(-2, -2);
			FillRoundRect(hlRect, 3, 3);
			SetDrawingMode(B_OP_ALPHA);
			SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		}

		if (fButtons[i].bitmap != NULL) {
			// Icon already rendered at correct size, draw 1:1
			float iconSz = _IconSize();
			float iconX = btnRect.left
				+ (btnRect.Width() - iconSz) / 2.0;
			float iconY = btnRect.top
				+ (btnRect.Height() - iconSz) / 2.0;
			DrawBitmap(fButtons[i].bitmap, BPoint(iconX, iconY));
		}
	}
	SetDrawingMode(B_OP_COPY);
}


void
FieldView::AttachedToWindow()
{
	BView::AttachedToWindow();

	// Create the text view now that we have a parent
	if (fEditable && fTextView == NULL) {
		BRect interior = _BorderInterior();
		// Leave room for buttons on the right
		float buttonsWidth = 0;
		for (int32 i = 0; i < fButtonCount; i++)
			buttonsWidth += _IconSize() + kButtonPad;
		if (fButtonCount > 0)
			buttonsWidth += kButtonPad;
		interior.right -= buttonsWidth;

		BRect tvTextRect(0, 0, interior.Width(), interior.Height());

		fTextView = new FieldTextView(interior, "fieldEditor", tvTextRect,
			B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP,
			B_WILL_DRAW | B_NAVIGABLE);
		fTextView->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		fTextView->SetWordWrap(false);

		AddChild(fTextView);

		// Use SetInsets for proper text padding inside the BTextView
		fTextView->SetInsets(kTextInset, 0, 0, 0);

		// Set text, then enable hide-typing, then restore text.
		fTextView->SetText(fText.String());
		if (fHideTyping) {
			fTextView->HideTyping(true);
			fTextView->SetText(fText.String());
		}

		fTextView->Select(0, 0);
	}
}


void
FieldView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgFlashDone:
			fFlashButton = -1;
			Invalidate();
			break;

		default:
			BView::MessageReceived(message);
			break;
	}
}


void
FieldView::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	_UpdateTextViewRect();
	Invalidate();
}


BSize
FieldView::MinSize()
{
	float width = 200.0;
	for (int32 i = 0; i < fButtonCount; i++)
		width += _IconSize() + kButtonPad;
	return BSize(width, _FieldHeight());
}


BSize
FieldView::MaxSize()
{
	return BSize(B_SIZE_UNLIMITED, _FieldHeight());
}


BSize
FieldView::PreferredSize()
{
	return BSize(250, _FieldHeight());
}


void
FieldView::SetText(const char* text)
{
	fText = text != NULL ? text : "";
	if (fTextView != NULL)
		fTextView->SetText(fText.String());
	Invalidate();
}


const char*
FieldView::Text() const
{
	if (fEditable && fTextView != NULL) {
		if (fHideTyping) {
			// When hide-typing is on, BTextView contains bullets.
			// The real text was saved in fText before hiding started,
			// but the user may have typed more since. BTextView::Text()
			// with HideTyping gives us the actual text.
			return fTextView->Text();
		}
		return fTextView->Text();
	}
	return fText.String();
}


void
FieldView::SetEnabled(bool enabled)
{
	fEnabled = enabled;
	if (fTextView != NULL)
		fTextView->MakeEditable(enabled);
	Invalidate();
}


void
FieldView::SetEditable(bool editable)
{
	if (fEditable == editable)
		return;

	fEditable = editable;

	if (editable && Window() != NULL && fTextView == NULL) {
		// Create the text view immediately if already attached
		BRect interior = _BorderInterior();
		float buttonsWidth = 0;
		for (int32 i = 0; i < fButtonCount; i++)
			buttonsWidth += _IconSize() + kButtonPad;
		if (fButtonCount > 0)
			buttonsWidth += kButtonPad;
		interior.right -= buttonsWidth;

		BRect tvTextRect(0, 0, interior.Width(), interior.Height());

		fTextView = new FieldTextView(interior, "fieldEditor", tvTextRect,
			B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP,
			B_WILL_DRAW | B_NAVIGABLE);
		fTextView->SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		fTextView->SetWordWrap(false);

		AddChild(fTextView);


		// Use SetInsets for proper text padding inside the BTextView
		fTextView->SetInsets(kTextInset, 0, 0, 0);
		fTextView->SetText(fText.String());
		if (fHideTyping) {
			fTextView->HideTyping(true);
			fTextView->SetText(fText.String());
		}
	} else if (!editable && fTextView != NULL) {
		// Grab text before removing
		fText = fTextView->Text();
		RemoveChild(fTextView);
		delete fTextView;
		fTextView = NULL;
	}

	Invalidate();
}


void
FieldView::SetHideTyping(bool hide)
{
	if (fHideTyping == hide)
		return;

	if (fTextView != NULL) {
		// Save the current real text before toggling
		BString realText = fTextView->Text();
		fTextView->HideTyping(hide);
		// Restore the text (HideTyping may have cleared it)
		fTextView->SetText(realText.String());
	}

	fHideTyping = hide;
	Invalidate();
}


void
FieldView::AddButton(int32 resourceId, uint32 msgWhat)
{
	if (fButtonCount >= 4)
		return;

	fButtons[fButtonCount].resourceId = resourceId;
	fButtons[fButtonCount].msgWhat = msgWhat;
	fButtons[fButtonCount].bitmap = NULL;
	fButtonCount++;

	fRenderedIconSize = 0;	// force re-render
	_UpdateTextViewRect();
	Invalidate();
}


void
FieldView::SetTarget(BHandler* target)
{
	fTarget = target;
}


void
FieldView::SetButtonIcon(uint32 msgWhat, int32 resourceId)
{
	for (int32 i = 0; i < fButtonCount; i++) {
		if (fButtons[i].msgWhat == msgWhat) {
			fButtons[i].resourceId = resourceId;
			delete fButtons[i].bitmap;
			fButtons[i].bitmap = NULL;
			fRenderedIconSize = 0;	// force re-render
			Invalidate();
			return;
		}
	}
}


void
FieldView::SetClickable(bool clickable)
{
	fClickable = clickable;
	Invalidate();
}


void
FieldView::MakeFocus(bool focus)
{
	if (fTextView != NULL)
		fTextView->MakeFocus(focus);
	else
		BView::MakeFocus(focus);
}


void
FieldView::MouseDown(BPoint where)
{
	// Check if a button was clicked
	int32 btnIndex = _ButtonAt(where);
	if (btnIndex >= 0 && fEnabled) {
		// Flash the button
		fFlashButton = btnIndex;
		Invalidate();

		// Schedule flash clear
		BMessage flashMsg(kMsgFlashDone);
		BMessageRunner* runner = new BMessageRunner(BMessenger(this),
			&flashMsg, kFlashDuration, 1);
		(void)runner;

		// Send the button's message
		BHandler* target = fTarget;
		if (target == NULL)
			target = this;
		BMessage msg(fButtons[btnIndex].msgWhat);
		BMessenger(target).SendMessage(&msg);
		return;
	}

	// Check clickable URL
	if (fClickable && fText.Length() > 0) {
		BRect textRect = _TextRect();
		if (textRect.Contains(where)) {
			open_url(fText.String());
			return;
		}
	}

	// In editable mode, clicking the text area focuses the text view
	if (fEditable && fTextView != NULL) {
		BRect textRect = _TextRect();
		if (textRect.Contains(where)) {
			fTextView->MakeFocus(true);
			return;
		}
	}

	BView::MouseDown(where);
}


void
FieldView::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	bool overClickable = false;

	if (fClickable && fText.Length() > 0) {
		BRect textRect = _TextRect();
		if (textRect.Contains(where))
			overClickable = true;
	}

	if (fEnabled && _ButtonAt(where) >= 0)
		overClickable = true;

	if (overClickable &&
		(transit == B_ENTERED_VIEW || transit == B_INSIDE_VIEW)) {
		BCursor linkCursor(B_CURSOR_ID_FOLLOW_LINK);
		SetViewCursor(&linkCursor);
	} else if (transit == B_EXITED_VIEW ||
		(!overClickable && transit == B_INSIDE_VIEW)) {
		BCursor normalCursor(B_CURSOR_ID_SYSTEM_DEFAULT);
		SetViewCursor(&normalCursor);
	}

	BView::MouseMoved(where, transit, dragMessage);
}


BRect
FieldView::_ButtonRect(int32 index) const
{
	BRect bounds = Bounds();
	float x = bounds.right - kButtonPad;

	// Buttons are laid out right-to-left
	for (int32 i = fButtonCount - 1; i >= 0; i--) {
		float btnWidth = _IconSize() + kButtonPad;
		x -= btnWidth;
		if (i == index) {
			float y = bounds.top + (_FieldHeight() - _IconSize()) / 2.0;
			return BRect(x, y, x + _IconSize(), y + _IconSize());
		}
	}
	return BRect();
}


BRect
FieldView::_TextRect() const
{
	BRect bounds = Bounds();
	float buttonsWidth = 0;
	for (int32 i = 0; i < fButtonCount; i++)
		buttonsWidth += _IconSize() + kButtonPad;

	return BRect(bounds.left + kTextInset, bounds.top + 2,
		bounds.right - buttonsWidth - kTextInset,
		bounds.top + _FieldHeight() - 3);
}


int32
FieldView::_ButtonAt(BPoint where) const
{
	for (int32 i = 0; i < fButtonCount; i++) {
		BRect btnRect = _ButtonRect(i);
		// Expand hit area slightly for easier clicking
		btnRect.InsetBy(-2, -2);
		if (btnRect.Contains(where))
			return i;
	}
	return -1;
}


void
FieldView::_UpdateTextViewRect()
{
	if (fTextView == NULL)
		return;

	BRect interior = _BorderInterior();
	float buttonsWidth = 0;
	for (int32 i = 0; i < fButtonCount; i++)
		buttonsWidth += _IconSize() + kButtonPad;
	if (fButtonCount > 0)
		buttonsWidth += kButtonPad;
	interior.right -= buttonsWidth;

	fTextView->MoveTo(interior.left, interior.top);
	fTextView->ResizeTo(interior.Width(), interior.Height());

	// Update the text rect inside the BTextView too
	BRect tvTextRect(0, 0, interior.Width(), interior.Height());
	fTextView->SetTextRect(tvTextRect);
}


void
FieldView::_RenderIcons()
{
	float iconSize = _IconSize();
	BResources* resources = be_app->AppResources();
	if (resources == NULL)
		return;

	for (int32 i = 0; i < fButtonCount; i++) {
		delete fButtons[i].bitmap;
		fButtons[i].bitmap = NULL;

		size_t dataSize;
		const void* data = resources->LoadResource(B_VECTOR_ICON_TYPE,
			fButtons[i].resourceId, &dataSize);
		if (data == NULL)
			continue;

		int32 sz = static_cast<int32>(iconSize);
		BBitmap* bitmap = new BBitmap(
			BRect(0, 0, sz - 1, sz - 1), B_RGBA32);
		if (BIconUtils::GetVectorIcon(
				static_cast<const uint8*>(data), dataSize, bitmap) == B_OK) {
			fButtons[i].bitmap = bitmap;
		} else {
			delete bitmap;
		}
	}

	fRenderedIconSize = iconSize;
}


BRect
FieldView::_BorderInterior() const
{
	BRect bounds = Bounds();
	BRect fieldRect(bounds.left, bounds.top,
		bounds.right, bounds.top + _FieldHeight() - 1);

	// DrawTextControlBorder insets by 2 pixels on each side
	fieldRect.InsetBy(2, 2);
	return fieldRect;
}


float
FieldView::_FieldHeight() const
{
	font_height fh;
	GetFontHeight(&fh);
	float textHeight = ceilf(fh.ascent + fh.descent + fh.leading);
	float height = textHeight + 2 * kFieldVPad;

	// Ensure icons fit
	if (height < _IconSize() + 2 * kFieldVPad)
		height = _IconSize() + 2 * kFieldVPad;

	return height;
}


float
FieldView::_IconSize() const
{
	font_height fh;
	GetFontHeight(&fh);
	// Scale icon to match font: use ascent + descent, rounded to even number
	float size = ceilf(fh.ascent + fh.descent);
	if (fmodf(size, 2.0) != 0)
		size += 1.0;
	// Clamp to reasonable range
	if (size < 12.0)
		size = 12.0;
	if (size > 64.0)
		size = 64.0;
	return size;
}
