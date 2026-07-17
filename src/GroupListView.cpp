/*
 * GroupListView.cpp
 * Kura - Password Manager for Haiku
 * Distributed under the terms of the MIT License.
 * Copyright 2026 Il Felice.
 *
 * Sidebar group tree implementation. Item drawing modeled on
 * TimeZoneListItem from the Haiku Time preferences applet.
 */

#include "GroupListView.h"
#include "KuraDefs.h"

#include <Application.h>
#include <Bitmap.h>
#include <ControlLook.h>
#include <IconUtils.h>
#include <LayoutBuilder.h>
#include <Mime.h>
#include <Resources.h>
#include <ScrollView.h>

#include <cmath>
#include <new>


// --- Icon data loading (cached raw HVIF buffers) ---

// Load a named vector icon from the application's resources.
// The returned buffer is owned by the app's BResources object and
// stays valid for the lifetime of the app.
static const uint8*
_LoadNamedIcon(const char* name, size_t& size)
{
	size = 0;
	if (name == NULL || name[0] == '\0' || be_app == NULL)
		return NULL;

	BResources* resources = be_app->AppResources();
	if (resources == NULL)
		return NULL;

	size_t resourceSize = 0;
	const void* data = resources->LoadResource(B_VECTOR_ICON_TYPE,
		name, &resourceSize);
	if (data == NULL || resourceSize == 0)
		return NULL;

	size = resourceSize;
	return (const uint8*)data;
}


// The system's generic folder icon, fetched once from the MIME
// database. Owned by this function (leaked deliberately at exit).
static const uint8*
_FolderIconData(size_t& size)
{
	static uint8* sData = NULL;
	static size_t sSize = 0;
	static bool sTried = false;

	if (!sTried) {
		sTried = true;
		BMimeType dirType("application/x-vnd.Be-directory");
		uint8* data = NULL;
		size_t dataSize = 0;
		if (dirType.GetIcon(&data, &dataSize) == B_OK && data != NULL) {
			sData = data;
			sSize = dataSize;
		}
	}

	size = sSize;
	return sData;
}


// Resolve the icon data for an item: a custom per-group resource
// name wins, then the built-in "folder" resource for groups (or
// "icon-root" for the root item), then the app's own icon for the
// root, then the system folder icon.
static const uint8*
_IconDataFor(const char* customName, bool isRoot, size_t& size)
{
	const uint8* data = _LoadNamedIcon(customName, size);
	if (data != NULL)
		return data;

	if (isRoot) {
		data = _LoadNamedIcon("icon-root", size);
		if (data != NULL)
			return data;
		data = _LoadNamedIcon("BEOS:ICON", size);
		if (data != NULL)
			return data;
	} else {
		data = _LoadNamedIcon("folder", size);
		if (data != NULL)
			return data;
	}

	return _FolderIconData(size);
}


// --- GroupItem ---

GroupItem::GroupItem(const char* label, kura_id groupId, const char* icon,
	uint32 outlineLevel, bool isRoot)
	:
	BStringItem(label, outlineLevel),
	fGroupId(groupId),
	fIcon(icon),
	fIsRoot(isRoot),
	fBitmap(NULL)
{
}


GroupItem::~GroupItem()
{
	delete fBitmap;
}


