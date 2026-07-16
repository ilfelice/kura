/*
 * KuraClipboard.cpp
 * Kura - Password Manager for Haiku
 * Distributed under the terms of the MIT License.
 * Copyright 2026 Il Felice.
 *
 * Clipboard handling with verified timed clear.
 */

#include "KuraClipboard.h"
#include "KuraDefs.h"
#include "KuraUtils.h"

#include <Application.h>
#include <Clipboard.h>
#include <MessageRunner.h>

#include <cstring>


BString KuraClipboard::sLastCopied;
bigtime_t KuraClipboard::sClearDelay
	= (bigtime_t)kDefaultClipboardClearSeconds * 1000000;
BMessageRunner* KuraClipboard::sRunner = NULL;
BLocker KuraClipboard::sLock("kura clipboard");


void
KuraClipboard::CopyWithTimedClear(const char* text)
{
	if (text == NULL)
		text = "";

	if (!be_clipboard->Lock())
		return;

	be_clipboard->Clear();
	BMessage* clip = be_clipboard->Data();
	if (clip != NULL) {
		clip->AddData("text/plain", B_MIME_TYPE, text, strlen(text));
		be_clipboard->Commit();
	}
	be_clipboard->Unlock();

	if (!sLock.Lock())
		return;

	// Remember what we copied so the clear can verify it later
	scrub_string(sLastCopied);
	sLastCopied = text;

	// (Re)schedule the timed clear. Deleting a fired runner is safe.
	delete sRunner;
	sRunner = NULL;
	if (sClearDelay > 0) {
		BMessage clearMsg(kMsgClearClipboard);
		sRunner = new BMessageRunner(BMessenger(be_app), &clearMsg,
			sClearDelay, 1);
	}

	sLock.Unlock();
}


void
KuraClipboard::SetClearDelay(bigtime_t delay)
{
	if (!sLock.Lock())
		return;
	sClearDelay = delay;
	sLock.Unlock();
}


void
KuraClipboard::ClearIfOurs()
{
	if (!sLock.Lock())
		return;

	if (sLastCopied.Length() == 0) {
		sLock.Unlock();
		return;
	}

	if (be_clipboard->Lock()) {
		bool isOurs = false;
		BMessage* clip = be_clipboard->Data();
		if (clip != NULL) {
			const char* data = NULL;
			ssize_t size = 0;
			if (clip->FindData("text/plain", B_MIME_TYPE,
					(const void**)&data, &size) == B_OK
				&& data != NULL
				&& size == (ssize_t)sLastCopied.Length()
				&& memcmp(data, sLastCopied.String(), size) == 0) {
				isOurs = true;
			}
		}

		if (isOurs) {
			be_clipboard->Clear();
			be_clipboard->Commit();
		}
		be_clipboard->Unlock();
	}

	scrub_string(sLastCopied);
	sLastCopied = "";

	delete sRunner;
	sRunner = NULL;

	sLock.Unlock();
}
