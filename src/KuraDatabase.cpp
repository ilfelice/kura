/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * In-memory database with JSON serialization.
 * Uses a simple hand-rolled JSON parser/writer to avoid external dependencies.
 */

#include "KuraDatabase.h"

#include <Message.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>


// --- KuraEntry ---

KuraEntry::KuraEntry()
	:
	id(kNoId),
	groupId(kAllGroupId),
	title(""),
	username(""),
	password(""),
	url(""),
	notes(""),
	createdAt(time(NULL)),
	modifiedAt(time(NULL))
{
}


KuraEntry::KuraEntry(const KuraEntry& other)
	:
	id(other.id),
	groupId(other.groupId),
	title(other.title),
	username(other.username),
	password(other.password),
	url(other.url),
	notes(other.notes),
	createdAt(other.createdAt),
	modifiedAt(other.modifiedAt)
{
}


KuraEntry::~KuraEntry()
{
	// Scrub sensitive fields from memory
	memset(password.LockBuffer(password.Length()), 0, password.Length());
	password.UnlockBuffer(0);
}


// --- KuraGroup ---

KuraGroup::KuraGroup()
	:
	id(kNoId),
	parentId(kNoId),
	name(""),
	icon("")
{
}


KuraGroup::KuraGroup(const KuraGroup& other)
	:
	id(other.id),
	parentId(other.parentId),
	name(other.name),
	icon(other.icon)
{
}


KuraGroup::~KuraGroup()
{
}


// --- KuraDatabase ---

KuraDatabase::KuraDatabase()
	:
	fGroups(10),
	fEntries(20),
	fNextId(100),
	fModified(false),
	fHasTarget(false)
{
}


KuraDatabase::~KuraDatabase()
{
	Clear();
}


void
KuraDatabase::Clear()
{
	fGroups.MakeEmpty();
	fEntries.MakeEmpty();
	fNextId = 100;
	fModified = false;
}


void
KuraDatabase::InitDefaults()
{
	Clear();

	// New databases start with just the virtual "Root" group
	// (KeePass-style). Users create their own group structure.

	fModified = false;
}


void
KuraDatabase::SetTarget(BMessenger target)
{
	fTarget = target;
	fHasTarget = true;
}


// --- Group operations ---

const KuraGroup*
KuraDatabase::GroupById(kura_id id) const
{
	for (int32 i = 0; i < fGroups.CountItems(); i++) {
		if (fGroups.ItemAt(i)->id == id)
			return fGroups.ItemAt(i);
	}
	return NULL;
}


int32
KuraDatabase::CountGroups() const
{
	return fGroups.CountItems();
}


const KuraGroup*
KuraDatabase::GroupAt(int32 index) const
{
	return fGroups.ItemAt(index);
}


kura_id
KuraDatabase::AddGroup(const KuraGroup& group)
{
	KuraGroup* newGroup = new KuraGroup(group);
	newGroup->id = _NextId();
	fGroups.AddItem(newGroup);
	fModified = true;
	_NotifyChange(kMsgGroupAdded, newGroup->id);
	return newGroup->id;
}


status_t
KuraDatabase::UpdateGroup(kura_id id, const KuraGroup& group)
{
	for (int32 i = 0; i < fGroups.CountItems(); i++) {
		KuraGroup* existing = fGroups.ItemAt(i);
		if (existing->id == id) {
			existing->name = group.name;
			existing->icon = group.icon;
			existing->parentId = group.parentId;
			fModified = true;
			_NotifyChange(kMsgGroupUpdated, id);
			return B_OK;
		}
	}
	return B_ENTRY_NOT_FOUND;
}


bool
KuraDatabase::IsDescendantOf(kura_id candidate, kura_id id) const
{
	// Walk up from candidate to the root; if we pass through id,
	// candidate is id or one of its descendants.
	kura_id current = candidate;
	while (current != kNoId && current != kAllGroupId) {
		if (current == id)
			return true;
		const KuraGroup* group = GroupById(current);
		if (group == NULL)
			break;
		current = group->parentId;
	}
	return false;
}


