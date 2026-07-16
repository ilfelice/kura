/*
 * KuraApp.h
 * Kura - Password Manager for Haiku
 *
 * Main application class.
 */
#ifndef KURA_APP_H
#define KURA_APP_H

#include <Application.h>


class KuraApp : public BApplication {
public:
						KuraApp();
	virtual				~KuraApp();

	virtual void		ReadyToRun();
	virtual void		MessageReceived(BMessage* message);
};

#endif // KURA_APP_H
