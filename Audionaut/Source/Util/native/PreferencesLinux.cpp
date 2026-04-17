
#include "Util/Preferences.h"

using namespace juce;

namespace audium {

class Preferences::Impl
{
public:
    explicit Impl () {}
        
    std::unique_ptr<juce::PropertiesFile> propertiesFile;

private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Impl)
    JUCE_DECLARE_NON_MOVEABLE (Impl)
};

Preferences::Preferences() :
    impl (std::make_unique<Impl> ())
{
}

Preferences::~Preferences() = default;

void Preferences::init(const std::string& product, const std::string& manufacture)
{
    PropertiesFile::Options options;
    options.applicationName     = product;
    options.filenameSuffix      = "settings";
    options.osxLibrarySubFolder = "Application Support";
   #if JUCE_LINUX || JUCE_BSD
    options.folderName          = "~/.config/" + manufacture;
   #else
    options.folderName          = manufacture;
   #endif

    impl->propertiesFile = std::make_unique<juce::PropertiesFile>(options);

    std::cout << "settings: " << impl->propertiesFile->getFile().getFullPathName() << std::endl;
}

void Preferences::synchronize()
{
    impl->propertiesFile->saveIfNeeded();
}

std::string Preferences::getValue(const std::string& key,
                                  const std::string& defaultValue)
{
    return impl->propertiesFile->getValue(juce::StringRef(key)).toStdString();
}

bool Preferences::setValue(const std::string& key,
                           const std::string& value)
{
    impl->propertiesFile->setValue(juce::StringRef(key), juce::String(value));
    return true;
}

bool Preferences::valueExists(const std::string& key)
{
    return impl->propertiesFile->containsKey(juce::StringRef(key));
}

void Preferences::removeKey(const std::string& key)
{
    if (valueExists(key)) {
        impl->propertiesFile->removeValue(juce::StringRef(key));
    }
}

} // namespace audium
