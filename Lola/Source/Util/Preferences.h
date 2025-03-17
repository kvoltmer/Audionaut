//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <JuceHeader.h>

using namespace juce;

class Preferences
{
public:
    Preferences() = default;
    
    static String getRegPath();
    
    static String getAbsolutePlistPath();

    static void init(const String& product);
    static void deinit();
    
    static void synchronize();
    
	static String getValue(const String& key, const String& defaultValue = "");
	static bool setValue(const String& key, const String& value);
    
    static int getIntegerValue(const String& key, const int defaultValue = 0);
    static bool setIntegerValue(const String& key, const uint64_t& value);
    
    static bool valueExists(const String& key);
    
    static bool getString (const std::string& key, std::string& value);

    static void removeKey(const String& key);

    static String s_RegistryName;

private:
    
    
};

namespace PreferenceKeys
{
    static const char* const defaultFile = "DefaultProjectFile";
    static const char* const initialOpenDirectory = "InitialOpenDirectory";
    static const char* const initialSaveDirectory = "InitialSaveDirectory";
    static const char* const recentFiles = "RecentProjectFiles";
}
