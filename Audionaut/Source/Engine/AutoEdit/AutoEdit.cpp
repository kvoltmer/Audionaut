//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <JuceHeader.h>

#include "AutoEdit.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/Export/AudioExportThread.h"

namespace audium {

const juce::String AutoEdit::getTempDirectory()
{
    // Temp directory on is ~Library/Caches/AppAudium
    return juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName();
}

bool AutoEdit::invokeAutoEdit(AutoEditConfig &config,
                              std::function<void(std::string)> callback)
{
    if (auto track = audiumEngine->getAudioTrackContainer()->getAudioTrack(config.trackId)) {
        if (auto item = track->getPlayListContainer()->playListItems.getObject(config.playlistItemId)) {
            bool success = false;
            auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
            bounceConfig->playListItem = item;
            bounceConfig->fileName = juce::File::createTempFile(".wav");
            bounceConfig->sampleRate = 48000.0;
            
            auto thread = std::make_unique<AudioExportThread>(*audiumEngine, bounceConfig);
            if (thread->runThread()) {
                // thread finished normally..
                config.bounceFileName = bounceConfig->fileName.getFullPathName().toStdString();
                
                success = invokePython(config, std::move(callback));
                
                if (success) {
                    applyAutoEditResult(bounceConfig->sampleRate);
                }
            }
            else {
                // user pressed the cancel button during export
            }
            return success;
        }
    }
    NullCheckedInvocation::invoke (callback, "Please select a valid audio clip to edit.");
    return false;
}

bool AutoEdit::invokePython(AutoEditConfig &config,
                            std::function<void(std::string)> callback)
{
    // NOTE: Make sure PATH and PYTHONPATH is set correctly.
    // With XCode you must edit the scheme and set the environment variables
    // double check with:
    // system("env");
    
    // Path to python binary
    std::string python = "python3";
    
    if (juce::File(config.bounceFileName).existsAsFile())
    {
        audioResourceFilePath = config.bounceFileName;
        // Build the command line string
        std::string commandString;
        commandString += "cd " + getTempDirectory().toStdString() + ";";
        commandString += python + " $HOME/dev/gaborgandalf/gaborgandalf/automain.py --verbose autoedit";
        //      commandString += " --assemble_mode " + config.mode;
        commandString += " --duration " + std::to_string(config.duration);
        commandString += " --numsegs " + std::to_string(config.numSegments);
        //        commandString += " --seglen_min " + std::to_string(config.minSegLength);
        //        commandString += " --seglen_max " + std::to_string(config.maxSegLength);
        commandString += " --filenames " + config.bounceFileName;
        
        try {
            // execute
            if (std::system(commandString.c_str()) == 0) {
                return true;
            }
        } catch (std::exception &e) {
            NullCheckedInvocation::invoke (callback, e.what());
        }
    }
    else {
        NullCheckedInvocation::invoke(callback, "Export audio failed.");
    }
    
    NullCheckedInvocation::invoke (callback, "Call to std::system failed.");
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
    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();
    auto track = audioTrackContainer->getDefaultGroup();
    if (track != nullptr)
    {
        auto action = std::make_unique<audium::UndoableContainerAction>(*audioTrackContainer.get());
        
        auto countString = getCountFromFile();
        jassert(countString.length() > 0);
        
        //  read segments in json format
        std::string segFileName = getTempDirectory().toStdString() + "/data/segs/" + getBaseName() + "-seg-data.json";
        createRegionsFromSegFile(segFileName, sampleRate);
        
        
        // song in json format
        auto dir = juce::File(audioResourceFilePath).getParentDirectory().getFullPathName().toStdString();
        std::string songFileName = dir + "/" + getBaseName() + "-autoedit-" + countString + ".json";
        createPlayListFromSongFile(songFileName);
        
        // Undo: store new state
        action->storeNewState();
        audioTrackContainer->getUndoManager()->perform(action.release(), "Auto Edit");
        audioTrackContainer->getUndoManager()->beginNewTransaction();
    }
}


bool AutoEdit::createRegionsFromSegFile(std::string segFileName, double sampleRate)
{
    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();
    std::fstream segFile;
    segFile.open(segFileName, std::ios::in);
    if (segFile.is_open())
    {
        if (auto track = audioTrackContainer->getDefaultGroup())
        {
            track->cleanup();
            
            int counter = 1;
            auto segdata = nlohmann::json::parse(segFile);
            // create regions from parsed result
            for (auto& elem : segdata)
            {
                juce::Range<double> position;
                position.setStart(static_cast<double>(elem["start"]) / sampleRate);
                position.setEnd(static_cast<double>(elem["end"]) / sampleRate);
                juce::String regionName = "seg-" + juce::String(counter++);
                
                
                // CREATE REGIONs:
                
                // use the first sub track
                auto resourceGroups = track->getResourceGroups();
                
                jassert(resourceGroups.size() > 0);
                if (resourceGroups.size() > 0)
                {
                    resourceGroups[0]->getAudioRegionContainer()->createRegion(regionName, position, track, resourceGroups[0], nullptr, audium::seconds);
                }
            }
        }
        segFile.close();
        return true;
    }
    else
    {
        std::cout << "error seg file not found: " << segFileName << std::endl;
        return false;
    }
}


bool AutoEdit::createPlayListFromSongFile(std::string songFileName)
{
    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();
    std::fstream songFile;
    songFile.open(songFileName, std::ios::in);
    if (songFile.is_open())
    {
        // cleanup playlist
        if (auto track = audioTrackContainer->getDefaultGroup())
        {
            track->getPlayListContainer()->playListItems.cleanup();
        }
        
        auto songData = nlohmann::json::parse(songFile);
        for (auto& elem : songData)
        {
            if (auto track = audioTrackContainer->getDefaultGroup())
            {
                auto resourceGroup = track->getResourceGroups()[0];
                auto region = resourceGroup->getAudioRegionContainer()->getRegion(elem["index"]);
                jassert(region != nullptr);
                std::string filename = elem["file"];
                jassert(juce::String(filename).contains(region->getName()));
                
                auto insertIndex = static_cast<int>(track->getPlayListContainer()->playListItems.size());
                // CREATE PLAYLIST ITEM
                track->getPlayListContainer()->createPlayListItemUI(region, insertIndex);
                
                // is the duration consitant?
                double duration = elem["duration"];
                double regionDuration = region->getRegionData(audium::seconds).getLength();
                if (!juce::approximatelyEqual(duration, regionDuration))
                {
                    std::cout << "duration not equal" << duration << " " << regionDuration << std::endl;
                }
            }
        }
        
        songFile.close();
        return true;
    }
    else
    {
        std::cout << "error file not found: " << songFileName << std::endl;
        return false;
    }
}

} // namespace audium
