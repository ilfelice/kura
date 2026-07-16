/*
 * SearchTextControl.h
 * Kura - Password Manager for Haiku
 *
 * A BTextControl for search fields with a clickable clear "x" shown
 * inside the right edge when it contains text; Escape also clears
 * it. The "x" is a small child view layered over the control's own
 * text view (BTextControl's internal BTextView covers the parent,
 * so parent-side drawing there would be hidden). Haiku has no native
 * search widget, so this is a small self-contained one.
 */
#ifndef SEARCH_TEXT_CONTROL_H
#define SEARCH_TEXT_CONTROL_H

#include <Messenger.h>
#include <TextControl.h>

class ClearButton;


class SearchTextControl : public BTextControl {
public:
						SearchTextControl(const char* name,
							const char* label,
							BMessage* modificationMessage);
	virtual				~SearchTextControl();

	virtual void		AttachedToWindow();
	virtual void		SetEnabled(bool enabled);
	virtual void		SetText(const char* text);
	virtual status_t	SetTarget(BMessenger messenger);
	virtual status_t	SetTarget(const BHandler* handler,
							const BLooper* looper = NULL);
	virtual void		MessageReceived(BMessage* message);
	virtual void		FrameResized(float newWidth, float newHeight);
	virtual void		DoLayout();

	void				Clear();

	// Clear button rectangle in the text view's coordinate space
	// (used by the internal mouse filter).
	BRect				ClearButtonFrame() const;

	// Set the text view's cursor to arrow (over the button) or the
	// I-beam; driven by the mouse filter.
	void				SetClearCursor(bool arrow);

private:
	void				_LayoutClearButton();
	BRect				_ClearRectInControl() const;

	ClearButton*		fClearButton;
	BMessage*			fForwardMessage;
	BMessenger			fForwardTarget;
	bool				fClearCursor;
};

#endif // SEARCH_TEXT_CONTROL_H
