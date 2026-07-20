/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * KeePass CSV import implementation.
 */


#include "KuraCsvImport.h"

#include <new>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <vector>

#include <File.h>
#include <ObjectList.h>

#include "KuraUtils.h"


// Maximum accepted CSV file size (plain-text CSVs are small; this
// only guards against accidentally picking a huge file).
static const off_t kMaxCsvSize = 50 * 1024 * 1024;


// --- CSV parsing (RFC 4180 style) ---

typedef std::vector<BString> CsvRow;


static void
_ParseCsv(const char* data, size_t length, std::vector<CsvRow>& rows)
{
	CsvRow row;
	BString field;
	bool inQuotes = false;

	// Skip UTF-8 BOM
	size_t i = 0;
	if (length >= 3 && (uint8)data[0] == 0xEF
		&& (uint8)data[1] == 0xBB && (uint8)data[2] == 0xBF) {
		i = 3;
	}

	while (i < length) {
		char c = data[i];

		if (inQuotes) {
			if (c == '"') {
				if (i + 1 < length && data[i + 1] == '"') {
					// Doubled quote inside a quoted field
					field << '"';
					i += 2;
				} else {
					inQuotes = false;
					i++;
				}
			} else {
				field << c;
				i++;
			}
			continue;
		}

		if (c == '"') {
			inQuotes = true;
			i++;
		} else if (c == ',') {
			row.push_back(field);
			field = "";
			i++;
		} else if (c == '\r' || c == '\n') {
			// End of row; treat \r\n as a single break
			if (c == '\r' && i + 1 < length && data[i + 1] == '\n')
				i += 2;
			else
				i++;

			row.push_back(field);
			field = "";

			bool empty = true;
			for (size_t j = 0; j < row.size() && empty; j++) {
				if (row[j].Length() > 0)
					empty = false;
			}
			if (!empty)
				rows.push_back(row);
			row.clear();
		} else {
			field << c;
			i++;
		}
	}

	// Last row without trailing newline
	if (field.Length() > 0 || !row.empty()) {
		row.push_back(field);
		bool empty = true;
		for (size_t j = 0; j < row.size() && empty; j++) {
			if (row[j].Length() > 0)
				empty = false;
		}
		if (!empty)
			rows.push_back(row);
	}
}


// --- Header mapping ---

struct ColumnMap {
	int32 group;
	int32 title;
	int32 username;
	int32 password;
	int32 url;
	int32 notes;
	int32 created;
	int32 modified;

	ColumnMap()
		:
		group(-1), title(-1), username(-1), password(-1),
		url(-1), notes(-1), created(-1), modified(-1)
	{
	}
};


static BString
_NormalizeHeader(const BString& raw)
{
	BString s(raw);
	s.Trim();
	s.ToLower();
	return s;
}


// Try to interpret a row as a header. Returns true and fills the map
// if the row contains a recognizable password column plus a title or
// username column.
static bool
_MapHeader(const CsvRow& row, ColumnMap& map)
{
	for (size_t i = 0; i < row.size(); i++) {
		BString name = _NormalizeHeader(row[i]);
		int32 index = (int32)i;

		if (name == "group" || name == "grouping"
				|| name == "folder") {
			map.group = index;
		} else if (name == "title" || name == "account"
				|| name == "name") {
			map.title = index;
		} else if (name == "username" || name == "user name"
				|| name == "login name" || name == "login"
				|| name == "user") {
			map.username = index;
		} else if (name == "password") {
			map.password = index;
		} else if (name == "url" || name == "web site"
				|| name == "website") {
			map.url = index;
		} else if (name == "notes" || name == "comments"
				|| name == "comment") {
			map.notes = index;
		} else if (name == "created" || name == "creation time") {
			map.created = index;
		} else if (name == "last modified" || name == "modified"
				|| name == "last modification time") {
			map.modified = index;
		}
		// Unknown columns (TOTP, Icon, ...) are ignored
	}

	return map.password >= 0
		&& (map.title >= 0 || map.username >= 0);
}


// --- Timestamp parsing ---

