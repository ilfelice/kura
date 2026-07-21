/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Search field implementation.
 */


#include "SearchTextControl.h"

#include <math.h>

#include <ControlLook.h>
#include <Cursor.h>
#include <Invoker.h>
#include <Message.h>
#include <MessageFilter.h>
#include <Window.h>


// Filter installed on the internal text view. The text view sits on
// top of the control and has focus, so both Escape (key) and clicks
// on the clear button's area must be caught here rather than relying
// on view z-order:
//   - Escape clears the field.
//   - A mouse-down inside the clear button's rectangle clears the
//     field and is swallowed so the text view doesn't also react.
class InputFilter : public BMessageFilter {
public:
	InputFilter(SearchTextControl* owner)
		:
		BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE),
		fOwner(owner)
	{
	}

	virtual filter_result Filter(BMessage* message, BHandler**)
	{
		bool hasText = fOwner->Text() != NULL
			&& fOwner->Text()[0] != '\0';

		if (message->what == B_KEY_DOWN) {
			const char* bytes;
			if (hasText
				&& message->FindString("bytes", &bytes) == B_OK
				&& bytes[0] == B_ESCAPE) {
				fOwner->Clear();
				return B_SKIP_MESSAGE;
			}
		} else if (message->what == B_MOUSE_DOWN && hasText) {
			BPoint where;
			if (message->FindPoint("be:view_where", &where) == B_OK
				&& fOwner->ClearButtonFrame().Contains(where)) {
				fOwner->Clear();
				return B_SKIP_MESSAGE;
			}
		} else if (message->what == B_MOUSE_MOVED) {
			// The text view (below us in dispatch) force-sets the
			// I-beam in its own MouseMoved. The clear button view
			// never receives mouse events (the text view covers it
			// in the app_server's hit-testing), so the cursor must
			// be handled here. When over the button, set the arrow
			// and swallow the move so the text view's _TrackMouse
			// doesn't run and overwrite it.
			BPoint where;
			if (hasText
				&& message->FindPoint("be:view_where", &where) == B_OK
				&& fOwner->ClearButtonFrame().Contains(where)) {
				fOwner->SetClearCursor(true);
				return B_SKIP_MESSAGE;
			}
			fOwner->SetClearCursor(false);
		}
		return B_DISPATCH_MESSAGE;
	}

private:
	SearchTextControl*	fOwner;
};


// A small square button drawing an "x", layered over the text
// control's text view at its right edge. Hidden when the field is
// empty. Clicking clears the search field (its parent).
class ClearButton : public BView {
public:
	ClearButton(SearchTextControl* owner)
		:
		BView("clear", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fOwner(owner),
		fHot(false)
	{
	}

	virtual void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();

		rgb_color bg = ViewColor();
		SetHighColor(bg);
		FillRect(bounds);

		if (fHot) {
			be_control_look->DrawButtonBackground(this, bounds,
				updateRect, bg, BControlLook::B_HOVER);
		}

		BRect glyph = bounds;
		glyph.InsetBy(floorf(bounds.Width() / 3.0f),
			floorf(bounds.Height() / 3.0f));
		rgb_color cross = tint_color(bg, B_DARKEN_4_TINT);
		SetHighColor(cross);
		SetPenSize(ceilf(be_plain_font->Size() / 11.0f));
		StrokeLine(glyph.LeftTop(), glyph.RightBottom());
		StrokeLine(glyph.RightTop(), glyph.LeftBottom());
		SetPenSize(1.0f);
	}

	virtual void MouseDown(BPoint)
	{
		fOwner->Clear();
	}

	virtual void MouseMoved(BPoint where, uint32 code, const BMessage*)
	{
		bool hot = code != B_EXITED_VIEW && Bounds().Contains(where);
		if (hot != fHot) {
			fHot = hot;
			Invalidate();
		}
	}

	void SetColors(rgb_color viewColor)
	{
		SetViewColor(viewColor);
		SetLowColor(viewColor);
	}

private:
	SearchTextControl*	fOwner;
	bool				fHot;
};


SearchTextControl::SearchTextControl(const char* name,
	const char* label, BMessage* modificationMessage)
	:
	BTextControl(name, label, "", NULL),
	fClearButton(NULL),
	fForwardMessage(modificationMessage),
	fForwardTarget(),
	fClearCursor(false)
{
	// Intercept our own modification internally so the clear button
	// tracks every text change; the caller's message is forwarded
	// from MessageReceived() once the target is known.
	SetModificationMessage(new BMessage('_sch'));

	fClearButton = new ClearButton(this);
	AddChild(fClearButton);
	fClearButton->Hide();
}


SearchTextControl::~SearchTextControl()
{
	delete fForwardMessage;
}


