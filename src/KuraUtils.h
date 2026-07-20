/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Small shared helpers.
 */
#ifndef KURA_UTILS_H
#define KURA_UTILS_H


#include <string.h>

#include <Roster.h>
#include <Screen.h>
#include <String.h>
#include <Window.h>


// Best-effort scrubbing of a BString's contents from memory.
// (BString may be copy-on-write; LockBuffer forces our own copy,
// so this scrubs at least the buffer we hold.)
static inline void
scrub_string(BString& s)
{
	int32 length = s.Length();
	if (length <= 0)
		return;

	char* buffer = s.LockBuffer(length);
	if (buffer != NULL)
		memset(buffer, 0, length);
	s.UnlockBuffer(0);
}


// Open a URL in the preferred browser. Prepends https:// when no
// scheme is present. Never passes the URL through a shell.
static inline void
open_url(const char* url)
{
	if (url == NULL || url[0] == '\0')
		return;

	BString u(url);
	if (u.FindFirst("://") < 0)
		u.Prepend("https://");

	const char* args[] = { u.String() };
	be_roster->Launch("text/html", 1, const_cast<char**>(args));
}


// Center a window over another (typically the main window), clamped
// to stay fully on the same screen. Falls back to screen-centering
// when no parent is given.
static inline void
center_over(BWindow* window, BWindow* parent)
{
	if (window == NULL)
		return;
	if (parent == NULL) {
		window->CenterOnScreen();
		return;
	}

	BRect parentFrame = parent->Frame();
	BRect frame = window->Frame();
	float width = frame.Width();
	float height = frame.Height();

	BPoint topLeft(
		parentFrame.left + (parentFrame.Width() - width) / 2,
		parentFrame.top + (parentFrame.Height() - height) / 2);

	// Keep the window on-screen
	BRect screen(BScreen(parent).Frame());
	if (topLeft.x + width > screen.right)
		topLeft.x = screen.right - width;
	if (topLeft.y + height > screen.bottom)
		topLeft.y = screen.bottom - height;
	if (topLeft.x < screen.left)
		topLeft.x = screen.left;
	if (topLeft.y < screen.top)
		topLeft.y = screen.top;

	window->MoveTo(topLeft);
}


#endif // KURA_UTILS_H
