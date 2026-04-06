

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
    return std::string("HKEY_CURRENT_USER\\Software\\" + registryManufacture + "\\" + registryProduct + "\\");
}

void Preferences::init(const std::string& product, const std::string& manufacture)
{
    impl->registryProduct = product;
    impl->registryManufacture = manufacture;
}

void Preferences::synchronize()
{
}

std::string Preferences::getValue(const std::string& key,
                                  const std::string& defaultValue)
{
    return WindowsRegistry::getValue(impl->getRegPath() + key, defaultValue);
}

bool Preferences::setValue(const std::string& key,
                           const std::string& value)
{
    return WindowsRegistry::setValue(impl->getRegPath() + key, value);
}

bool Preferences::valueExists(const std::string& key)
{
    return WindowsRegistry::valueExists(impl->getRegPath() + key);
}

void Preferences::removeKey(const std::string& key)
{
    if (valueExists(key)) {
        setValue(key, "");
    }
}

} // namespace audium
