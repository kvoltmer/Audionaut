/*
  ==============================================================================

    AutoEdit.cpp
    Created: 14 Sep 2023 3:13:12pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <JuceHeader.h>

#include "AutoEdit.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/AudioRegionContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioGroupContainer.h"

const juce::String AutoEdit::getTempDirectory()
{
    // Temp directory on is ~Library/Caches/AppAudium
    return juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName();
}

bool AutoEdit::invokeAutoEdit(AutoEditConfig config)
{
    // NOTE: Make sure PATH and PYTHONPATH is set correctly.
    // With XCode you must edit the scheme and set the environment variables
    // double check with:
    // system("env");
        
    // Path to python binary
    std::string python = "python3";
    
    if(juce::File(config.bounceFileName).existsAsFile())
    {
        audioResourceFilePath = config.bounceFileName;
        // Build the command line string
        std::string commandString;
        commandString += "cd " + getTempDirectory().toStdString() + ";";
        commandString += python + " $HOME/dev/smp_audio/scripts/automain.py --verbose autoedit";
//      commandString += " --assemble_mode " + config.mode;
        commandString += " --duration " + std::to_string(config.duration);
        commandString += " --numsegs " + std::to_string(config.numSegments);
//        commandString += " --seglen_min " + std::to_string(config.minSegLength);
//        commandString += " --seglen_max " + std::to_string(config.maxSegLength);
        commandString += " --filenames " + config.bounceFileName;
        
        // execute
        auto result = std::system(commandString.c_str());
        if (result == 0)
        {
            return true;
        }
    }
    else
    {
        juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "no audio data", "bounce audio failed.");
    }
    
    return false;
}

const std::string AutoEdit::getBaseName() const
{
    return juce::File(audioResourceFilePath).getFileNameWithoutExtension().toStdString();
}

const std::string AutoEdit::getCountFromFile() const
{
    // read count.txt
    std::fstream countFile;
    std::string countFileName = getTempDirectory().toStdString() + "/data/autoedit/count.txt";
    countFile.open(countFileName, std::ios::in);
    std::string count;
    if (countFile.is_open())
    {
        if (getline(countFile, count))
        {
            std::cout << "count = " << count << std::endl;
        }
        countFile.close();
        return count;
    }
    else
    {
        std::cout << "error count.txt file not found: " << countFileName << std::endl;
        return "";
    }
}
void AutoEdit::applyAutoEditResult(double sampleRate)
{
    auto countString = getCountFromFile();
    jassert(countString.length());
    

    
    
    //  read segments in json format
    std::string segFileName = getTempDirectory().toStdString() + "/data/segs/" + getBaseName() + "-seg-data.json";
    std::fstream segFile;
    segFile.open(segFileName, std::ios::in);
    if (segFile.is_open())
    {
        int counter = 1;
        auto segdata = nlohmann::json::parse(segFile);
        // create regions from parsed result
        for (auto& elem : segdata)
        {
            juce::Range<double> position;
            position.setStart(static_cast<double>(elem["start"]) / sampleRate);
            position.setEnd(static_cast<double>(elem["end"]) / sampleRate);
            juce::String regionName = "seg-" + juce::String(counter++);
            // CREATE REGION
            for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
            {
                if (auto group = audioGroupContainer->getAudioGroup(i))
                {
                    auto resources = group->getAudioResources();
                    jassert(resources.size() > 0);
                    if (resources.size() > 0)
                    {
                        auto resource = resources[0];
                        audioRegionContainer->createRegion(regionName, position, group);
                    }
                }
            }
        }
        segFile.close();
    }
    else
    {
        std::cout << "error seg file not found: " << segFileName << std::endl;
        return;
    }
    
    // cleanup all playlists
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        if (auto group = audioGroupContainer->getAudioGroup(i))
        {
            group->getPlayListContainer()->cleanup();
        }
    }
    
    // song in json format
    auto dir = juce::File(audioResourceFilePath).getParentDirectory().getFullPathName().toStdString();
    std::string songFileName = dir + "/" + getBaseName() + "-autoedit-" + countString + ".json";
    std::fstream songFile;
    songFile.open(songFileName, std::ios::in);
    if (songFile.is_open())
    {
        auto songData = nlohmann::json::parse(songFile);
        for (auto& elem : songData)
        {
            auto region = audioRegionContainer->getRegion(elem["index"]);
            jassert(region != nullptr);
            std::string filename = elem["file"];
            jassert(juce::String(filename).contains(region->getName()));
            
            for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
            {
                if (auto group = audioGroupContainer->getAudioGroup(i))
                {
                    auto insertIndex = static_cast<int>(group->getPlayListContainer()->playListItems.size());
                    // CREATE PLAYLIST ITEM
                    group->getPlayListContainer()->createPlayListItem(elem["index"], insertIndex);
                    
                }
            }
            
            
            // is the duration consitant?
            double duration = elem["duration"];
            double regionDuration = region->getRegionDataInSeconds().getLength();
            if (!juce::approximatelyEqual(duration, regionDuration))
            {
                std::cout << "duration not equal" << duration << " " << regionDuration << std::endl;
            }
        }
        songFile.close();
    }
    else
    {
        std::cout << "error file not found: " << songFileName << std::endl;
        return;
    }
    
    // updateUI
    audioGroupContainer->sendActionMessage("");
}
