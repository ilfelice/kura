/*
 * AboutWindow.cpp
 * Kura - Password Manager for Haiku
 * Distributed under the terms of the MIT License.
 * Copyright 2026 Il Felice.
 *
 * About dialog implementation.
 */

#include "AboutWindow.h"
#include "KuraDefs.h"
#include "KuraUtils.h"

#include <AppFileInfo.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <File.h>
#include <Font.h>
#include <GroupView.h>
#include <IconUtils.h>
#include <LayoutBuilder.h>
#include <Resources.h>
#include <Roster.h>
#include <ScrollView.h>
#include <String.h>
#include <StringView.h>
#include <TextView.h>
#include <View.h>

#include <cstring>
#include <new>


enum {
	kMsgAboutOk = 'abok',
};


// --- Text shown in the about box (edit freely) ---

static const char* kAppName = "Kura";

static const char* kAppDescription =
	"Kura (\xe2\x80\x9c\xe8\x94\xb5\xe2\x80\x9d, a traditional Japanese "
	"storehouse) is a password manager for the Haiku operating system. "
	"It keeps your usernames, passwords and related secrets in a single "
	"encrypted database that only you can open.";

static const char* kAppSecurity =
	"Your database is encrypted with AES-256-GCM. The key is derived "
	"from your master password with PBKDF2-HMAC-SHA256, and the "
	"plaintext is held in memory only while the database is unlocked.";

static const char* kAppLicense =
	"Created with the assistance of AI tools.\n"
	"Distributed under the terms of the MIT License.\n"
	"Copyright 2026 Il Felice.";

static const char* kCreditsHeader = "Credits:";
static const char* kCreditsBody =
	"OpenSSL for the cryptographic primitives.\n"
	"KeePass and KeePassXC as sources of inspiration and for CSV "
	"import compatibility.\n"
	"The Haiku Project for the operating system and its API.";


// Read a human-readable version string from the app's resources,
// falling back to the app_version short_info, then a literal.
static BString
_VersionString()
{
	BString result;

	entry_ref appRef;
	if (be_roster->FindApp(kAppSignature, &appRef) == B_OK) {
		BFile appFile(&appRef, B_READ_ONLY);
		if (appFile.InitCheck() == B_OK) {
			BAppFileInfo info(&appFile);
			version_info versionInfo;
			if (info.InitCheck() == B_OK
				&& info.GetVersionInfo(&versionInfo,
					B_APP_VERSION_KIND) == B_OK) {
				if (versionInfo.short_info[0] != '\0')
					result = versionInfo.short_info;
			}
		}
	}

	if (result.IsEmpty())
		result = "Kura";

	// short_info is like "Kura - 0.2 alpha"; show just the version
	int32 dash = result.FindFirst(" - ");
	if (dash >= 0)
		result.Remove(0, dash + 3);

	BString out("Version ");
	out << result;
	return out;
}


// Load the application icon at the given size.
static BBitmap*
_AppIcon(float size)
{
	if (be_app == NULL)
		return NULL;
	BResources* resources = be_app->AppResources();
	if (resources == NULL)
		return NULL;

	size_t dataSize = 0;
	const void* data = resources->LoadResource(B_VECTOR_ICON_TYPE,
		"BEOS:ICON", &dataSize);
	if (data == NULL || dataSize == 0)
		return NULL;

	BBitmap* bitmap = new(std::nothrow) BBitmap(
		BRect(0, 0, size - 1, size - 1), B_RGBA32);
	if (bitmap == NULL)
		return NULL;

	if (BIconUtils::GetVectorIcon((const uint8*)data, dataSize,
			bitmap) != B_OK) {
		delete bitmap;
		return NULL;
	}
	return bitmap;
}


