/*
 * GroupListView.h
 * Kura - Password Manager for Haiku
 * Distributed under the terms of the MIT License.
 * Copyright 2026 Il Felice.
 *
 * Sidebar view showing the group/folder tree, KeePass-style:
 * a BOutlineListView with native expand/collapse latches, vector
 * icons and per-level indentation. The item drawing follows the
 * pattern of TimeZoneListItem from the Time preferences applet.
 *
 * The first item is always the virtual "Root" group (labelled with
 * the database name when set), which shows every entry.
 */
#ifndef GROUP_LIST_VIEW_H
#define GROUP_LIST_VIEW_H

#include <List.h>
#include <OutlineListView.h>
#include <StringItem.h>
#include <View.h>

#include "KuraDatabase.h"

class BBitmap;
class BScrollView;


// List item holding a group ID; draws icon + name inside the frame
// provided by BOutlineListView (which is already indented past the
// latch area).
class GroupItem : public BStringItem {
public:
						GroupItem(const char* label, kura_id groupId,
							const char* icon, uint32 outlineLevel,
							bool isRoot);
	virtual				~GroupItem();

	virtual void		DrawItem(BView* owner, BRect frame,
							bool complete = false);
	virtual void		Update(BView* owner, const BFont* font);

	kura_id				GroupId() const { return fGroupId; }

private:
	void				_UpdateIcon();

	kura_id				fGroupId;
	BString				fIcon;		// named icon resource, may be ""
	bool				fIsRoot;
	BBitmap*			fBitmap;
};


class GroupListView : public BView {
public:
						GroupListView();
	virtual				~GroupListView();

	// Populate from database
	void				SetDatabase(KuraDatabase* db);
	void				Refresh();

	// Label of the virtual root item (usually the database name)
	void				SetRootLabel(const char* label);

	// Get the currently selected group ID
	kura_id				SelectedGroupId() const;

	// Select a group by ID
	void				SelectGroup(kura_id groupId);

	BOutlineListView*	ListView() const { return fListView; }

private:
	void				_DeleteAllItems();
	void				_AddGroupItems(kura_id parentId, uint32 level,
							const BList& collapsedIds);

	BOutlineListView*	fListView;
	BScrollView*		fScrollView;
	KuraDatabase*		fDatabase;
	BString				fRootLabel;
};


#endif // GROUP_LIST_VIEW_H
