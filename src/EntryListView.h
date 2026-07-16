/*
 * EntryListView.h
 * Kura - Password Manager for Haiku
 * Distributed under the terms of the MIT License.
 * Copyright 2026 Il Felice.
 *
 * Middle pane showing entries in a ColumnListView.
 * Displays title, username, and modified date columns.
 * Supports sorting by clicking column headers.
 */
#ifndef ENTRY_LIST_VIEW_H
#define ENTRY_LIST_VIEW_H

#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <View.h>

class SearchTextControl;

#include "KuraDatabase.h"


// Row that holds a reference to a KuraEntry
class EntryRow : public BRow {
public:
						EntryRow(const KuraEntry* entry);

	kura_id				EntryId() const { return fEntryId; }

	void				UpdateFromEntry(const KuraEntry* entry);

private:
	kura_id				fEntryId;
};


class EntryListView : public BView {
public:
						EntryListView();
	virtual				~EntryListView();

	virtual void		AttachedToWindow();
	virtual void		MessageReceived(BMessage* message);

	void				SetDatabase(KuraDatabase* db);

	// Show entries for a specific group (kAllGroupId = all)
	void				ShowGroup(kura_id groupId);

	// Filter entries by search text (empty = no filter)
	void				SetSearchFilter(const char* query);

	// Get the currently selected entry ID
	kura_id				SelectedEntryId() const;

	// Select an entry by ID
	void				SelectEntry(kura_id entryId);

	// Refresh the list (e.g., after add/edit/delete)
	void				Refresh();

	// Move keyboard focus to the search field
	void				FocusSearch();
	void				SetSearchEnabled(bool enabled);

	BColumnListView*	ListView() const { return fListView; }

private:
	void				_PopulateList();
	BString				_FormatTime(time_t t) const;

	BColumnListView*	fListView;
	SearchTextControl*	fSearchField;
	KuraDatabase*		fDatabase;
	kura_id				fCurrentGroupId;
	BString				fSearchQuery;
};


#endif // ENTRY_LIST_VIEW_H
