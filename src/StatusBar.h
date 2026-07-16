/*
 * StatusBar.h
 * Kura - Password Manager for Haiku
 *
 * Bottom status bar. Exactly as tall as a horizontal scroll bar so
 * it lines up flush with the document window's resize knob, and
 * draws its own top border line as its first pixel row - stacking a
 * separate BSeparatorView on top would sit 2px above the knob's top
 * edge. The right end stays clear of the knob.
 */
#ifndef STATUS_BAR_H
#define STATUS_BAR_H

#include <String.h>
#include <View.h>


class StatusBar : public BView {
public:
						StatusBar();

	virtual void		AttachedToWindow();
	virtual void		Draw(BRect updateRect);

	virtual BSize		MinSize();
	virtual BSize		MaxSize();
	virtual BSize		PreferredSize();

	void				SetStatusText(const char* text);
	void				SetCountText(const char* text);

private:
	BString				fStatusText;
	BString				fCountText;
};

#endif // STATUS_BAR_H
