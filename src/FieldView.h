/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * A composite view that looks like a single text field with embedded
 * icon buttons on the right side. Draws icons manually with alpha
 * compositing for proper transparency.
 *
 * Supports two modes:
 *   - Read-only (default): displays text with optional clickable URL
 *   - Editable: embeds a BTextView for text input, with optional
 *     hide-typing (password masking)
 *
 * Icons are stored as raw HVIF vector data and rendered at the
 * appropriate size for the current font, ensuring crisp display
 * at any font size.
 */

#ifndef FIELD_VIEW_H
#define FIELD_VIEW_H

#include <String.h>
#include <View.h>

class BBitmap;
class BHandler;
class BTextView;


class FieldView : public BView {
public:
						FieldView(const char* name);
	virtual				~FieldView();

	virtual void		Draw(BRect updateRect);
	virtual void		AttachedToWindow();
	virtual void		FrameResized(float width, float height);
	virtual void		MouseDown(BPoint where);
	virtual void		MouseMoved(BPoint where, uint32 transit,
							const BMessage* dragMessage);
	virtual void		MessageReceived(BMessage* message);
	virtual BSize		MinSize();
	virtual BSize		MaxSize();
	virtual BSize		PreferredSize();

	void				SetText(const char* text);
	const char*			Text() const;

	void				SetEnabled(bool enabled);
	bool				IsEnabled() const { return fEnabled; }

	// Editable mode: embeds a BTextView for input
	void				SetEditable(bool editable);
	bool				IsEditable() const { return fEditable; }

	// Password masking (only meaningful in editable mode)
	void				SetHideTyping(bool hide);
	bool				IsHideTyping() const { return fHideTyping; }

	// Add an icon button using a resource ID (loaded from app resources).
	// msgWhat is sent to the target when clicked.
	void				AddButton(int32 resourceId, uint32 msgWhat);

	// Set the target handler for button messages
	void				SetTarget(BHandler* target);

	// Replace the icon for a previously added button (by message what)
	void				SetButtonIcon(uint32 msgWhat, int32 resourceId);

	// Clickable URL support
	void				SetClickable(bool clickable);

	// Focus the embedded text view (for editable mode)
	void				MakeFocus(bool focus = true);

private:
	BRect				_ButtonRect(int32 index) const;
	BRect				_TextRect() const;
	BRect				_BorderInterior() const;
	int32				_ButtonAt(BPoint where) const;
	void				_UpdateTextViewRect();
	void				_RenderIcons();
	float				_FieldHeight() const;
	float				_IconSize() const;

	BString				fText;
	bool				fEnabled;
	bool				fClickable;
	bool				fEditable;
	bool				fHideTyping;
	BHandler*			fTarget;

	BTextView*			fTextView;	// embedded editor (NULL when read-only)

	struct ButtonInfo {
		int32			resourceId;	// HVIF resource ID
		uint32			msgWhat;
		BBitmap*		bitmap;		// rendered at current icon size
	};
	ButtonInfo			fButtons[4];
	int32				fButtonCount;
	int32				fFlashButton;	// index of button being flashed, or -1
	float				fRenderedIconSize;	// size icons were last rendered at
};

#endif // FIELD_VIEW_H
