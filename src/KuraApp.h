/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
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