void
SearchTextControl::AttachedToWindow()
{
	BTextControl::AttachedToWindow();

	// Deliver our own modification message to ourselves; the caller
	// sets the real forward target via SetTarget(). Fall back to the
	// window if none is set.
	if (!fForwardTarget.IsValid())
		fForwardTarget = BMessenger(Window());
	BInvoker::SetTarget(this);

	float size = ceilf(be_plain_font->Size());
	if (TextView() != NULL) {
		TextView()->SetInsets(2, 2, static_cast<int32>(size) + 4, 2);
		fClearButton->SetColors(TextView()->ViewColor());
		TextView()->AddFilter(new InputFilter(this));
	}
	_LayoutClearButton();
}


void
SearchTextControl::SetEnabled(bool enabled)
{
	BTextControl::SetEnabled(enabled);

	// BTextControl::SetEnabled() makes the text view non-editable
	// but leaves it selectable, so a click still drops a blinking
	// caret. Match the read-only look of the detail-pane fields by
	// also toggling selectability, and reset the cursor to normal.
	if (TextView() != NULL) {
		TextView()->MakeSelectable(enabled);
		if (!enabled)
			SetClearCursor(false);
	}
}


void
SearchTextControl::SetText(const char* text)
{
	BTextControl::SetText(text);
	_LayoutClearButton();
}


status_t
SearchTextControl::SetTarget(BMessenger messenger)
{
	// Remember where the caller wants modification notices sent, but
	// keep the control's own message coming to us so the clear
	// button can track changes; we forward from MessageReceived().
	fForwardTarget = messenger;
	return BInvoker::SetTarget(this);
}


status_t
SearchTextControl::SetTarget(const BHandler* handler,
	const BLooper* looper)
{
	fForwardTarget = BMessenger(handler, looper);
	return BInvoker::SetTarget(this);
}


void
SearchTextControl::DoLayout()
{
	BTextControl::DoLayout();
	_LayoutClearButton();
}


void
SearchTextControl::FrameResized(float newWidth, float newHeight)
{
	BTextControl::FrameResized(newWidth, newHeight);
	_LayoutClearButton();
}


// The clear button's rectangle in the control's coordinate space.
BRect
SearchTextControl::_ClearRectInControl() const
{
	if (TextView() == NULL)
		return BRect();
	BRect textFrame = TextView()->Frame();
	float size = textFrame.Height();
	BRect rect(0, 0, size, size);
	rect.OffsetTo(textFrame.right - size, textFrame.top);
	return rect;
}


// The same rectangle in the text view's own coordinate space, used
// by the mouse filter (whose points arrive in text-view coords).
BRect
SearchTextControl::ClearButtonFrame() const
{
	if (TextView() == NULL)
		return BRect();
	BRect bounds = TextView()->Bounds();
	float size = bounds.Height();
	BRect rect(0, 0, size, size);
	rect.OffsetTo(bounds.right - size, bounds.top);
	return rect;
}


void
SearchTextControl::_LayoutClearButton()
{
	if (fClearButton == NULL || TextView() == NULL)
		return;

	BRect rect = _ClearRectInControl();
	fClearButton->MoveTo(rect.LeftTop());
	fClearButton->ResizeTo(rect.Width(), rect.Height());

	bool hasText = Text() != NULL && Text()[0] != '\0';
	if (hasText && fClearButton->IsHidden())
		fClearButton->Show();
	else if (!hasText && !fClearButton->IsHidden()) {
		fClearButton->Hide();
		SetClearCursor(false);
	}
}


void
SearchTextControl::SetClearCursor(bool arrow)
{
	if (arrow == fClearCursor)
		return;
	fClearCursor = arrow;

	if (TextView() == NULL)
		return;
	if (arrow) {
		BCursor system(B_CURSOR_ID_SYSTEM_DEFAULT);
		TextView()->SetViewCursor(&system);
	} else {
		BCursor ibeam(B_CURSOR_ID_I_BEAM);
		TextView()->SetViewCursor(&ibeam);
	}
}


void
SearchTextControl::Clear()
{
	SetText("");	// updates the button via our SetText override
	if (fForwardMessage != NULL && fForwardTarget.IsValid()) {
		BMessage forward(*fForwardMessage);
		fForwardTarget.SendMessage(&forward);
	}
	MakeFocus(true);
}


void
SearchTextControl::MessageReceived(BMessage* message)
{
	if (message->what == '_sch') {
		// Our own modification message: update the button, then
		// forward the caller's message to the real target.
		_LayoutClearButton();
		if (fForwardMessage != NULL && fForwardTarget.IsValid()) {
			BMessage forward(*fForwardMessage);
			fForwardTarget.SendMessage(&forward);
		}
		return;
	}

	BTextControl::MessageReceived(message);
}