status_t
KuraDatabase::MoveGroup(kura_id id, kura_id newParentId)
{
	const KuraGroup* group = GroupById(id);
	if (group == NULL)
		return B_ENTRY_NOT_FOUND;

	// Normalize the virtual root to "top level" (kNoId)
	if (newParentId == kAllGroupId)
		newParentId = kNoId;

	// No-op if already parented there
	if (group->parentId == newParentId)
		return B_OK;

	// Reject cycles: the new parent must not be the group itself
	// or any of its descendants
	if (newParentId != kNoId && IsDescendantOf(newParentId, id))
		return B_NOT_ALLOWED;

	// New parent must exist (unless moving to top level)
	if (newParentId != kNoId && GroupById(newParentId) == NULL)
		return B_ENTRY_NOT_FOUND;

	KuraGroup updated(*group);
	updated.parentId = newParentId;
	return UpdateGroup(id, updated);
}


status_t
KuraDatabase::MoveGroupToPosition(kura_id id, kura_id newParentId,
	kura_id beforeId)
{
	const KuraGroup* group = GroupById(id);
	if (group == NULL)
		return B_ENTRY_NOT_FOUND;

	// Normalize the virtual root to "top level" (kNoId)
	if (newParentId == kAllGroupId)
		newParentId = kNoId;

	// Reject cycles: the new parent must not be the group itself
	// or any of its descendants
	if (newParentId != kNoId && IsDescendantOf(newParentId, id))
		return B_NOT_ALLOWED;

	// New parent must exist (unless moving to top level)
	if (newParentId != kNoId && GroupById(newParentId) == NULL)
		return B_ENTRY_NOT_FOUND;

	// Positioning a group immediately before itself is a no-op,
	// not a request to append at the end.
	if (beforeId == id)
		return B_OK;

	int32 fromIndex = fGroups.IndexOf(const_cast<KuraGroup*>(group));
	if (fromIndex < 0)
		return B_ERROR;

	// Apply the re-parent
	fGroups.ItemAt(fromIndex)->parentId = newParentId;

	// Determine the target array index. Sibling order is the order
	// groups appear in fGroups, so we position this group's list
	// slot immediately before beforeId's slot (or at the end when
	// beforeId is kNoId / not found).
	int32 toIndex;
	if (beforeId != kNoId) {
		const KuraGroup* beforeGroup = GroupById(beforeId);
		int32 beforeIndex = beforeGroup != NULL
			? fGroups.IndexOf(const_cast<KuraGroup*>(beforeGroup))
			: -1;
		if (beforeIndex < 0) {
			toIndex = fGroups.CountItems() - 1;
		} else if (fromIndex < beforeIndex) {
			// Removing the group first shifts beforeId left by one,
			// so the slot just before it is beforeIndex - 1.
			toIndex = beforeIndex - 1;
		} else {
			toIndex = beforeIndex;
		}
	} else {
		toIndex = fGroups.CountItems() - 1;
	}

	if (toIndex != fromIndex && toIndex >= 0)
		fGroups.MoveItem(fromIndex, toIndex);

	fModified = true;
	_NotifyChange(kMsgGroupUpdated, id);
	return B_OK;
}


status_t
KuraDatabase::RemoveGroup(kura_id id)
{
	// First, move all entries in this group to no-group
	for (int32 i = 0; i < fEntries.CountItems(); i++) {
		KuraEntry* entry = fEntries.ItemAt(i);
		if (entry->groupId == id)
			entry->groupId = kAllGroupId;
	}

	// Re-parent child groups
	for (int32 i = 0; i < fGroups.CountItems(); i++) {
		KuraGroup* group = fGroups.ItemAt(i);
		if (group->parentId == id) {
			const KuraGroup* removedGroup = GroupById(id);
			group->parentId = removedGroup ? removedGroup->parentId : kNoId;
		}
	}

	// Remove the group
	for (int32 i = 0; i < fGroups.CountItems(); i++) {
		if (fGroups.ItemAt(i)->id == id) {
			fGroups.RemoveItemAt(i);
			fModified = true;
			_NotifyChange(kMsgGroupRemoved, id);
			return B_OK;
		}
	}
	return B_ENTRY_NOT_FOUND;
}


int32
KuraDatabase::CountEntriesInGroup(kura_id groupId, bool recursive) const
{
	if (groupId == kAllGroupId)
		return fEntries.CountItems();

	int32 count = 0;
	for (int32 i = 0; i < fEntries.CountItems(); i++) {
		const KuraEntry* entry = fEntries.ItemAt(i);
		if (entry->groupId == groupId)
			count++;
		else if (recursive && _GroupHasAncestor(entry->groupId, groupId))
			count++;
	}
	return count;
}


void
KuraDatabase::GetChildGroups(kura_id parentId,
	BObjectList<KuraGroup>& children) const
{
	for (int32 i = 0; i < fGroups.CountItems(); i++) {
		const KuraGroup* group = fGroups.ItemAt(i);
		if (group->parentId == parentId)
			children.AddItem(const_cast<KuraGroup*>(group));
	}
}


