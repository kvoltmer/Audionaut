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

AutoEdit::AutoEdit(std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                   std::shared_ptr<AudioRegionContainer> audioRegionContainer,
                   std::shared_ptr<PlayListContainer> playListContainer) :
    audioResourceContainer(audioResourceContainer),
    audioRegionContainer(audioRegionContainer),
    playListContainer(playListContainer)
{
}

const std::string getTempDirectory()
{
    // Temp directory on is ~Library/Caches/AppAudium
    return juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName().toStdString();
}

bool AutoEdit::invokeAutoEdit(const AutoEditConfig config)
{
    // NOTE: Make sure PATH and PYTHONPATH is set correctly.
    // With XCode you must edit the scheme and set the environment variables
    // double check with:
    // system("env");
        
    // Path to python binary
    std::string python = "/opt/homebrew/bin/python3";
    
    
    // For now we simply use the first audio resource of the project
    if (audioResourceContainer->getNumAudioResources() > 0)
    {
        audioResourceFilePath = audioResourceContainer->getAudioResource(0)->getFullPathName().toStdString();
        
        // Build the command line string
        std::string commandString;
        commandString += "cd " + getTempDirectory() +";";
        commandString += python + " $HOME/dev/smp_audio/scripts/automain.py --verbose autoedit";
//      commandString += " --assemble_mode " + config.mode;
        commandString += " --duration " + std::to_string(config.duration);
        commandString += " --numsegs " + std::to_string(config.numSegments);
//        commandString += " --seglen_min " + std::to_string(config.minSegLength);
//        commandString += " --seglen_max " + std::to_string(config.maxSegLength);
        commandString += " --filenames " + audioResourceFilePath;
        
        // execute
        auto result = std::system(commandString.c_str());
        if (result == 0)
        {
            return true;
        }
    }
    else
    {
        juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "no audio data", "you must at least load one audio file to use auto edit");
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
    std::string countFileName = getTempDirectory() + "/data/autoedit/count.txt";
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
void AutoEdit::applyAutoEditResult()
{
    auto countString = getCountFromFile();
    jassert(countString.length());
    
    // clear playlist and regions
    playListContainer->cleanup();
    audioRegionContainer->cleanup();
    
    // get sample rate from audio resource
    jassert(audioResourceContainer->getAudioResource(0));
    auto sampleRate = audioResourceContainer->getAudioResource(0)->getSampleRate();
    
    //  read segments in json format
    std::string segFileName = getTempDirectory() + "/data/segs/" + getBaseName() + "-seg-data.json";
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
            audioRegionContainer->createRegion(regionName, position, audioResourceContainer->getDefaultGroup());
        }
        segFile.close();
    }
    else
    {
        std::cout << "error seg file not found: " << segFileName << std::endl;
        return;
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
            jassert(juce::String(filename).contains(region->name));
            
            auto insertIndex = static_cast<int>(playListContainer->playListItems.size());
            // CREATE PLAYLIST ITEM
            playListContainer->createPlayListItem(elem["index"], insertIndex);
            
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
    playListContainer->sendActionMessage("");
}