// Days since 1970-01-01 for a civil date (Howard Hinnant's
// days_from_civil algorithm).
static int64
_DaysFromCivil(int y, int m, int d)
{
	y -= m <= 2;
	int era = (y >= 0 ? y : y - 399) / 400;
	unsigned yoe = (unsigned)(y - era * 400);
	unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return (int64)era * 146097 + (int64)doe - 719468;
}


// Parse "YYYY-MM-DDTHH:MM:SS[Z]" or "YYYY-MM-DD HH:MM:SS".
// KeePassXC exports UTC; timestamps are interpreted as UTC.
// Returns 0 when the string can't be parsed.
static time_t
_ParseTimestamp(const BString& s)
{
	if (s.Length() < 19)
		return 0;

	int y, mo, d, h, mi, se;
	char sep;
	if (sscanf(s.String(), "%d-%d-%d%c%d:%d:%d",
			&y, &mo, &d, &sep, &h, &mi, &se) != 7)
		return 0;

	if (y < 1970 || mo < 1 || mo > 12 || d < 1 || d > 31
		|| h < 0 || h > 23 || mi < 0 || mi > 59
		|| se < 0 || se > 60)
		return 0;

	return (time_t)(_DaysFromCivil(y, mo, d) * 86400
		+ h * 3600 + mi * 60 + se);
}


// --- KuraCsvImport ---

KuraCsvImport::KuraCsvImport()
	:
	fImported(0),
	fGroupsCreated(0),
	fSkipped(0)
{
}


kura_id
KuraCsvImport::_FindOrCreateGroup(KuraDatabase* db, const BString& path)
{
	if (path.Length() == 0)
		return kAllGroupId;

	// Walk the path components, creating groups as needed
	kura_id parent = kNoId;
	kura_id result = kAllGroupId;

	int32 start = 0;
	while (start <= path.Length()) {
		int32 slash = path.FindFirst('/', start);
		int32 end = slash < 0 ? path.Length() : slash;

		BString component;
		path.CopyInto(component, start, end - start);
		component.Trim();
		start = end + 1;

		if (component.Length() == 0) {
			if (slash < 0)
				break;
			continue;
		}

		// Look for an existing child with this name
		BObjectList<KuraGroup> children(10);
		db->GetChildGroups(parent, children);

		kura_id found = kNoId;
		for (int32 i = 0; i < children.CountItems(); i++) {
			if (children.ItemAt(i)->name == component) {
				found = children.ItemAt(i)->id;
				break;
			}
		}

		if (found == kNoId) {
			KuraGroup group;
			group.name = component;
			group.parentId = parent;
			found = db->AddGroup(group);
			fGroupsCreated++;
		}

		parent = found;
		result = found;

		if (slash < 0)
			break;
	}

	return result;
}