// --- Entry operations ---

const KuraEntry*
KuraDatabase::EntryById(kura_id id) const
{
	for (int32 i = 0; i < fEntries.CountItems(); i++) {
		if (fEntries.ItemAt(i)->id == id)
			return fEntries.ItemAt(i);
	}
	return NULL;
}


int32
KuraDatabase::CountEntries() const
{
	return fEntries.CountItems();
}


const KuraEntry*
KuraDatabase::EntryAt(int32 index) const
{
	return fEntries.ItemAt(index);
}


kura_id
KuraDatabase::AddEntry(const KuraEntry& entry, bool preserveTimestamps)
{
	KuraEntry* newEntry = new KuraEntry(entry);
	newEntry->id = _NextId();
	if (!preserveTimestamps || newEntry->createdAt == 0)
		newEntry->createdAt = time(NULL);
	if (!preserveTimestamps || newEntry->modifiedAt == 0)
		newEntry->modifiedAt = time(NULL);
	fEntries.AddItem(newEntry);
	fModified = true;
	_NotifyChange(kMsgEntryAdded, newEntry->id);
	return newEntry->id;
}


status_t
KuraDatabase::UpdateEntry(kura_id id, const KuraEntry& entry)
{
	for (int32 i = 0; i < fEntries.CountItems(); i++) {
		KuraEntry* existing = fEntries.ItemAt(i);
		if (existing->id == id) {
			existing->groupId = entry.groupId;
			existing->title = entry.title;
			existing->username = entry.username;
			existing->password = entry.password;
			existing->url = entry.url;
			existing->notes = entry.notes;
			existing->modifiedAt = time(NULL);
			fModified = true;
			_NotifyChange(kMsgEntryUpdated, id);
			return B_OK;
		}
	}
	return B_ENTRY_NOT_FOUND;
}


status_t
KuraDatabase::RemoveEntry(kura_id id)
{
	for (int32 i = 0; i < fEntries.CountItems(); i++) {
		if (fEntries.ItemAt(i)->id == id) {
			fEntries.RemoveItemAt(i);
			fModified = true;
			_NotifyChange(kMsgEntryRemoved, id);
			return B_OK;
		}
	}
	return B_ENTRY_NOT_FOUND;
}


void
KuraDatabase::GetEntriesInGroup(kura_id groupId,
	BObjectList<KuraEntry>& entries, bool recursive) const
{
	for (int32 i = 0; i < fEntries.CountItems(); i++) {
		const KuraEntry* entry = fEntries.ItemAt(i);
		if (groupId == kAllGroupId
			|| entry->groupId == groupId
			|| (recursive && _GroupHasAncestor(entry->groupId, groupId))) {
			entries.AddItem(const_cast<KuraEntry*>(entry));
		}
	}
}


void
KuraDatabase::SearchEntries(const char* query,
	BObjectList<KuraEntry>& results) const
{
	if (query == NULL || query[0] == '\0')
		return;

	BString lowerQuery(query);
	lowerQuery.ToLower();

	for (int32 i = 0; i < fEntries.CountItems(); i++) {
		const KuraEntry* entry = fEntries.ItemAt(i);

		BString title(entry->title);
		title.ToLower();
		BString username(entry->username);
		username.ToLower();
		BString url(entry->url);
		url.ToLower();
		BString notes(entry->notes);
		notes.ToLower();

		if (title.FindFirst(lowerQuery) >= 0
			|| username.FindFirst(lowerQuery) >= 0
			|| url.FindFirst(lowerQuery) >= 0
			|| notes.FindFirst(lowerQuery) >= 0) {
			results.AddItem(const_cast<KuraEntry*>(entry));
		}
	}
}


// --- Private helpers ---

kura_id
KuraDatabase::_NextId()
{
	return fNextId++;
}


void
KuraDatabase::_NotifyChange(uint32 what, kura_id id)
{
	if (!fHasTarget)
		return;

	BMessage msg(what);
	if (id != kNoId)
		msg.AddInt32("id", id);
	fTarget.SendMessage(&msg);
}


bool
KuraDatabase::_GroupHasAncestor(kura_id groupId, kura_id ancestorId) const
{
	// Walk up the parent chain, with depth limit to prevent loops
	kura_id current = groupId;
	for (int depth = 0; depth < 20; depth++) {
		const KuraGroup* group = GroupById(current);
		if (group == NULL)
			return false;
		if (group->parentId == ancestorId)
			return true;
		if (group->parentId == kNoId)
			return false;
		current = group->parentId;
	}
	return false;
}


