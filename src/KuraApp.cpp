/*
 * KuraApp.cpp
 * Kura - Password Manager for Haiku
 *
 * Application entry point and BApplication implementation.
 */

#include "KuraApp.h"
#include "KuraClipboard.h"
#include "KuraDefs.h"
#include "KuraWindow.h"



KuraApp::KuraApp()
	:
	BApplication(kAppSignature)
{
}


KuraApp::~KuraApp()
{
}


void
KuraApp::ReadyToRun()
{
	KuraWindow* window = new KuraWindow();
	window->Show();
}


void
KuraApp::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgClearClipboard:
			// Timed clipboard clear scheduled by KuraClipboard.
			// Targeted at the app so it fires even if the window
			// that copied the data is gone.
			KuraClipboard::ClearIfOurs();
			break;

		default:
			BApplication::MessageReceived(message);
			break;
	}
}


int
main()
{
	KuraApp app;
	app.Run();
	return 0;
}