void
GroupItem::DrawItem(BView* owner, BRect frame, bool complete)
{
	// The frame we receive from BOutlineListView is already indented
	// past the latch area for this item's outline level.
	rgb_color highColor = owner->HighColor();
	rgb_color lowColor = owner->LowColor();

	rgb_color bgColor;
	if (IsSelected())
		bgColor = ui_color(B_LIST_SELECTED_BACKGROUND_COLOR);
	else
		bgColor = owner->ViewColor();

	if (IsSelected() || complete) {
		// Fill the entire row, including the latch/indent area to
		// the left of the item frame
		BRect fillRect(frame);
		fillRect.left = owner->Bounds().left;
		fillRect.right = owner->Bounds().right;
		owner->SetHighColor(bgColor);
		owner->SetLowColor(bgColor);
		owner->FillRect(fillRect);
	} else
		owner->SetLowColor(owner->ViewColor());

	float spacing = be_control_look->DefaultLabelSpacing();
	float textOffset = 0;

	// Column model: the frame starts at this item's icon column;
	// content is inset by half the label spacing, which centers an
	// icon of (rowheight - 3) in a column of (iconsize + spacing).
	// The same pad applies to every level including the root.
	float leading = floorf(spacing / 2);

	// BRect dimensions are inclusive: a frame from top to bottom
	// covers Height() + 1 pixels.
	float itemHeight = frame.Height() + 1;

	// Icon, vertically centered in the item frame
	if (fBitmap != NULL && fBitmap->IsValid()) {
		float iconSize = fBitmap->Bounds().Width() + 1;
		float iconTop = frame.top
			+ floorf((itemHeight - iconSize) / 2 + 0.5f);
		BRect iconFrame(frame.left + leading, iconTop,
			frame.left + leading + iconSize - 1,
			iconTop + iconSize - 1);

		owner->SetDrawingMode(B_OP_ALPHA);
		owner->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		owner->DrawBitmap(fBitmap, iconFrame);
		owner->SetDrawingMode(B_OP_COPY);

		textOffset = iconSize + spacing;
	}

	// Name, baseline placed so the glyph block (ascent + descent) is
	// vertically centered as well. Exact float metrics, rounded once
	// at the end - rounding ascent/descent individually shifts the
	// baseline by a pixel or two. (BStringItem's BaselineOffset()
	// can't be used: it assumes the height BStringItem computed, and
	// Update() makes the item taller.)
	rgb_color textColor;
	if (IsSelected())
		textColor = ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR);
	else
		textColor = ui_color(B_LIST_ITEM_TEXT_COLOR);

	BFont font;
	owner->GetFont(&font);
	font_height fontHeight;
	font.GetHeight(&fontHeight);
	float baseline = frame.top + floorf((itemHeight
			- (fontHeight.ascent + fontHeight.descent)) / 2
		+ fontHeight.ascent + 0.5f);

	owner->SetHighColor(textColor);
	owner->MovePenTo(frame.left + leading + textOffset, baseline);
	owner->DrawString(Text());

	owner->SetHighColor(highColor);
	owner->SetLowColor(lowColor);
}


void
GroupItem::Update(BView* owner, const BFont* font)
{
	BStringItem::Update(owner, font);

	// A little breathing room; also determines the icon size
	SetHeight(Height() + 4);
	_UpdateIcon();
	if (fBitmap != NULL) {
		SetWidth(Width() + fBitmap->Bounds().Width() + 1
			+ be_control_look->DefaultLabelSpacing());
	}
}


void
GroupItem::_UpdateIcon()
{
	delete fBitmap;
	fBitmap = NULL;

	float iconSize = Height() - 3;
	if (iconSize < 8)
		return;

	size_t dataSize = 0;
	const uint8* data = _IconDataFor(fIcon.String(), fIsRoot, dataSize);
	if (data == NULL)
		return;

	fBitmap = new(std::nothrow) BBitmap(
		BRect(0, 0, iconSize - 1, iconSize - 1), B_RGBA32);
	if (fBitmap == NULL)
		return;

	if (BIconUtils::GetVectorIcon(data, dataSize, fBitmap) != B_OK) {
		delete fBitmap;
		fBitmap = NULL;
	}
}


// --- Sidebar tree geometry: the column model ---
//
// The tree is laid out on a grid of equal columns. An item at
// outline level L has its latch (expand arrow) centered in column
// L, its icon in column L+1 and its text starting in column L+2.
// The root (level 0) has no latch: its icon is in column 1 and its
// text in column 2 - so the root icon and the top-level latches
// share column 1, center-aligned.
//
// The column width derives entirely from font-scaled quantities:
// the icon size (which follows the row height) plus the control
// look's label spacing. Content is centered in its column, which
// gives every column a natural half-spacing pad on each side.
// Nothing here is a tunable constant.

static float
_ColumnWidth(const BRect& itemRect)
{
	// Icon pixel size matches GroupItem::_UpdateIcon()
	float iconSize = itemRect.Height() - 3;
	return floorf(iconSize + be_control_look->DefaultLabelSpacing());
}


// --- GroupOutlineView ---

// The virtual root item (level 0) is a fixed anchor like the
// database root in KeePassXC: it draws no latch and cannot be
// collapsed. Expand()/Collapse(), latch clicks and the keyboard
// shortcuts all funnel through ExpandOrCollapse(), so blocking it
// there covers every path.
class GroupOutlineView : public BOutlineListView {
public:
	GroupOutlineView(const char* name)
		:
		BOutlineListView(name, B_SINGLE_SELECTION_LIST),
		fDropTargetIndex(-1)
	{
	}

	// --- Entry drag & drop target ---

