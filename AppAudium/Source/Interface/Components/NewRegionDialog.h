/*
  ==============================================================================

  This is an automatically generated GUI class created by the Projucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Projucer version: 7.0.5

  ------------------------------------------------------------------------------

  The Projucer is part of the JUCE library.
  Copyright (c) 2020 - Raw Material Software Limited.

  ==============================================================================
*/

#pragma once

//[Headers]     -- You can add your own extra header files here --
#include <JuceHeader.h>
using namespace juce;
//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Projucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class NewRegionDialog  : public juce::Component
{
public:
    //==============================================================================
    NewRegionDialog ();
    ~NewRegionDialog() override;

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    void createNewFileInternal ()
    {
        asyncAlertWindow = std::make_unique<AlertWindow> (TRANS ("Create new Component class"),
                                                          TRANS ("Please enter the name for the new class"),
                                                          MessageBoxIconType::NoIcon, nullptr);

        asyncAlertWindow->addTextEditor ("New Region", String(), String(), false);
        asyncAlertWindow->addButton (TRANS ("Create Files"),  1, KeyPress (KeyPress::returnKey));
        asyncAlertWindow->addButton (TRANS ("Cancel"),        0, KeyPress (KeyPress::escapeKey));

//        auto resultCallback = [safeThis = WeakReference<NewRegionDialog> { this }, parent] (int result)
//        {
//            if (safeThis == nullptr)
//                return;
//
//            auto& aw = *(safeThis->asyncAlertWindow);
//
//            aw.exitModalState (result);
//            aw.setVisible (false);
//
//            if (result == 0)
//                return;
//
//            //const String className (aw.getTextEditorContents (getClassNameFieldName()).trim());
//
////            if (className == build_tools::makeValidIdentifier (className, false, true, false))
////            {
////                safeThis->askUserToChooseNewFile (className + ".h", "*.h;*.cpp",
////                                                  parent,
////                                                  [safeThis, parent, className] (File newFile)
////                                                  {
////                                                      if (safeThis == nullptr)
////                                                          return;
////
////                                                      if (newFile != File())
////                                                          safeThis->createFiles (parent, className, newFile);
////                                                  });
////
////                return;
////            }
//
////            safeThis->createNewFileInternal (parent);
//        };

        //asyncAlertWindow->enterModalState (true, ModalCallbackFunction::create (std::move (resultCallback)), false);
        asyncAlertWindow->enterModalState (true, nullptr, false);
    }

    std::unique_ptr<AlertWindow> asyncAlertWindow;
    //[/UserMethods]

    void paint (juce::Graphics& g) override;
    void resized() override;



private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    //[/UserVariables]

    //==============================================================================


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewRegionDialog)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

