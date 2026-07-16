/*
 * EntryListView.cpp
 * Kura - Password Manager for Haiku
 * Distributed under the terms of the MIT License.
 * Copyright 2026 Il Felice.
 *
 * ColumnListView-based entry list implementation.
 */

#include "EntryListView.h"
#include "SearchTextControl.h"
#include "KuraDefs.h"

#include <Bitmap.h>
#include <LayoutBuilder.h>
#include <TextControl.h>

#include <cstdio>
#include <ctime>


// Column indices
enum {
	kTitleColumn = 0,
	kUsernameColumn,
	kModifiedColumn,
};


// --- EntryRow ---

EntryRow::EntryRow(const KuraEntry* entry)
	:
	BRow(),
	fEntryId(entry->id)
{
	UpdateFromEntry(entry);
}


void
EntryRow::UpdateFromEntry(const KuraEntry* entry)
{
	// Title
	SetField(new BStringField(entry->title.String()), kTitleColumn);
	// Username
	SetField(new BStringField(entry->username.String()), kUsernameColumn);
	// Modified date
	char timeStr[32];
	struct tm* tm = localtime(&entry->modifiedAt);
	if (tm != NULL)
		strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", tm);
	else
		snprintf(timeStr, sizeof(timeStr), "—");
	SetField(new BStringField(timeStr), kModifiedColumn);
}


// --- EntryListView ---

// BColumnListView that starts a drag when a row is pulled. The
// drag message carries the entry ID; GroupListView accepts the
// drop and the window performs the move.
class EntryColumnListView : public BColumnListView {
public:
	EntryColumnListView()
		:
		BColumnListView("entryList", 0, B_PLAIN_BORDER)
	{
	}

	virtual bool InitiateDrag(BPoint where, bool wasSelected)
	{
		EntryRow* row = dynamic_cast<EntryRow*>(RowAt(where));
		if (row == NULL)
			return false;

		BMessage drag(kMsgEntryDrag);
		drag.AddInt32("entryId", row->EntryId());

		DragMessage(&drag, _MakeDragBitmap(row), B_OP_ALPHA,
			BPoint(-8, -8));
		return true;
	}

private:
	// A small translucent tag showing the entry title; clearer than
	// the stock dashed outline. Ownership passes to DragMessage().
	BBitmap* _MakeDragBitmap(EntryRow* row)
	{
		BString title;
		BStringField* field
			= dynamic_cast<BStringField*>(row->GetField(0));
		if (field != NULL)
			title = field->String();
		if (title.Length() == 0)
			title = "(entry)";

		BFont font(be_plain_font);
		font_height fontHeight;
		font.GetHeight(&fontHeight);
		float height = ceilf(fontHeight.ascent)
			+ ceilf(fontHeight.descent) + 8;
		float width = font.StringWidth(title.String()) + 16;

		BBitmap* bitmap = new(std::nothrow) BBitmap(
			BRect(0, 0, width - 1, height - 1), B_RGBA32, true);
		if (bitmap == NULL)
			return NULL;

		BView* canvas = new BView(bitmap->Bounds(), "dragCanvas",
			B_FOLLOW_NONE, 0);
		bitmap->AddChild(canvas);
		bitmap->Lock();

		rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
		rgb_color fill = base;
		fill.alpha = 200;
		canvas->SetDrawingMode(B_OP_COPY);
		canvas->SetHighColor(fill);
		canvas->FillRect(canvas->Bounds());
		canvas->SetHighColor(tint_color(base, B_DARKEN_2_TINT));
		canvas->StrokeRect(canvas->Bounds());
		canvas->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
		canvas->SetLowColor(fill);
		canvas->MovePenTo(8, 4 + ceilf(fontHeight.ascent));
		canvas->DrawString(title.String());
		canvas->Sync();
		bitmap->Unlock();

		return bitmap;
	}
};