	virtual void MouseMoved(BPoint where, uint32 code,
		const BMessage* dragMessage)
	{
		if (dragMessage != NULL
			&& dragMessage->what == kMsgEntryDrag) {
			int32 index = code == B_EXITED_VIEW
				? -1 : IndexOf(where);
			_SetDropTarget(index);
		} else if (fDropTargetIndex >= 0)
			_SetDropTarget(-1);

		BOutlineListView::MouseMoved(where, code, dragMessage);
	}

	virtual void MessageReceived(BMessage* message)
	{
		if (message->what == kMsgEntryDrag
			&& message->WasDropped()) {
			BPoint where = ConvertFromScreen(message->DropPoint());
			int32 index = IndexOf(where);
			_SetDropTarget(-1);

			GroupItem* item = index < 0 ? NULL
				: dynamic_cast<GroupItem*>(ItemAt(index));
			int32 entryId;
			if (item != NULL
				&& message->FindInt32("entryId", &entryId) == B_OK) {
				BMessage dropped(kMsgEntryDropped);
				dropped.AddInt32("entryId", entryId);
				dropped.AddInt32("groupId", item->GroupId());
				Window()->PostMessage(&dropped);
			}
			return;
		}

		BOutlineListView::MessageReceived(message);
	}

	virtual void Draw(BRect updateRect)
	{
		BOutlineListView::Draw(updateRect);

		// Drop target feedback: frame the hovered group
		if (fDropTargetIndex >= 0) {
			BRect frame = ItemFrame(fDropTargetIndex);
			SetHighColor(ui_color(B_KEYBOARD_NAVIGATION_COLOR));
			StrokeRect(frame);
			SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
		}
	}

	virtual void ExpandOrCollapse(BListItem* item, bool expand)
	{
		if (!expand && item != NULL && item->OutlineLevel() == 0)
			return;
		BOutlineListView::ExpandOrCollapse(item, expand);
	}

	// LatchRect() positions the latch arrow and defines its click
	// area: the arrow box (stock size, one font size square) is
	// centered both ways in column L. The root (level 0) has no
	// latch column; a degenerate rect disables its hit area.
	virtual BRect LatchRect(BRect itemRect, int32 level) const
	{
		if (level == 0) {
			return BRect(itemRect.left, itemRect.top,
				itemRect.left, itemRect.top);
		}

		float width = _ColumnWidth(itemRect);
		float latchSize = be_plain_font->Size();
		float columnLeft = itemRect.left + (level - 1) * width;
		float x = columnLeft
			+ floorf((width - latchSize) / 2 + 0.5f);
		float y = itemRect.top + floorf((itemRect.Height() + 1
			- latchSize) / 2 + 0.5f);

		return BRect(0, 0, latchSize, latchSize)
			.OffsetBySelf(x, y);
	}

	virtual void DrawLatch(BRect itemRect, int32 level,
		bool collapsed, bool highlighted, bool misTracked)
	{
		if (level == 0)
			return;
		BOutlineListView::DrawLatch(itemRect, level, collapsed,
			highlighted, misTracked);
	}

	// The stock implementation draws the latch first and the item
	// second; a full-row selection background painted by the item
	// would erase the latch. Draw the item first, then the latch on
	// top (DrawLatch() only strokes the arrow, no background fill).
	virtual void DrawItem(BListItem* item, BRect itemRect,
		bool complete = false)
	{
		// Column model: a level-L item's icon column is L+1, whose
		// left edge is L column-widths in
		BRect frame(itemRect);
		frame.left += item->OutlineLevel() * _ColumnWidth(itemRect);
		item->DrawItem(this, frame, complete);

		if (CountItemsUnder(item, true) > 0) {
			DrawLatch(itemRect, (int32)item->OutlineLevel(),
				!item->IsExpanded(),
				item->IsSelected() || complete, false);
		}
	}

private:
	void _SetDropTarget(int32 index)
	{
		if (index == fDropTargetIndex)
			return;
		if (fDropTargetIndex >= 0)
			Invalidate(ItemFrame(fDropTargetIndex));
		fDropTargetIndex = index;
		if (fDropTargetIndex >= 0)
			Invalidate(ItemFrame(fDropTargetIndex));
	}

	int32 fDropTargetIndex;
};


// --- GroupListView ---

