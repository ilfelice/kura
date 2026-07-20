/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Status bar implementation.
 */

#include "StatusBar.h"

#include <ControlLook.h>

#include <cmath>


StatusBar::StatusBar()
	:
	BView("statusBar", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
}


void
StatusBar::AttachedToWindow()
{
	BView::AttachedToWindow();

	BFont font(be_plain_font);
	font.SetSize(font.Size() * 0.9);
	SetFont(&font);
}


// The status bar must line up with the document window's resize
// knob. There is no public API for the knob's metrics, so this
// replicates how the default decorator computes it
// (TabDecorator::_DoLayout in the app_server):
//
//   scaleFactor = max(titleFontSize / 12, 1)
//   knobSize    = 18 * scaleFactor
//   borderWidth = int32(5 * scaleFactor)
//
// and the knob protrudes (knobSize - borderWidth) above the client
// area's bottom edge, covering knobSize - borderWidth + 1 pixel
// rows including its top edge. The window title font is the bold
// font unless changed through private settings.
//
// Note this deliberately does NOT follow the system scroll bars:
// scroll bars scale with the plain font via ComposeSpacing() while
// the knob scales with the title font as above, so the two disagree
// at non-default font sizes. The bar sides with the knob it sits
// next to.
static float
_KnobProtrusion()
{
	static float sProtrusion = -1;
	if (sProtrusion < 0) {
		float scale = max_c(be_bold_font->Size() / 12.0f, 1.0f);
		float knobSize = floorf(18.0f * scale);
		float borderWidth = (float)(int32)(5.0f * scale);
		sProtrusion = knobSize - borderWidth + 1;
	}
	return sProtrusion;
}


// Layout sizes use BRect's inclusive semantics (pixels - 1)
static float
_BarHeight()
{
	return _KnobProtrusion() - 1;
}


BSize
StatusBar::MinSize()
{
	return BSize(0, _BarHeight());
}


BSize
StatusBar::MaxSize()
{
	return BSize(B_SIZE_UNLIMITED, _BarHeight());
}


BSize
StatusBar::PreferredSize()
{
	return BSize(B_SIZE_UNLIMITED, _BarHeight());
}


// Uncomment to flood the status bar solid red: makes the exact
// extent of this view unmistakable in a Magnify screenshot, so its
// edges can be compared pixel-by-pixel against the decorator's
// resize knob and window frame.
//#define KURA_STATUSBAR_DEBUG

void
StatusBar::Draw(BRect updateRect)
{
	BRect bounds = Bounds();

#ifdef KURA_STATUSBAR_DEBUG
	SetHighColor(255, 0, 0);
	FillRect(bounds);
	return;
#endif

	rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);

	// Top border line: the first pixel row of the bar, level with
	// the top edge of the resize knob
	SetHighColor(tint_color(base, B_DARKEN_2_TINT));
	StrokeLine(bounds.LeftTop(), bounds.RightTop());

	float spacing = be_control_look->ComposeSpacing(
		B_USE_HALF_ITEM_SPACING);

	// Text is vertically centered in the area below the border line
	// (bounds.Height() + 1 pixels total, minus the 1px border)
	font_height fontHeight;
	GetFontHeight(&fontHeight);
	float areaHeight = bounds.Height();
	float baseline = 1 + floorf((areaHeight
			- (fontHeight.ascent + fontHeight.descent)) / 2
		+ fontHeight.ascent + 0.5f);

	SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
	SetLowColor(base);

	// Right-aligned count, kept clear of the resize knob
	float countLeft = bounds.right;
	if (fCountText.Length() > 0) {
		float width = StringWidth(fCountText.String());
		countLeft = bounds.right - _KnobProtrusion() - spacing - width;
		MovePenTo(countLeft, baseline);
		DrawString(fCountText.String());
	}

	// Status text on the left, truncated if it would run into the
	// count
	if (fStatusText.Length() > 0) {
		BString text(fStatusText);
		float available = countLeft - spacing * 2 - bounds.left
			- spacing;
		if (available > 0) {
			BFont font;
			GetFont(&font);
			font.TruncateString(&text, B_TRUNCATE_END, available);
			MovePenTo(bounds.left + spacing, baseline);
			DrawString(text.String());
		}
	}
}


void
StatusBar::SetStatusText(const char* text)
{
	fStatusText = text != NULL ? text : "";
	Invalidate();
}


void
StatusBar::SetCountText(const char* text)
{
	fCountText = text != NULL ? text : "";
	Invalidate();
}