status_t
KuraCsvImport::Import(const char* path, KuraDatabase* db)
{
	fImported = 0;
	fGroupsCreated = 0;
	fSkipped = 0;
	fError = "";

	if (path == NULL || db == NULL) {
		fError = "Invalid arguments";
		return B_BAD_VALUE;
	}

	BFile file(path, B_READ_ONLY);
	status_t result = file.InitCheck();
	if (result != B_OK) {
		fError = "Failed to open file";
		return result;
	}

	off_t size = 0;
	file.GetSize(&size);
	if (size <= 0) {
		fError = "File is empty";
		return B_BAD_DATA;
	}
	if (size > kMaxCsvSize) {
		fError = "File is too large to be a CSV export";
		return B_BAD_DATA;
	}

	char* data = new(std::nothrow) char[size];
	if (data == NULL) {
		fError = "Out of memory";
		return B_NO_MEMORY;
	}

	if (file.Read(data, size) != (ssize_t)size) {
		fError = "Failed to read file";
		delete[] data;
		return B_IO_ERROR;
	}

	std::vector<CsvRow> rows;
	_ParseCsv(data, (size_t)size, rows);

	// The raw buffer contains plaintext passwords - scrub it now
	memset(data, 0, size);
	delete[] data;

	if (rows.empty()) {
		fError = "No data rows found in the file";
		return B_BAD_DATA;
	}

	// Determine the column layout
	ColumnMap map;
	size_t firstDataRow = 0;

	if (_MapHeader(rows[0], map)) {
		firstDataRow = 1;
	} else {
		// No header: assume KeePass 1.x positional layout
		// ("Account","Login Name","Password","Web Site","Comments")
		map.title = 0;
		map.username = 1;
		map.password = 2;
		map.url = 3;
		map.notes = 4;
	}

	if (rows.size() <= firstDataRow) {
		fError = "The file contains a header but no entries";
		return B_BAD_DATA;
	}

	// Helper lambda-ish accessor
	#define FIELD(row, idx) \
		((idx) >= 0 && (size_t)(idx) < (row).size() \
			? (row)[(idx)] : BString())

	// Decide whether to strip the export's root group component
	// (KeePassXC prefixes every path with the root group's name,
	// e.g. "Root/Internet"). Strip the first component when every
	// non-empty path shares it and it's either literally "Root" or
	// at least one path has more than one component - a lone
	// depth-1 group name shared by all rows could be a real group
	// in a generic CSV, so it's kept in that case.
	BString rootPrefix;
	bool haveDeepPath = false;
	bool samePrefix = true;
	bool anyGroup = false;

	for (size_t i = firstDataRow; i < rows.size() && samePrefix; i++) {
		BString group = FIELD(rows[i], map.group);
		group.Trim();
		if (group.Length() == 0)
			continue;

		anyGroup = true;
		int32 slash = group.FindFirst('/');
		BString first;
		if (slash >= 0) {
			group.CopyInto(first, 0, slash);
			haveDeepPath = true;
		} else
			first = group;

		if (rootPrefix.Length() == 0)
			rootPrefix = first;
		else if (rootPrefix != first)
			samePrefix = false;
	}

	bool stripRoot = anyGroup && samePrefix
		&& (rootPrefix == "Root" || haveDeepPath);

	// Import the rows
	for (size_t i = firstDataRow; i < rows.size(); i++) {
		const CsvRow& row = rows[i];

		KuraEntry entry;
		entry.title = FIELD(row, map.title);
		entry.username = FIELD(row, map.username);
		entry.password = FIELD(row, map.password);
		entry.url = FIELD(row, map.url);
		entry.notes = FIELD(row, map.notes);
		entry.title.Trim();
		entry.url.Trim();

		// Skip rows that carry no usable data at all
		if (entry.title.Length() == 0
			&& entry.username.Length() == 0
			&& entry.password.Length() == 0
			&& entry.url.Length() == 0
			&& entry.notes.Length() == 0) {
			fSkipped++;
			continue;
		}

		if (entry.title.Length() == 0)
			entry.title = "Untitled";

		// Group path
		BString group = FIELD(row, map.group);
		group.Trim();
		if (stripRoot && group.Length() > 0) {
			int32 slash = group.FindFirst('/');
			if (slash < 0)
				group = "";
			else
				group.Remove(0, slash + 1);
		}
		entry.groupId = _FindOrCreateGroup(db, group);

		// Timestamps (kept when parseable; AddEntry falls back to
		// "now" for anything left at 0)
		time_t created = _ParseTimestamp(FIELD(row, map.created));
		time_t modified = _ParseTimestamp(FIELD(row, map.modified));
		entry.createdAt = created;
		entry.modifiedAt = modified != 0 ? modified : created;

		db->AddEntry(entry, true);
		fImported++;

		// Best-effort scrub of the copied password
		scrub_string(entry.password);
	}

	#undef FIELD

	// Best-effort scrub of all parsed fields (they may contain
	// passwords)
	for (size_t i = 0; i < rows.size(); i++) {
		for (size_t j = 0; j < rows[i].size(); j++)
			scrub_string(rows[i][j]);
	}

	if (fImported == 0) {
		fError = "No entries could be imported from the file";
		return B_BAD_DATA;
	}

	return B_OK;
}
