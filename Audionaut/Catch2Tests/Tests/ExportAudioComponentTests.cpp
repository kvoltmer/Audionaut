//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>
#include <JuceHeader.h>

#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Interface/Dialogs/ExportAudioComponent.h"

#include "TestEngine.h"

using namespace juce;

namespace {

/** The component's combo boxes in child order: format, (sample rate when a
    device is open), output channels, bit depth, then quality once created. */
std::vector<ComboBox*> comboBoxesOf (Component& component)
{
    std::vector<ComboBox*> combos;
    for (auto* child : component.getChildren())
        if (auto* combo = dynamic_cast<ComboBox*> (child))
            combos.push_back (combo);
    return combos;
}

} // namespace

SCENARIO ("the export dialog offers a quality for a lossy format", "[interface][export][ogg]")
{
    TestEngine engine;
    AudiumLookAndFeel lookAndFeel;

    ExportAudioComponent component (engine.ptr());
    component.setLookAndFeel (&lookAndFeel);
    component.update();

    auto* format = comboBoxesOf (component).front();

    GIVEN ("Ogg Vorbis chosen as the format")
    {
        format->setSelectedId ((int) audium::ExportFormat::ogg + 1, sendNotificationSync);
        auto* quality = comboBoxesOf (component).back();

        THEN ("the quality row shows, set to the default 192 kbps, in place of the bit depth")
        {
            REQUIRE (component.getFormat() == audium::ExportFormat::ogg);
            REQUIRE (quality->isVisible());
            REQUIRE (quality->getNumItems() == 11);
            REQUIRE (quality->getText() == "192 kbps");
            REQUIRE (component.getQuality() == audium::defaultQuality (audium::ExportFormat::ogg));
            REQUIRE_FALSE (quality->getBounds().isEmpty());

            for (auto* combo : comboBoxesOf (component))
                if (combo != quality && combo->getText().endsWith ("Bits"))
                    REQUIRE_FALSE (combo->isVisible());
        }

        AND_WHEN ("another quality is picked and the format switched away and back")
        {
            quality->setSelectedId (3, sendNotificationSync);
            format->setSelectedId ((int) audium::ExportFormat::flac + 1, sendNotificationSync);
            format->setSelectedId ((int) audium::ExportFormat::ogg + 1, sendNotificationSync);

            THEN ("the picked quality is kept")
            {
                REQUIRE (quality->getText() == "96 kbps");
                REQUIRE (component.getQuality() == 2);
            }
        }
    }

    component.setLookAndFeel (nullptr);
}
