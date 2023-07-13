/*
  ==============================================================================

    AudiumMenuModel.cpp
    Created: 11 May 2023 4:57:44pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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

