/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * In-memory data model for the password database.
 * Manages groups (folders) and entries (credentials).
 * Serializes to/from JSON for encryption by KuraCrypto.
 *
 * Data hierarchy:
 *   Database
 *     └── Group (id, name, icon, parentId)
 *           └── Entry (id, groupId, title, username, password,
 *                      url, notes, createdAt, modifiedAt)
 */
#ifndef KURA_DATABASE_H
#define KURA_DATABASE_H

#include <Messenger.h>
#include <ObjectList.h>
#include <String.h>

#include <ctime>


// Unique ID type for groups and entries
typedef int32 kura_id;
static const kura_id kNoId = -1;
static const kura_id kAllGroupId = 0;  // Virtual "Root" group


struct KuraEntry {
						KuraEntry();
						KuraEntry(const KuraEntry& other);
						~KuraEntry();

	kura_id				id;
	kura_id				groupId;
	BString				title;
	BString				username;
	BString				password;
	BString				url;
	BString				notes;
	time_t				createdAt;
	time_t				modifiedAt;
};


struct KuraGroup {
						KuraGroup();
						KuraGroup(const KuraGroup& other);
						~KuraGroup();

	kura_id				id;
	kura_id				parentId;	// kNoId for top-level groups
	BString				name;
	BString				icon;		// Emoji or icon identifier
};


// Messages sent by KuraDatabase to notify observers of changes
enum {
	kMsgDatabaseLoaded		= 'dbld',
	kMsgDatabaseSaved		= 'dbsv',
	kMsgDatabaseModified	= 'dbmf',
	kMsgEntryAdded			= 'eadd',
	kMsgEntryUpdated		= 'eupd',
	kMsgEntryRemoved		= 'erm',
	kMsgGroupAdded			= 'gadd',
	kMsgGroupUpdated		= 'gupd',
	kMsgGroupRemoved		= 'grm',
};


class KuraDatabase {
public:
						KuraDatabase();
						~KuraDatabase();

	// --- Serialization ---

	// Serialize the entire database to a JSON string.
	status_t			SerializeToJson(BString& jsonOut) const;

	// Deserialize from a JSON string, replacing current contents.
	status_t			DeserializeFromJson(const BString& json);

	// Initialize with default groups for a new database.
	void				InitDefaults();

	// Clear all data.
	void				Clear();

	// --- Modification tracking ---

	bool				IsModified() const { return fModified; }
	void				SetModified(bool modified) { fModified = modified; }

	// --- Observer for UI updates ---

	void				SetTarget(BMessenger target);

	// --- Group operations ---

	const KuraGroup*	GroupById(kura_id id) const;
	int32				CountGroups() const;
	const KuraGroup*	GroupAt(int32 index) const;

	kura_id				AddGroup(const KuraGroup& group);
	status_t			UpdateGroup(kura_id id, const KuraGroup& group);
	status_t			RemoveGroup(kura_id id);

	// Re-parent a group. Rejects moves that would create a cycle
	// (onto itself or one of its own descendants) and no-ops when
	// the group is already a child of newParentId. newParentId may
	// be kNoId (top level) or kAllGroupId (the virtual root, also
	// top level).
	status_t			MoveGroup(kura_id id, kura_id newParentId);

	// Re-parent AND position a group in one operation: it becomes a
	// child of newParentId, inserted immediately before the sibling
	// beforeId (or appended among that parent's children when
	// beforeId is kNoId). Sibling order is stored as the order of
	// groups in the underlying list, so this both reorders siblings
	// and moves subtrees between levels. Same cycle rejection as
	// MoveGroup().
	status_t			MoveGroupToPosition(kura_id id,
							kura_id newParentId, kura_id beforeId);

	// True if candidate is id itself or a descendant of id.
	bool				IsDescendantOf(kura_id candidate,
							kura_id id) const;

	// Count entries in a group (and its children if recursive).
	int32				CountEntriesInGroup(kura_id groupId,
							bool recursive = true) const;

	// Get child groups of a parent.
	void				GetChildGroups(kura_id parentId,
							BObjectList<KuraGroup>& children) const;

	// --- Entry operations ---

	const KuraEntry*	EntryById(kura_id id) const;
	int32				CountEntries() const;
	const KuraEntry*	EntryAt(int32 index) const;

	// preserveTimestamps keeps entry.createdAt/modifiedAt (used by
	// importers); otherwise both are set to "now".
	kura_id				AddEntry(const KuraEntry& entry,
							bool preserveTimestamps = false);
	status_t			UpdateEntry(kura_id id, const KuraEntry& entry);
	status_t			RemoveEntry(kura_id id);

	// Get entries filtered by group (kAllGroupId for all).
	void				GetEntriesInGroup(kura_id groupId,
							BObjectList<KuraEntry>& entries,
							bool recursive = true) const;

	// Search entries by text across title, username, url, notes.
	void				SearchEntries(const char* query,
							BObjectList<KuraEntry>& results) const;

private:
	kura_id				_NextId();
	void				_NotifyChange(uint32 what, kura_id id = kNoId);
	bool				_GroupHasAncestor(kura_id groupId,
							kura_id ancestorId) const;

	// Simple JSON helpers (no external dependency)
	BString				_EscapeJson(const BString& str) const;
	BString				_UnescapeJson(const BString& str) const;

	BObjectList<KuraGroup, true>	fGroups;
	BObjectList<KuraEntry, true>	fEntries;
	kura_id				fNextId;
	bool				fModified;
	BMessenger			fTarget;
	bool				fHasTarget;
};

#endif // KURA_DATABASE_H
