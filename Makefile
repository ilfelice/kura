## Kura - Password Manager for the Haiku operating system
## Makefile

NAME = Kura
TYPE = APP
SRCS = \
	src/KuraApp.cpp \
	src/KuraWindow.cpp \
	src/KuraCrypto.cpp \
	src/KuraDatabase.cpp \
	src/UnlockWindow.cpp \
	src/GroupListView.cpp \
	src/EntryListView.cpp \
	src/DetailView.cpp \
	src/FieldView.cpp \
	src/EntryEditWindow.cpp \
	src/GroupEditWindow.cpp \
	src/KuraSettings.cpp \
	src/PasswordGeneratorWindow.cpp \
	src/KuraClipboard.cpp \
	src/KuraCsvImport.cpp \
	src/StatusBar.cpp \
	src/SettingsWindow.cpp \
	src/AboutWindow.cpp \
	src/SearchTextControl.cpp

RDEFS = src/Kura.rdef
RSRCS =
LIBS = be tracker shared localestub crypto ssl columnlistview $(STDCPPLIBS)
LIBPATHS =
SYSTEM_INCLUDE_PATHS = /boot/system/develop/headers/private/interface
LOCAL_INCLUDE_PATHS = src
OPTIMIZE := FULL
LOCALES =
DEFINES =
WARNINGS = TRUE
SYMBOLS := FALSE
DEBUGGER := FALSE
COMPILER_FLAGS =
LINKER_FLAGS =

## Include the Makefile-Engine
DEVEL_DIRECTORY := /boot/system/develop
include $(DEVEL_DIRECTORY)/etc/makefile-engine