// Icon display view (owns its bitmap)
class IconView : public BView {
public:
	IconView(BBitmap* icon)
		:
		BView("icon", B_WILL_DRAW),
		fIcon(icon)
	{
		float size = icon != NULL ? icon->Bounds().Width() + 1 : 64;
		SetExplicitMinSize(BSize(size, size));
		SetExplicitMaxSize(BSize(size, size));
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	}

	virtual ~IconView()
	{
		delete fIcon;
	}

	virtual void Draw(BRect updateRect)
	{
		if (fIcon == NULL)
			return;
		SetDrawingMode(B_OP_ALPHA);
		DrawBitmap(fIcon, BPoint(0, 0));
		SetDrawingMode(B_OP_COPY);
	}

private:
	BBitmap* fIcon;
};


AboutWindow::AboutWindow(BWindow* target)
	:
	BWindow(BRect(0, 0, 460, 300), "About Kura",
		B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_CLOSE_ON_ESCAPE),
	fTextView(NULL),
	fTarget(target)
{
	IconView* iconView = new IconView(_AppIcon(64));

	// App name, large and bold
	BStringView* nameView = new BStringView("name", kAppName);
	BFont boldFont(be_bold_font);
	boldFont.SetSize(boldFont.Size() * 1.8);
	nameView->SetFont(&boldFont);

	// Version
	BStringView* versionView = new BStringView("version",
		_VersionString().String());

	// Scrollable description / credits
	fTextView = new BTextView("description");
	fTextView->SetViewUIColor(B_LIST_BACKGROUND_COLOR);
	fTextView->SetLowUIColor(B_LIST_BACKGROUND_COLOR);
	fTextView->MakeEditable(false);
	fTextView->MakeSelectable(false);
	fTextView->SetStylable(true);
	fTextView->SetInsets(8, 8, 8, 8);

	BString text;
	text << kAppDescription << "\n\n"
		<< kAppSecurity << "\n\n"
		<< kAppLicense << "\n\n"
		<< kCreditsHeader << "\n" << kCreditsBody << "\n";
	fTextView->SetText(text.String());

	rgb_color textColor = ui_color(B_LIST_ITEM_TEXT_COLOR);
	fTextView->SetFontAndColor(0, text.Length(), NULL, B_FONT_ALL,
		&textColor);

	// Bold the credits header
	BFont sectionFont(be_bold_font);
	int32 creditsPos = text.FindFirst(kCreditsHeader);
	if (creditsPos >= 0) {
		fTextView->SetFontAndColor(creditsPos,
			creditsPos + strlen(kCreditsHeader), &sectionFont,
			B_FONT_ALL, &textColor);
	}

	BScrollView* scrollView = new BScrollView("scroll", fTextView,
		B_WILL_DRAW | B_FRAME_EVENTS, false, true, B_PLAIN_BORDER);
	scrollView->SetExplicitMinSize(BSize(340, 160));

	BButton* okButton = new BButton("ok", "OK",
		new BMessage(kMsgAboutOk));
	okButton->MakeDefault(true);

	// Left column: icon, pinned to the top
	BGroupView* leftColumn = new BGroupView(B_VERTICAL);
	BLayoutBuilder::Group<>(leftColumn)
		.Add(iconView)
		.AddGlue()
	.End();

	// Right column: name, version, credits, OK
	BGroupView* rightColumn = new BGroupView(B_VERTICAL,
		B_USE_SMALL_SPACING);
	BLayoutBuilder::Group<>(rightColumn)
		.Add(nameView)
		.Add(versionView)
		.AddStrut(B_USE_SMALL_SPACING)
		.Add(scrollView, 1.0f)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(okButton)
		.End()
	.End();

	BLayoutBuilder::Group<>(this, B_HORIZONTAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(leftColumn, 0.0f)
		.Add(rightColumn, 1.0f)
	.End();

	center_over(this, fTarget);
}


AboutWindow::~AboutWindow()
{
}


void
AboutWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgAboutOk:
			PostMessage(B_QUIT_REQUESTED);
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}
