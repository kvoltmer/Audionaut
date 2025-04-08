//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#ifndef VOLUMEMETER_H_INCLUDED
#define VOLUMEMETER_H_INCLUDED

#include "JuceHeader.h"

using namespace juce;

//==============================================================================
class LevelComponent  : public Component
{
public:
    LevelComponent(){}
    
    void paint(Graphics& g) override;
    void resized() override;
    void redrawLevels();
private:
    
    void drawLevels(Graphics& g);
    
    Image mImage; // image with all levels
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelComponent)
};

//==============================================================================
class LevelMeter : public Component
{
public:
    LevelMeter(bool bVertical, bool bInverted);
    ~LevelMeter();
    
    void paint(Graphics& g) override;
    void resized() override;
    
    float getDisplayLevel();
    float getDisplayPeak();
    
    void setLevel (float level);
    void setPeakLevel (float level);
    
    int getSpacing() const { return m_iSpace; }
    
    bool getInverted() const { return m_bInverted; }
    void setInverted(bool bInverted);
    
    bool getVertical() const { return m_bVertical; }
    void setVertical(bool bVertical);
    
    const float getDecibelScaled(const float db);
    
    const int getPeakHold() const { return m_iPeakHold; }
    
    void setDbMin(float dbMin);
    void setDbMax(float dbMax);
    
private:
    bool m_bVertical;
    bool m_bInverted;
    
    float m_fMindB;
    float m_dMaxdB;
    float m_fLeveldB;
    float m_fPeakLeveldB;
    
    int	m_iPeakHoldDuration;
    int	m_iPeakHold;
    
    float m_fLastLevel;
    
    int m_iSpace;
    
    std::unique_ptr<LevelComponent> m_pLevelComponent;
    juce::Rectangle<int> mRect;
    

    
    static const float reverse_linear(const float fVal, const float fMin, const float fMax)
    {
        return fabsf(fVal - fMin) / fabsf(fMax - fMin);
    }
    
public:
    
    static const float gainToDecebel(const float fVolume)
    {
        return 20.f * log10f(fVolume);
    }
    
    static const float decebelToGain(const float fDecibel)
    {
        return powf(10.f, (0.05f * fDecibel));
    }
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
    
};


//==============================================================================

const int numChannels = 2;

class StereoMeter : public Component
{
public:
    StereoMeter();
    ~StereoMeter();
    
    //void paint(Graphics& g);
    void resized();
    void setLevel(int channel, float level);
    
    bool getInverted() const { return m_bInverted; }
    void setInverted(bool bInverted);
    
    bool getVertical() const { return m_bVertical; }
    void setVertical(bool bVertical);
    
    void setDbMin(float dbMin);
    void setDbMax(float dbMax);
    
private:
    
    std::unique_ptr<LevelMeter> m_pLevelMeter[numChannels];
    
    bool m_bVertical;
    bool m_bInverted;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoMeter)
};


#endif  // VOLUMEMETER_H_INCLUDED
