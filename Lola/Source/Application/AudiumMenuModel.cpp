//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumMenuModel.h"
#include "Application/AudiumApplication.h"

AudiumMenuModel::AudiumMenuModel()
{
    setApplicationCommandManagerToWatch (&AudiumApplication::getCommandManager());
}

StringArray AudiumMenuModel::getMenuBarNames()
{
    return AudiumApplication::getApp().getMenuNames();
}

PopupMenu AudiumMenuModel::getMenuForIndex (int /*topLevelMenuIndex*/, const String& menuName)
{
    return AudiumApplication::getApp().createMenu (menuName);
}

void AudiumMenuModel::menuItemSelected (int menuItemID, int /*topLevelMenuIndex*/)
{
    AudiumApplication::getApp().handleMainMenuCommand (menuItemID);
}