// --- JSON serialization ---

BString
KuraDatabase::_EscapeJson(const BString& str) const
{
	BString result;
	for (int32 i = 0; i < str.Length(); i++) {
		char c = str[i];
		switch (c) {
			case '"':	result << "\\\""; break;
			case '\\':	result << "\\\\"; break;
			case '\n':	result << "\\n"; break;
			case '\r':	result << "\\r"; break;
			case '\t':	result << "\\t"; break;
			default:	result << c; break;
		}
	}
	return result;
}


BString
KuraDatabase::_UnescapeJson(const BString& str) const
{
	BString result;
	for (int32 i = 0; i < str.Length(); i++) {
		if (str[i] == '\\' && i + 1 < str.Length()) {
			i++;
			switch (str[i]) {
				case '"':	result << '"'; break;
				case '\\':	result << '\\'; break;
				case 'n':	result << '\n'; break;
				case 'r':	result << '\r'; break;
				case 't':	result << '\t'; break;
				default:	result << '\\' << str[i]; break;
			}
		} else {
			result << str[i];
		}
	}
	return result;
}


// Helper to find a JSON string value by key in a JSON object string.
// This is a minimal parser for our known-good output format.
static BString
_FindJsonString(const BString& json, const char* key)
{
	BString search;
	search << "\"" << key << "\":\"";
	int32 pos = json.FindFirst(search);
	if (pos < 0)
		return "";

	pos += search.Length();
	BString value;
	for (int32 i = pos; i < json.Length(); i++) {
		if (json[i] == '\\' && i + 1 < json.Length()) {
			value << json[i] << json[i + 1];
			i++;
		} else if (json[i] == '"') {
			break;
		} else {
			value << json[i];
		}
	}
	return value;
}


// Helper to find a JSON integer value by key.
static int32
_FindJsonInt(const BString& json, const char* key)
{
	BString search;
	search << "\"" << key << "\":";
	int32 pos = json.FindFirst(search);
	if (pos < 0)
		return 0;

	pos += search.Length();
	BString numStr;
	for (int32 i = pos; i < json.Length(); i++) {
		char c = json[i];
		if ((c >= '0' && c <= '9') || c == '-')
			numStr << c;
		else
			break;
	}
	return atoi(numStr.String());
}


// Helper to find a JSON int64 value by key.
static int64
_FindJsonInt64(const BString& json, const char* key)
{
	BString search;
	search << "\"" << key << "\":";
	int32 pos = json.FindFirst(search);
	if (pos < 0)
		return 0;

	pos += search.Length();
	BString numStr;
	for (int32 i = pos; i < json.Length(); i++) {
		char c = json[i];
		if ((c >= '0' && c <= '9') || c == '-')
			numStr << c;
		else
			break;
	}
	return strtoll(numStr.String(), NULL, 10);
}


// Split a JSON array of objects into individual object strings.
// Braces inside string values (passwords and notes may contain any
// characters) must not affect the depth count, so the scanner tracks
// whether it is inside a string.
static void
_SplitJsonArray(const BString& arrayStr, BObjectList<BString, true>& items)
{
	int depth = 0;
	int32 start = -1;
	bool inString = false;

	for (int32 i = 0; i < arrayStr.Length(); i++) {
		char c = arrayStr[i];

		if (inString) {
			if (c == '\\')
				i++; // skip escaped character
			else if (c == '"')
				inString = false;
			continue;
		}

		if (c == '"') {
			inString = true;
		} else if (c == '{') {
			if (depth == 0)
				start = i;
			depth++;
		} else if (c == '}') {
			depth--;
			if (depth == 0 && start >= 0) {
				BString* item = new BString();
				arrayStr.CopyInto(*item, start, i - start + 1);
				items.AddItem(item);
				start = -1;
			}
		}
	}
}


// Find a JSON array value by key, returns the array content including brackets.
static BString
_FindJsonArray(const BString& json, const char* key)
{
	BString search;
	search << "\"" << key << "\":[";
	int32 pos = json.FindFirst(search);
	if (pos < 0)
		return "[]";

	pos += search.Length() - 1; // point at '['
	int depth = 0;
	int32 start = pos;
	bool inString = false;

	// Brackets inside string values must not affect the depth count.
	for (int32 i = pos; i < json.Length(); i++) {
		char c = json[i];

		if (inString) {
			if (c == '\\')
				i++; // skip escaped character
			else if (c == '"')
				inString = false;
			continue;
		}

		if (c == '"') {
			inString = true;
		} else if (c == '[') {
			depth++;
		} else if (c == ']') {
			depth--;
			if (depth == 0) {
				BString result;
				json.CopyInto(result, start, i - start + 1);
				return result;
			}
		}
	}
	return "[]";
}


