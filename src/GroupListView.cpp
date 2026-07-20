/*
 * Copyright 2026 Il Felice. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Sidebar group tree implementation. Item drawing modeled on
 * TimeZoneListItem from the Haiku Time preferences applet.
 */


#include "GroupListView.h"

#include <math.h>
#include <new>

#include <Application.h>
#include <Bitmap.h>
#include <ControlLook.h>
#include <IconUtils.h>
#include <LayoutBuilder.h>
#include <Mime.h>
#include <Resources.h>
#include <ScrollView.h>

#include "KuraDefs.h"


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
	// Where a group drag would land relative to the hovered row:
	// as a child of it (onto), or as a sibling before/after it.
	enum drop_zone {
		DROP_NONE,
		DROP_ONTO,
		DROP_BEFORE,
		DROP_AFTER,
	};

	GroupOutlineView(const char* name)
		:
		BOutlineListView(name, B_SINGLE_SELECTION_LIST),
		fDropTargetIndex(-1),
		fDropZone(DROP_NONE),
		fDatabase(NULL)
	{
	}

	void SetDatabase(KuraDatabase* db) { fDatabase = db; }

	// --- Starting a group drag ---

	virtual bool InitiateDrag(BPoint where, int32 index,
		bool wasSelected)
	{
		GroupItem* item = index < 0 ? NULL
			: dynamic_cast<GroupItem*>(ItemAt(index));
		// The virtual root is a fixed anchor and can't be moved.
		if (item == NULL || item->IsRoot())
			return false;

		BMessage drag(kMsgGroupDrag);
		drag.AddInt32("groupId", item->GroupId());

		DragMessage(&drag, _MakeGroupDragBitmap(item),
			B_OP_ALPHA, BPoint(-8, -8));
		return true;
	}

	// --- Drag & drop target (entries and groups) ---

	virtual void MouseMoved(BPoint where, uint32 code,
		const BMessage* dragMessage)
	{
		int32 index = -1;
		drop_zone zone = DROP_NONE;
		if (dragMessage != NULL && code != B_EXITED_VIEW) {
			if (dragMessage->what == kMsgEntryDrag) {
				index = IndexOf(where);
				if (index >= 0)
					zone = DROP_ONTO;
			} else if (dragMessage->what == kMsgGroupDrag) {
				int32 hit = IndexOf(where);
				zone = _ZoneFor(hit, where);
				if (zone != DROP_NONE
					&& _ValidGroupDrop(dragMessage, hit, zone))
					index = hit;
				else
					zone = DROP_NONE;
			}
		}
		_SetDropTarget(index, zone);

		BOutlineListView::MouseMoved(where, code, dragMessage);
	}

	virtual void MessageReceived(BMessage* message)
	{
		if ((message->what == kMsgEntryDrag
				|| message->what == kMsgGroupDrag)
			&& message->WasDropped()) {
			BPoint where = ConvertFromScreen(message->DropPoint());
			int32 index = IndexOf(where);
			_SetDropTarget(-1, DROP_NONE);

			GroupItem* item = index < 0 ? NULL
				: dynamic_cast<GroupItem*>(ItemAt(index));
			if (item == NULL) {
				return;
			}

			if (message->what == kMsgEntryDrag) {
				int32 entryId;
				if (message->FindInt32("entryId", &entryId) == B_OK) {
					BMessage dropped(kMsgEntryDropped);
					dropped.AddInt32("entryId", entryId);
					dropped.AddInt32("groupId", item->GroupId());
					Window()->PostMessage(&dropped);
				}
			} else {	// kMsgGroupDrag
				drop_zone zone = _ZoneFor(index, where);
				int32 groupId;
				if (zone != DROP_NONE
					&& _ValidGroupDrop(message, index, zone)
					&& message->FindInt32("groupId", &groupId)
						== B_OK) {
					kura_id newParentId;
					kura_id beforeId;
					_ResolveDrop(index, zone, &newParentId, &beforeId);

					BMessage dropped(kMsgGroupReparented);
					dropped.AddInt32("groupId", groupId);
					dropped.AddInt32("newParentId", newParentId);
					dropped.AddInt32("beforeId", beforeId);
					Window()->PostMessage(&dropped);
				}
			}
			return;
		}

		BOutlineListView::MessageReceived(message);
	}

	virtual void Draw(BRect updateRect)
	{
		BOutlineListView::Draw(updateRect);

		if (fDropTargetIndex < 0 || fDropZone == DROP_NONE)
			return;

		BRect frame = ItemFrame(fDropTargetIndex);
		SetHighColor(ui_color(B_KEYBOARD_NAVIGATION_COLOR));

		if (fDropZone == DROP_ONTO) {
			// Frame the row: drop becomes a child
			StrokeRect(frame);
		} else {
			// Insertion line between rows: drop reorders siblings.
			// Indent the line to the hovered row's content so it
			// reads as "at this level". A 1px hairline is the
			// conventional insertion cue.
			float indent = _InsertionIndent(fDropTargetIndex);
			float y = fDropZone == DROP_BEFORE
				? frame.top : frame.bottom;
			y = floorf(y) + 0.5f;	// land on a pixel center
			StrokeLine(BPoint(frame.left + indent, y),
				BPoint(frame.right, y));
		}

		SetHighColor(ui_color(B_LIST_ITEM_TEXT_COLOR));
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
	// True if dropping the dragged group onto the item at index
	// would be a valid re-parent (not onto itself, not onto a
	// descendant, not onto its current parent, target exists).
	// Which third of the row is the pointer in? Top -> insert
	// before, bottom -> insert after, middle -> drop onto. The
	// virtual root only accepts "onto" (you can't sit above or
	// below the single root row as a sibling).
	drop_zone _ZoneFor(int32 index, BPoint where)
	{
		if (index < 0)
			return DROP_NONE;
		GroupItem* item = dynamic_cast<GroupItem*>(ItemAt(index));
		if (item == NULL)
			return DROP_NONE;
		if (item->IsRoot())
			return DROP_ONTO;

		BRect frame = ItemFrame(index);
		float third = frame.Height() / 3.0f;
		if (where.y < frame.top + third)
			return DROP_BEFORE;
		if (where.y > frame.bottom - third)
			return DROP_AFTER;
		return DROP_ONTO;
	}

	// Translate (row, zone) into the parameters MoveGroupToPosition
	// expects: the new parent and the sibling to insert before
	// (kNoId = append at end of that parent's children).
	void _ResolveDrop(int32 index, drop_zone zone,
		kura_id* newParentId, kura_id* beforeId)
	{
		GroupItem* target = dynamic_cast<GroupItem*>(ItemAt(index));
		if (target == NULL) {
			*newParentId = kNoId;
			*beforeId = kNoId;
			return;
		}

		if (zone == DROP_ONTO) {
			*newParentId = target->IsRoot()
				? kNoId : target->GroupId();
			*beforeId = kNoId;	// append among the new children
			return;
		}

		// Sibling insert: same parent as the target row
		const KuraGroup* targetGroup
			= fDatabase->GroupById(target->GroupId());
		kura_id parent = targetGroup != NULL
			? targetGroup->parentId : kNoId;
		*newParentId = parent;

		if (zone == DROP_BEFORE) {
			*beforeId = target->GroupId();
		} else {	// DROP_AFTER: before the next sibling, or append
			*beforeId = _NextSiblingId(target->GroupId(), parent);
		}
	}

	// The id of the sibling that follows groupId among parent's
	// children in display order, or kNoId if it's the last.
	kura_id _NextSiblingId(kura_id groupId, kura_id parent)
	{
		BObjectList<KuraGroup> siblings;
		fDatabase->GetChildGroups(parent, siblings);
		for (int32 i = 0; i < siblings.CountItems(); i++) {
			if (siblings.ItemAt(i)->id == groupId) {
				if (i + 1 < siblings.CountItems())
					return siblings.ItemAt(i + 1)->id;
				return kNoId;
			}
		}
		return kNoId;
	}

	// Left indent for an insertion line: align to the row's content
	// (icon column), so the line reads as "at this group's level".
	float _InsertionIndent(int32 index)
	{
		GroupItem* item = dynamic_cast<GroupItem*>(ItemAt(index));
		if (item == NULL)
			return 0;
		BRect frame = ItemFrame(index);
		return item->OutlineLevel() * _ColumnWidth(frame);
	}

	// Zone-aware validity: resolves the intended parent for the
	// zone, then rejects self/descendant cycles and no-ops.
	bool _ValidGroupDrop(const BMessage* drag, int32 index,
		drop_zone zone)
	{
		if (fDatabase == NULL || index < 0 || zone == DROP_NONE)
			return false;
		GroupItem* target = dynamic_cast<GroupItem*>(ItemAt(index));
		if (target == NULL)
			return false;

		int32 groupId;
		if (drag->FindInt32("groupId", &groupId) != B_OK)
			return false;
		kura_id dragged = groupId;

		// Can't drop a group onto/around itself
		if (target->GroupId() == dragged)
			return false;

		kura_id newParent;
		kura_id beforeId;
		_ResolveDrop(index, zone, &newParent, &beforeId);

		const KuraGroup* group = fDatabase->GroupById(dragged);
		if (group == NULL)
			return false;

		// Onto itself or a descendant -> would create a cycle
		if (newParent != kNoId
			&& (newParent == dragged
				|| fDatabase->IsDescendantOf(newParent, dragged)))
			return false;

		// Inserting relative to one of the dragged group's own
		// descendants is also a cycle
		if (fDatabase->IsDescendantOf(target->GroupId(), dragged))
			return false;

		// No-op detection for "onto": already a child there
		if (zone == DROP_ONTO && group->parentId == newParent)
			return false;

		// No-op detection for sibling inserts: dropping into either
		// separator adjacent to the dragged group leaves it exactly
		// where it is. That happens when the resolved parent is the
		// group's current parent and the insertion point (beforeId)
		// is the group itself or the sibling that already follows
		// it.
		if (zone != DROP_ONTO && newParent == group->parentId) {
			kura_id currentNext
				= _NextSiblingId(dragged, group->parentId);
			if (beforeId == dragged || beforeId == currentNext)
				return false;
		}

		return true;
	}

	// A translucent tag showing the group name, like the entry drag.
	BBitmap* _MakeGroupDragBitmap(GroupItem* item)
	{
		BString label(item->Text());
		if (label.Length() == 0)
			label = "(group)";

		BFont font(be_plain_font);
		font_height fontHeight;
		font.GetHeight(&fontHeight);
		float height = ceilf(fontHeight.ascent)
			+ ceilf(fontHeight.descent) + 8;
		float width = font.StringWidth(label.String()) + 16;

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
		canvas->DrawString(label.String());
		canvas->Sync();
		bitmap->Unlock();

		return bitmap;
	}

	void _SetDropTarget(int32 index, drop_zone zone)
	{
		if (index == fDropTargetIndex && zone == fDropZone)
			return;
		_InvalidateDrop(fDropTargetIndex);
		fDropTargetIndex = index;
		fDropZone = zone;
		_InvalidateDrop(fDropTargetIndex);
	}

	void _InvalidateDrop(int32 index)
	{
		if (index < 0)
			return;
		// Inflate slightly so an insertion line drawn on the row's
		// top/bottom edge is fully repainted.
		BRect frame = ItemFrame(index);
		frame.InsetBy(0, -2);
		Invalidate(frame);
	}

	int32 fDropTargetIndex;
	drop_zone fDropZone;
	KuraDatabase* fDatabase;
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
	static_cast<GroupOutlineView*>(fListView)->SetDatabase(db);
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
