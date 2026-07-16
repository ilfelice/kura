/*
 * AboutWindow.h
 * Kura - Password Manager for Haiku
 * Distributed under the terms of the MIT License.
 * Copyright 2026 Il Felice.
 *
 * About dialog: application icon, name, version and a scrollable
 * credits/description area. Modeled on the standard Haiku about box.
 */
#ifndef ABOUT_WINDOW_H
#define ABOUT_WINDOW_H

#include <Window.h>

class BTextView;


class AboutWindow : public BWindow {
public:
						AboutWindow(BWindow* target);
	virtual				~AboutWindow();

	virtual void		MessageReceived(BMessage* message);

private:
	BTextView*			fTextView;
	BWindow*			fTarget;
};

#endif // ABOUT_WINDOW_H
