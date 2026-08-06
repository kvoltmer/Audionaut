//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <iostream>
#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/AutoEdit/AutoEdit.h"
#include "Interface/Dialogs/AutoEditComponent.h"
#include "Interface/Dialogs/FloatingToolWindow.h"
#include "Util/Preferences.h"

using namespace juce;

class AutoEditDialog
{

public:
    AutoEditDialog(std::shared_ptr<audium::AudiumEngine> audiumEngine_) :
        audiumEngine(audiumEngine_)
    {
    }

    void invoke(std::shared_ptr<audium::AudiumEngine> engine)
    {
        audiumEngine = engine;

        if (window != nullptr) {
            autoEditComponent->update();
            window->toFront (true);
            return;
        }

        // The window owns the component; both live until the user closes the
        // window, which resets `window` again.
        autoEditComponent = new AutoEditComponent (audiumEngine);
        autoEditComponent->onApply = [this]() {
            apply();

            // The window owns the button whose click invoked this callback,
            // so it must not be destroyed before the click has finished.
            MessageManager::callAsync ([safeThis = WeakReference<AutoEditDialog> { this }]() {
                if (safeThis != nullptr)
                    safeThis->close();
            });
        };
        auto w = autoEditComponent->getWidth();
        auto h = autoEditComponent->getHeight();
        new FloatingToolWindow (autoEditComponent->getTitle().toStdString(),
                                audium::PreferenceKeys::autoEditWindowState,
                                autoEditComponent, window, false,
                                w, h, w, h, w, h);

        // Preview the pending edit's cuts in the arrangement.
        autoEditComponent->update();
    }

private:

    void close()
    {
        window.reset();
        autoEditComponent = nullptr;
    }

    void apply()
    {
        auto autoEdit = std::make_unique<audium::AutoEdit>(audiumEngine);
        autoEdit->invokeAutoEdit(autoEditComponent->getAutoEditConfig(), [](std::string error) {
#if CATCH2_TESTS
            std::cout << "error: " << error << std::endl;
#else
            NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                  "Error",
                                                  String(error));
#endif
        });
    }

    AutoEditComponent* autoEditComponent = nullptr;
    std::unique_ptr<Component> window;

    std::shared_ptr<audium::AudiumEngine> audiumEngine;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AutoEditDialog)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoEditDialog)
};
