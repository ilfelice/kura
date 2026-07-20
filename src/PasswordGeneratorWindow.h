/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Password generator dialog. Can run standalone (Tools menu, offers
 * "Copy") or attached to an entry edit dialog (offers "Use", which
 * sends kMsgUsePassword with the generated password to the target).
 *
 * Uses OpenSSL RAND_bytes with rejection sampling, so the character
 * selection is uniform (no modulo bias).
 */
#ifndef PASSWORD_GENERATOR_WINDOW_H
#define PASSWORD_GENERATOR_WINDOW_H


#include <Messenger.h>
#include <String.h>
#include <Window.h>

class BButton;
class BCheckBox;
class BSlider;
class BStringView;
class BTextControl;


class PasswordGeneratorWindow : public BWindow {
public:
	// If target is valid, a "Use" button sends the password to it.
								PasswordGeneratorWindow(BRect frame,
									BMessenger target);
	virtual						~PasswordGeneratorWindow();

	virtual void				MessageReceived(BMessage* message);

private:
			void				_Generate();
			void				_UpdateEntropyLabel();
			BString				_BuildPool() const;
			status_t			_RandomIndex(uint32 range,
									uint32& out) const;

			BSlider*			fLengthSlider;
			BCheckBox*			fLowerBox;
			BCheckBox*			fUpperBox;
			BCheckBox*			fDigitsBox;
			BCheckBox*			fSymbolsBox;
			BCheckBox*			fAmbiguousBox;
			BTextControl*		fResultField;
			BStringView*		fEntropyView;
			BButton*			fGenerateButton;
			BButton*			fCopyButton;
			BButton*			fUseButton;

			BMessenger			fTarget;
			bool				fHasTarget;
};


#endif // PASSWORD_GENERATOR_WINDOW_H
