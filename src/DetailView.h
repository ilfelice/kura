/*
 * DetailView.h
 * Kura - Password Manager for Haiku
 *
 * Right panel displaying the fields of the currently selected entry.
 */
#ifndef DETAIL_VIEW_H
#define DETAIL_VIEW_H

#include <View.h>

#include "KuraDatabase.h"

class BStringView;
class BTextView;
class FieldView;


class DetailView : public BView {
public:
						DetailView();
	virtual				~DetailView();

	virtual void		MessageReceived(BMessage* message);
	virtual void		AttachedToWindow();

	// Needed to resolve the group path; pass NULL when locked.
	void				SetDatabase(KuraDatabase* db);

	void				ShowEntry(const KuraEntry* entry);
	void				TogglePasswordVisible();

private:
	BString				_GroupPath(kura_id groupId) const;

	BStringView*		fTitleView;
	BStringView*		fGroupPathView;
	BStringView*		fUsernameLabel;
	FieldView*			fUsernameField;
	BStringView*		fPasswordLabel;
	FieldView*			fPasswordField;
	BStringView*		fUrlLabel;
	FieldView*			fUrlField;
	BStringView*		fNotesLabel;
	BStringView*		fCreatedView;
	BStringView*		fModifiedView;
	BTextView*			fNotesView;

	KuraDatabase*		fDatabase;
	BString				fActualPassword;
	bool				fPasswordVisible;
	kura_id				fCurrentEntryId;
};

#endif // DETAIL_VIEW_H
