/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Centralized clipboard handling with timed auto-clear.
 *
 * CopyWithTimedClear() copies text to the system clipboard and
 * schedules a kMsgClearClipboard message to be_app after the
 * configured delay (SetClearDelay(); 0 disables the timed clear).
 * KuraApp forwards that to ClearIfOurs(),
 * which only clears the clipboard if it still contains exactly
 * what Kura put there - so we never destroy something the user
 * copied elsewhere in the meantime.
 */
#ifndef KURA_CLIPBOARD_H
#define KURA_CLIPBOARD_H

#include <Locker.h>
#include <String.h>

class BMessageRunner;


class KuraClipboard {
public:
	// Copy text to the clipboard and schedule a timed clear.
	static void			CopyWithTimedClear(const char* text);

	// Clear the clipboard, but only if it still holds our text.
	// Also called when the database locks.
	static void			ClearIfOurs();

	// Delay before the timed clear; 0 disables it (the clipboard is
	// then only cleared on lock/quit)
	static void			SetClearDelay(bigtime_t delay);

private:
	static BString		sLastCopied;
	static bigtime_t	sClearDelay;
	static BMessageRunner*	sRunner;
	static BLocker		sLock;
};

#endif // KURA_CLIPBOARD_H