status_t
KuraDatabase::SerializeToJson(BString& jsonOut) const
{
	jsonOut = "{";

	// Next ID
	jsonOut << "\"nextId\":" << fNextId << ",";

	// Groups array
	jsonOut << "\"groups\":[";
	for (int32 i = 0; i < fGroups.CountItems(); i++) {
		const KuraGroup* group = fGroups.ItemAt(i);
		if (i > 0)
			jsonOut << ",";
		jsonOut << "{";
		jsonOut << "\"id\":" << group->id << ",";
		jsonOut << "\"parentId\":" << group->parentId << ",";
		jsonOut << "\"name\":\"" << _EscapeJson(group->name) << "\",";
		jsonOut << "\"icon\":\"" << _EscapeJson(group->icon) << "\"";
		jsonOut << "}";
	}
	jsonOut << "],";

	// Entries array
	jsonOut << "\"entries\":[";
	for (int32 i = 0; i < fEntries.CountItems(); i++) {
		const KuraEntry* entry = fEntries.ItemAt(i);
		if (i > 0)
			jsonOut << ",";
		jsonOut << "{";
		jsonOut << "\"id\":" << entry->id << ",";
		jsonOut << "\"groupId\":" << entry->groupId << ",";
		jsonOut << "\"title\":\"" << _EscapeJson(entry->title) << "\",";
		jsonOut << "\"username\":\"" << _EscapeJson(entry->username) << "\",";
		jsonOut << "\"password\":\"" << _EscapeJson(entry->password) << "\",";
		jsonOut << "\"url\":\"" << _EscapeJson(entry->url) << "\",";
		jsonOut << "\"notes\":\"" << _EscapeJson(entry->notes) << "\",";
		jsonOut << "\"createdAt\":" << (int64)entry->createdAt << ",";
		jsonOut << "\"modifiedAt\":" << (int64)entry->modifiedAt;
		jsonOut << "}";
	}
	jsonOut << "]";

	jsonOut << "}";
	return B_OK;
}


status_t
KuraDatabase::DeserializeFromJson(const BString& json)
{
	Clear();

	fNextId = _FindJsonInt(json, "nextId");
	if (fNextId < 100)
		fNextId = 100;

	// Parse groups
	BString groupsArray = _FindJsonArray(json, "groups");
	BObjectList<BString, true> groupItems(10);
	_SplitJsonArray(groupsArray, groupItems);

	for (int32 i = 0; i < groupItems.CountItems(); i++) {
		const BString& item = *groupItems.ItemAt(i);
		KuraGroup* group = new KuraGroup();
		group->id = _FindJsonInt(item, "id");
		group->parentId = _FindJsonInt(item, "parentId");
		group->name = _UnescapeJson(_FindJsonString(item, "name"));
		group->icon = _UnescapeJson(_FindJsonString(item, "icon"));
		fGroups.AddItem(group);
	}

	// Parse entries
	BString entriesArray = _FindJsonArray(json, "entries");
	BObjectList<BString, true> entryItems(20);
	_SplitJsonArray(entriesArray, entryItems);

	for (int32 i = 0; i < entryItems.CountItems(); i++) {
		const BString& item = *entryItems.ItemAt(i);
		KuraEntry* entry = new KuraEntry();
		entry->id = _FindJsonInt(item, "id");
		entry->groupId = _FindJsonInt(item, "groupId");
		entry->title = _UnescapeJson(_FindJsonString(item, "title"));
		entry->username = _UnescapeJson(_FindJsonString(item, "username"));
		entry->password = _UnescapeJson(_FindJsonString(item, "password"));
		entry->url = _UnescapeJson(_FindJsonString(item, "url"));
		entry->notes = _UnescapeJson(_FindJsonString(item, "notes"));
		entry->createdAt = (time_t)_FindJsonInt64(item, "createdAt");
		entry->modifiedAt = (time_t)_FindJsonInt64(item, "modifiedAt");
		fEntries.AddItem(entry);
	}

	fModified = false;
	_NotifyChange(kMsgDatabaseLoaded);
	return B_OK;
}
