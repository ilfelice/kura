/*
 * KuraDefs.h
 * Kura - Password Manager for Haiku
 * Distributed under the terms of the MIT License.
 * Copyright 2026 Il Felice.
 *
 * Application-wide constants and message definitions.
 */
#ifndef KURA_DEFS_H
#define KURA_DEFS_H


// Application signature
static const char* kAppSignature = "application/x-vnd.Kura";

static const char* kSettingsDir = "Kura";

// Clipboard auto-clear delay (microseconds)
static const int32 kDefaultClipboardClearSeconds = 10;

// Auto-lock timeout (microseconds)
static const int32 kDefaultAutoLockMinutes = 5;

// UI message constants
enum {
	// Application
	kMsgNewDatabase			= 'andb',
	kMsgOpenDatabase		= 'aodb',
	kMsgSaveDatabase		= 'asdb',
	kMsgSaveDatabaseAs		= 'asda',
	kMsgLockDatabase		= 'aldb',
	kMsgChangePassword		= 'acpw',
	kMsgQuit				= 'aqut',

	// Unlock
	kMsgUnlock				= 'ulck',
	kMsgUnlockCancel		= 'ulcc',

	// Entries
	kMsgNewEntry			= 'enew',
	kMsgEditEntry			= 'eedt',
	kMsgDeleteEntry			= 'edel',
	kMsgDuplicateEntry		= 'edup',
	kMsgCopyUsername		= 'ecpu',
	kMsgCopyPassword		= 'ecpp',
	kMsgOpenUrl				= 'eurl',

	// Entry edit dialog
	kMsgEntrySave			= 'esav',
	kMsgEntryCancel			= 'ecan',

	// Groups
	kMsgNewGroup			= 'gnew',
	kMsgEditGroup			= 'gedt',
	kMsgDeleteGroup			= 'gdel',

	// Group edit dialog
	kMsgGroupSave			= 'gsav',
	kMsgGroupCancel			= 'gcan',

	// Group selection changed in sidebar
	kMsgGroupSelected		= 'gsel',

	// Entry selection changed in list
	kMsgEntrySelected		= 'esel',

	// Search
	kMsgSearchChanged		= 'srch',

	// Tools
	kMsgPasswordGenerator	= 'tpwg',

	// Settings window
	kMsgShowSettings		= 'sett',
	kMsgSettingsChanged		= 'stch',
	kMsgSettingsClosed		= 'stcl',
	kMsgActivateSettings	= 'stac',

	// Close (lock without the unlock prompt)
	kMsgCloseDatabase		= 'cldb',

	// Import
	kMsgImportCsv			= 'icsv',

	// Recent files ("path" string in the message)
	kMsgOpenRecent			= 'orec',
	kMsgClearRecent			= 'crec',

	// File panel results
	kMsgOpenDbRef			= 'odbr',
	kMsgNewDbRef			= 'ndbr',
	kMsgSaveAsRef			= 'sdbr',
	kMsgImportCsvRef		= 'icsr',

	// Focus the search field (Cmd+F)
	kMsgFocusSearch			= 'fsrc',

	// Entry drag & drop ("entryId" int32; target adds "groupId")
	kMsgEntryDrag			= 'edrg',
	kMsgEntryDropped		= 'edrp',

	// Password generator -> edit window ("password" string)
	kMsgUsePassword			= 'upwd',

	// Auto-lock timer
	kMsgAutoLockTick		= 'altk',
	kMsgResetAutoLock		= 'alrs',

	// Clipboard clear timer
	kMsgClearClipboard		= 'clcb',
};


#endif // KURA_DEFS_H
