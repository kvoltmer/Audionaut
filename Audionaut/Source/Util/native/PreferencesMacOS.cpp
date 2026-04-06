
#include "Util/Preferences.h"

using namespace juce;

namespace audium {

class Preferences::Impl
{
public:
    explicit Impl () {}
        
    std::string getRegPath();
    std::string getAbsolutePlistPath();

    std::string registryProduct;
    std::string registryManufacture;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Impl)
    JUCE_DECLARE_NON_MOVEABLE (Impl)
};

Preferences::Preferences() :
    impl (std::make_unique<Impl> ())
{
}

Preferences::~Preferences() = default;

std::string Preferences::Impl::getRegPath()
{
    // call init !
    jassert(not registryProduct.empty() && not registryManufacture.empty());
    return std::string("com." + registryManufacture + "." + registryProduct);
}

std::string Preferences::Impl::getAbsolutePlistPath()
{
    std::string path = "/Users/";
    path += getenv("USER");
    path += "/Library/Preferences/";
    path += getRegPath();
    return path;
}

void Preferences::init(const std::string& product, const std::string& manufacture)
{
    impl->registryProduct = product;
    impl->registryManufacture = manufacture;

    CFStringRef prefsHandle = juce::String(impl->getRegPath()).toCFString();
    ::CFPreferencesAddSuitePreferencesToApp(kCFPreferencesCurrentApplication, prefsHandle);
    ::CFRelease(prefsHandle);
}

void Preferences::synchronize()
{
    CFStringRef prefsHandle = juce::String(impl->getRegPath()).toCFString();
    ::CFPreferencesAppSynchronize(prefsHandle);
    ::CFRelease(prefsHandle);
}

std::string Preferences::getValue(const std::string& key,
                                  const std::string& defaultValue)
{
    std::string theValue = defaultValue;
    
    CFStringRef    keyRef = juce::String(key).toCFString();
    CFStringRef prefsHandle = juce::String(impl->getRegPath()).toCFString();
    CFStringRef    dataRef    = reinterpret_cast<CFStringRef>( ::CFPreferencesCopyAppValue(keyRef, prefsHandle));
    
    if (dataRef == nullptr) {
        // fallback sandboxed app
        dataRef = reinterpret_cast<CFStringRef>(CFPreferencesCopyValue(juce::String(key).toCFString(),
                                                                       juce::String(impl->getAbsolutePlistPath()).toCFString(),
                                                                       CFSTR("kCFPreferencesCurrentUser"),
                                                                       CFSTR("kCFPreferencesAnyHost")));
    }
    
    
    if (dataRef) {
        if (::CFGetTypeID(dataRef) == ::CFStringGetTypeID()) {
            theValue = juce::String::fromCFString(dataRef).toStdString();
        }
        ::CFRelease(dataRef);
    }
    
    ::CFRelease(keyRef);
    ::CFRelease(prefsHandle);
    
    return theValue;
}

bool Preferences::setValue(const std::string& key,
                           const std::string& value)
{
    CFStringRef keyRef      = juce::String(key).toCFString();
    CFStringRef valueRef    = juce::String(value).toCFString();
    CFStringRef prefsHandle = juce::String(impl->getRegPath()).toCFString();
    ::CFPreferencesSetAppValue(keyRef, valueRef, prefsHandle);
    ::CFRelease(prefsHandle);
    ::CFRelease(valueRef);
    ::CFRelease(keyRef);
    return true;
}

bool Preferences::valueExists(const std::string& key)
{
    bool result = false;
    CFStringRef prefsHandle = juce::String(impl->getRegPath()).toCFString();
    CFArrayRef keyList = ::CFPreferencesCopyKeyList(prefsHandle, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    
    if (keyList != nullptr) {
        CFIndex keyListSize = ::CFArrayGetCount(keyList);
        CFStringRef keyRef = juce::String(key).toCFString();
        result = ::CFArrayContainsValue(keyList, CFRangeMake(0,keyListSize), keyRef);
        ::CFRelease(keyRef);
        ::CFRelease(keyList);
    }
    else {
        // fallback sandboxed app
        CFStringRef dataRef    = reinterpret_cast<CFStringRef>(::CFPreferencesCopyValue(juce::String(key).toCFString(),
                                                                                        juce::String(impl->getAbsolutePlistPath()).toCFString(),
                                                                                        CFSTR("kCFPreferencesCurrentUser"),
                                                                                        CFSTR("kCFPreferencesAnyHost")));
        if (dataRef &&
           (::CFGetTypeID(dataRef) == ::CFNumberGetTypeID() ||
            ::CFGetTypeID(dataRef) == ::CFStringGetTypeID())) {
            result = true;
            ::CFRelease(dataRef);
        }
    }
    
    ::CFRelease(prefsHandle);
    
    return result;
}

void Preferences::removeKey(const std::string& key)
{
    if (valueExists(key)) {
        setValue(key, "");
    }
}

} // namespace audium