EntryListView::EntryListView()
	:
	BView("entryListView", B_WILL_DRAW),
	fListView(NULL),
	fDatabase(NULL),
	fCurrentGroupId(kAllGroupId),
	fSearchQuery("")
{
	fListView = new EntryColumnListView();

	// Add columns
	fListView->AddColumn(new BStringColumn("Title", 180, 80, 400,
		B_TRUNCATE_MIDDLE), kTitleColumn);
	fListView->AddColumn(new BStringColumn("Username", 150, 60, 300,
		B_TRUNCATE_END), kUsernameColumn);
	fListView->AddColumn(new BStringColumn("Modified", 130, 80, 200,
		B_TRUNCATE_END), kModifiedColumn);

	fListView->SetSortColumn(fListView->ColumnAt(kTitleColumn), true, true);

	fListView->SetSelectionMessage(new BMessage(kMsgEntrySelected));
	fListView->SetInvocationMessage(new BMessage(kMsgEditEntry));

	fListView->SetExplicitMinSize(BSize(0, 0));

	// Search field above the list, with an inline clear "x"
	fSearchField = new SearchTextControl("search", "Search:",
		new BMessage(kMsgSearchChanged));

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_HALF_ITEM_SPACING)
		.AddGroup(B_HORIZONTAL)
			.SetInsets(B_USE_HALF_ITEM_SPACING,
				B_USE_HALF_ITEM_SPACING, B_USE_HALF_ITEM_SPACING, 0)
			.Add(fSearchField)
		.End()
		.Add(fListView)
	.End();
}


void
EntryListView::AttachedToWindow()
{
	BView::AttachedToWindow();
	fSearchField->SetTarget(this);
}


void
EntryListView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSearchChanged:
			SetSearchFilter(fSearchField->Text());
			break;

		default:
			BView::MessageReceived(message);
			break;
	}
}


EntryListView::~EntryListView()
{
}


void
EntryListView::SetDatabase(KuraDatabase* db)
{
	fDatabase = db;
	fSearchQuery = "";
	fSearchField->SetText("");
	fCurrentGroupId = kAllGroupId;
	Refresh();
}


void
EntryListView::ShowGroup(kura_id groupId)
{
	bool searchActive = fSearchQuery.Length() > 0;
	if (fCurrentGroupId == groupId && !searchActive)
		return;

	fCurrentGroupId = groupId;

	// Selecting a group cancels an active search
	if (searchActive) {
		fSearchQuery = "";
		fSearchField->SetText("");
	}

	_PopulateList();
}


void
EntryListView::SetSearchFilter(const char* query)
{
	if (fSearchQuery == query)
		return;

	fSearchQuery = query ? query : "";
	_PopulateList();
}


kura_id
EntryListView::SelectedEntryId() const
{
	BRow* row = fListView->CurrentSelection();
	if (row == NULL)
		return kNoId;

	EntryRow* entryRow = dynamic_cast<EntryRow*>(row);
	if (entryRow == NULL)
		return kNoId;

	return entryRow->EntryId();
}


void
EntryListView::SelectEntry(kura_id entryId)
{
	for (int32 i = 0; i < fListView->CountRows(); i++) {
		EntryRow* row = dynamic_cast<EntryRow*>(fListView->RowAt(i));
		if (row != NULL && row->EntryId() == entryId) {
			fListView->AddToSelection(row);
			fListView->ScrollTo(row);
			return;
		}
	}
}


void
EntryListView::SetSearchEnabled(bool enabled)
{
	fSearchField->SetEnabled(enabled);
}


void
EntryListView::FocusSearch()
{
	fSearchField->MakeFocus(true);
}


void
EntryListView::Refresh()
{
	kura_id selectedId = SelectedEntryId();
	_PopulateList();
	if (selectedId != kNoId)
		SelectEntry(selectedId);
}


void
EntryListView::_PopulateList()
{
	fListView->Clear();

	if (fDatabase == NULL)
		return;

	BObjectList<KuraEntry> entries(20);

	if (fSearchQuery.Length() > 0) {
		// Search mode: search all entries regardless of group
		fDatabase->SearchEntries(fSearchQuery.String(), entries);
	} else {
		// Group mode: show entries in selected group
		fDatabase->GetEntriesInGroup(fCurrentGroupId, entries);
	}

	for (int32 i = 0; i < entries.CountItems(); i++) {
		const KuraEntry* entry = entries.ItemAt(i);
		EntryRow* row = new EntryRow(entry);
		fListView->AddRow(row);
	}
}


BString
EntryListView::_FormatTime(time_t t) const
{
	char buf[32];
	struct tm* tm = localtime(&t);
	if (tm != NULL)
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm);
	else
		snprintf(buf, sizeof(buf), "—");
	return BString(buf);
}
