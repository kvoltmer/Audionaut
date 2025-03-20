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

#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "Preferences.h"


#if JUCE_WINDOWS
    static String regPath = "HKEY_CURRENT_USER\\Software\\Voltmer\\";
#else
    static String regPath = "com.voltmer.";
#endif

String Preferences::s_RegistryName = "";

//const char* Preferences::defaultFile = "DefaultProjectFile";


String Preferences::getRegPath()
{
    jassert(s_RegistryName.isNotEmpty()); // call init!

#if JUCE_MAC
    return String(regPath + s_RegistryName);
#else
    return String(regPath + s_RegistryName + "\\");
#endif
}

String Preferences::getAbsolutePlistPath()
{
#if JUCE_MAC
    String test = "/Users/";
    test += getenv("USER");
    test += "/Library/Preferences/";
    test += getRegPath();
    return test;
#else
    return "";
#endif
}

void Preferences::init(const String& product)
{
    s_RegistryName = product;
#if JUCE_MAC
    CFStringRef prefsHandle = getRegPath().toCFString();
    ::CFPreferencesAddSuitePreferencesToApp(kCFPreferencesCurrentApplication, prefsHandle);
    ::CFRelease(prefsHandle);
#endif
}

void Preferences::deinit()
{
    synchronize();
}

void Preferences::synchronize()
{
#if JUCE_MAC
    CFStringRef prefsHandle = getRegPath().toCFString();
    ::CFPreferencesAppSynchronize(prefsHandle);
    ::CFRelease(prefsHandle);
#endif
}

String Preferences::getValue(const String& key, const String& defaultValue)
{
#if JUCE_WINDOWS
	return WindowsRegistry::getValue(getRegPath() + key, defaultValue);
#else
    String theValue = defaultValue;
    
    CFStringRef	keyRef = key.toCFString();
    CFStringRef prefsHandle = getRegPath().toCFString();
    CFStringRef	dataRef	= reinterpret_cast<CFStringRef>( ::CFPreferencesCopyAppValue(keyRef, prefsHandle));

    if (dataRef == NULL) // fallback sandboxed app
    {
        dataRef = reinterpret_cast<CFStringRef>(CFPreferencesCopyValue(key.toCFString(),
                                                                       Preferences::getAbsolutePlistPath().toCFString(),
                                                                       CFSTR("kCFPreferencesCurrentUser"),
                                                                       CFSTR("kCFPreferencesAnyHost")));
    }
    

    if(dataRef)
    {
        if (::CFGetTypeID(dataRef) == ::CFStringGetTypeID())
        {
            theValue = String::fromCFString(dataRef);
        }
        ::CFRelease(dataRef);
    }
    
    ::CFRelease(keyRef);
    ::CFRelease(prefsHandle);
    
    return theValue;
#endif
}

bool Preferences::setValue(const String& key, const String& value)
{
#if JUCE_WINDOWS
	return WindowsRegistry::setValue(getRegPath() + key, value);
#else
    CFStringRef keyRef		= key.toCFString();
    CFStringRef valueRef	= value.toCFString();
    CFStringRef prefsHandle = getRegPath().toCFString();
    ::CFPreferencesSetAppValue(keyRef, valueRef, prefsHandle);
    ::CFRelease(prefsHandle);
    ::CFRelease(valueRef);
    ::CFRelease(keyRef);
    
    return true;
#endif
}

bool Preferences::valueExists(const String& key)
{
#if JUCE_WINDOWS
	return WindowsRegistry::valueExists(getRegPath() + key);
#else
    bool result = false;
    CFStringRef prefsHandle = getRegPath().toCFString();
    CFArrayRef keyList = ::CFPreferencesCopyKeyList(prefsHandle, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    
    if (keyList)
    {
        CFIndex keyListSize = ::CFArrayGetCount(keyList);
        CFStringRef keyRef = key.toCFString();
        result = ::CFArrayContainsValue(keyList, CFRangeMake(0,keyListSize), keyRef);
        ::CFRelease(keyRef);
        ::CFRelease(keyList);
    }
    else // fallback sandboxed app
    {
        CFStringRef dataRef    = reinterpret_cast<CFStringRef>(::CFPreferencesCopyValue(key.toCFString(),
                                                                        getAbsolutePlistPath().toCFString(),
                                                                        CFSTR("kCFPreferencesCurrentUser"),
                                                                        CFSTR("kCFPreferencesAnyHost")));
        if(dataRef &&
           (::CFGetTypeID(dataRef) == ::CFNumberGetTypeID() ||
            ::CFGetTypeID(dataRef) == ::CFStringGetTypeID()))
        {
            result = true;
            ::CFRelease(dataRef);
        }
        
    }
    
    
    ::CFRelease(prefsHandle);

    return result;
#endif
}

void Preferences::removeKey(const String& key)
{
    if (valueExists(key))
    {
        setValue(key, "");
    }
}

bool Preferences::getString (const std::string& key, std::string& value)
{
    if (valueExists(key)) {
        String result = getValue(key, value);
        value = result.toStdString();
        return true;
    }
    return false;
}

int Preferences::getIntegerValue(const String& key, const int defaultValue)
{
#if JUCE_WINDOWS
    String defaultStr(defaultValue);
    
    
    MemoryBlock mem;
    uint32 type = WindowsRegistry::getBinaryValue (getRegPath() + key, mem);
	if (mem.getData())
	{
		return (int) *reinterpret_cast<const unsigned long*> (mem.getData());
	}

	return 0;

    
#else
    int theValue = defaultValue;
    
    CFStringRef	keyRef = key.toCFString();
    CFStringRef prefsHandle = getRegPath().toCFString();
    
    CFNumberRef	dataRef	= reinterpret_cast<CFNumberRef>( ::CFPreferencesCopyAppValue(keyRef, prefsHandle));
    
    if (dataRef == NULL) // fallback sandboxed app
    {
        dataRef = reinterpret_cast<CFNumberRef>(CFPreferencesCopyValue(key.toCFString(),
                                                                       Preferences::getAbsolutePlistPath().toCFString(),
                                                                       CFSTR("kCFPreferencesCurrentUser"),
                                                                       CFSTR("kCFPreferencesAnyHost")));
    }
    
    if(dataRef && ::CFGetTypeID(dataRef) == ::CFNumberGetTypeID())
    {
        ::CFNumberGetValue(dataRef, kCFNumberLongType, &theValue);
    }
    
    if (dataRef) {
        ::CFRelease(dataRef);
    }
    
    ::CFRelease(keyRef);
    ::CFRelease(prefsHandle);
    
    return theValue;
#endif
    
}

bool Preferences::setIntegerValue(const String& key, const uint64_t& value)
{
#if JUCE_WINDOWS
    return WindowsRegistry::setValue(getRegPath() + key, (uint32)value);
#else
    CFStringRef	keyRef = key.toCFString();
    CFStringRef prefsHandle = getRegPath().toCFString();
    
    ::CFNumberRef number = ::CFNumberCreate(0, kCFNumberLongType, &value);
    ::CFPreferencesSetAppValue(keyRef, number, prefsHandle);
    ::CFRelease(number);
    ::CFRelease(prefsHandle);
    ::CFRelease(keyRef);
    
    return true;
    
    
#endif
    
}

JUCE_IMPLEMENT_SINGLETON (Preferences)



