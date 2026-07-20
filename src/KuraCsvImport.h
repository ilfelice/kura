/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Imports entries from KeePass-style CSV files.
 *
 * Supported formats:
 *   - KeePassXC CSV export: header row with columns like
 *     "Group","Title","Username","Password","URL","Notes",
 *     "TOTP","Icon","Created","Last Modified".
 *     Columns are matched by name (case-insensitive), so column
 *     order and extra columns don't matter.
 *   - KeePass 1.x CSV export: no header, positional columns
 *     "Account","Login Name","Password","Web Site","Comments".
 *
 * The parser handles quoted fields with embedded commas, newlines
 * and doubled quotes (RFC 4180 style), CRLF line endings, and a
 * UTF-8 BOM.
 *
 * Group paths ("Root/Internet/Work") are recreated as nested
 * groups. The export's root component is stripped when every path
 * shares it. KeePassXC "Created"/"Last Modified" timestamps are
 * preserved when present.
 */
#ifndef KURA_CSV_IMPORT_H
#define KURA_CSV_IMPORT_H


#include <String.h>
#include <SupportDefs.h>

#include "KuraDatabase.h"


class KuraCsvImport {
public:
						KuraCsvImport();

	// Import a CSV file into the database. Returns B_OK if at
	// least the file could be parsed (individual bad rows are
	// skipped and counted).
	status_t			Import(const char* path, KuraDatabase* db);

	int32				ImportedEntries() const { return fImported; }
	int32				CreatedGroups() const { return fGroupsCreated; }
	int32				SkippedRows() const { return fSkipped; }

	const char*			ErrorString() const { return fError.String(); }

private:
	kura_id				_FindOrCreateGroup(KuraDatabase* db,
							const BString& path);

	int32				fImported;
	int32				fGroupsCreated;
	int32				fSkipped;
	BString				fError;
};


#endif // KURA_CSV_IMPORT_H