GroupListView::GroupListView()
	:
	BView("groupListView", B_WILL_DRAW),
	fListView(NULL),
	fScrollView(NULL),
	fDatabase(NULL),
	fRootLabel("Root")
{
	fListView = new GroupOutlineView("groupList");
	fListView->SetSelectionMessage(new BMessage(kMsgGroupSelected));
	// Double-click (or Enter) on a group opens the edit dialog.
	// _EditGroup() ignores the virtual root and the locked state.
	fListView->SetInvocationMessage(new BMessage(kMsgEditGroup));

	// No frame of its own: the toolbar border above, the status
	// bar line below, the splitter to the right and the window edge
	// to the left provide all the framing. A border here would also
	// inset the scroll bar, leaving a gap under the toolbar border.
	fScrollView = new BScrollView("groupScroll", fListView, 0,
		false, true, B_NO_BORDER);

	fListView->SetExplicitMinSize(BSize(0, 0));
	fScrollView->SetExplicitMinSize(BSize(0, 0));

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fScrollView)
	.End();

	SetExplicitMinSize(BSize(0, 0));
}


GroupListView::~GroupListView()
{
	_DeleteAllItems();
}


void
GroupListView::SetDatabase(KuraDatabase* db)
{
	fDatabase = db;
	if (db == NULL)
		fRootLabel = "Root";
	Refresh();
}


void
GroupListView::SetRootLabel(const char* label)
{
	if (label != NULL && label[0] != '\0')
		fRootLabel = label;
	else
		fRootLabel = "Root";
}


void
GroupListView::_DeleteAllItems()
{
	// BOutlineListView::MakeEmpty() does not free the items
	BList old;
	for (int32 i = 0; i < fListView->FullListCountItems(); i++)
		old.AddItem(fListView->FullListItemAt(i));

	fListView->MakeEmpty();

	for (int32 i = 0; i < old.CountItems(); i++)
		delete (BListItem*)old.ItemAt(i);
}


void
GroupListView::Refresh()
{
	// Remember current selection and which groups are collapsed
	// (everything is expanded by default)
	kura_id selectedId = SelectedGroupId();

	BList collapsedIds;
	for (int32 i = 0; i < fListView->FullListCountItems(); i++) {
		GroupItem* item = dynamic_cast<GroupItem*>(
			fListView->FullListItemAt(i));
		if (item != NULL && !item->IsExpanded())
			collapsedIds.AddItem((void*)(addr_t)item->GroupId());
	}

	_DeleteAllItems();

	if (fDatabase == NULL)
		return;

	// Virtual "Root" item (labelled with the database name); it is
	// a fixed anchor and always expanded
	GroupItem* rootItem = new GroupItem(fRootLabel.String(),
		kAllGroupId, "", 0, true);
	fListView->AddItem(rootItem);

	// Real groups, depth-first with explicit outline levels; this
	// keeps siblings in database order (AddUnder would reverse them)
	_AddGroupItems(kNoId, 1, collapsedIds);

	// Restore selection
	SelectGroup(selectedId);

	// If nothing selected, select "Root"
	if (fListView->CurrentSelection() < 0)
		fListView->Select(0);
}


kura_id
GroupListView::SelectedGroupId() const
{
	int32 selected = fListView->CurrentSelection();
	if (selected < 0)
		return kAllGroupId;

	GroupItem* item = dynamic_cast<GroupItem*>(fListView->ItemAt(selected));
	if (item == NULL)
		return kAllGroupId;

	return item->GroupId();
}


void
GroupListView::SelectGroup(kura_id groupId)
{
	for (int32 i = 0; i < fListView->FullListCountItems(); i++) {
		GroupItem* item = dynamic_cast<GroupItem*>(
			fListView->FullListItemAt(i));
		if (item == NULL || item->GroupId() != groupId)
			continue;

		// If the item is hidden under collapsed ancestors, expand
		// them so the selection is visible
		if (fListView->IndexOf(item) < 0) {
			BListItem* super = fListView->Superitem(item);
			while (super != NULL) {
				fListView->Expand(super);
				super = fListView->Superitem(super);
			}
		}

		int32 index = fListView->IndexOf(item);
		if (index >= 0) {
			fListView->Select(index);
			fListView->ScrollToSelection();
		}
		return;
	}
}


void
GroupListView::_AddGroupItems(kura_id parentId, uint32 level,
	const BList& collapsedIds)
{
	BObjectList<KuraGroup> children(10);
	fDatabase->GetChildGroups(parentId, children);

	for (int32 i = 0; i < children.CountItems(); i++) {
		const KuraGroup* group = children.ItemAt(i);

		GroupItem* item = new GroupItem(group->name.String(), group->id,
			group->icon.String(), level, false);
		if (collapsedIds.HasItem((void*)(addr_t)group->id))
			item->SetExpanded(false);

		fListView->AddItem(item);

		// Children immediately follow their parent, one level deeper
		_AddGroupItems(group->id, level + 1, collapsedIds);
	}
}
