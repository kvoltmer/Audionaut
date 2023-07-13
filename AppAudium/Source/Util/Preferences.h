

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


    static String s_RegistryName;
    
    static const char* defaultFile;

private:
    
    
};
