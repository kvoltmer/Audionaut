//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#ifndef VOLUMEMETER_H_INCLUDED
#define VOLUMEMETER_H_INCLUDED

#include "JuceHeader.h"

using namespace juce;

/// Colour zones shared by the level meters and the DSP load meter.
namespace MeterColours
{
    inline const juce::Colour green      { 0xff23a101 };   // < -20dB
    inline const juce::Colour greenLight { 0xff61ff02 };   // > -20dB
    inline const juce::Colour orange     { 0xfffbb203 };   // > -6dB
    inline const juce::Colour red        { 0xffff0000 };   // > 0dB
}

class LevelComponent  : public Component
{
public:
    LevelComponent(){}
    
    void paint(Graphics& g) override;
    void resized() override;
    void redrawLevels();
    void setGrayscale(bool bGrayscale);
private:
    
    void drawLevels(Graphics& g);
    
    Image mImage; // image with all levels
    
    bool m_bGrayscale = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelComponent)
};

class LevelMeter : public Component
{
public:
    LevelMeter(bool bVertical, bool bInverted);
    ~LevelMeter();
    
    void paint(Graphics& g) override;
    void paintOverChildren (Graphics& g) override;
    void resized() override;
    
    float getDisplayLevel();
    float getDisplayPeak();
    
    void setLevel (float level);
    
    int getSpacing() const { return m_iSpace; }
    
    bool getInverted() const { return m_bInverted; }
    void setInverted(bool bInverted);
    
    bool getVertical() const { return m_bVertical; }
    void setVertical(bool bVertical);
    
    const float getDecibelScaled(const float db);
    
    const int getPeakHold() const { return m_iPeakHold; }
    
    void setDbMin(float dbMin);
    void setDbMax(float dbMax);
    
    void setGrayscale(bool bGrayscale);
    
    /// A dB scale beside the bar: ticks and unsigned figures on the meter's own law, in a column
    /// of scaleWidth taken from the left of the bounds (vertical meters only).
    static constexpr int scaleWidth = 18;
    static constexpr int scaleEndInset = 5;   ///< the bar's inset at the top and bottom, room for the end figures
    void setScaleShown (bool shown);
    bool isScaleShown() const { return m_bScale; }
    
    /// Where the bar begins below the meter's top (and ends above its bottom): to line the bar up with something beside it.
    int getBarInset() const { return (m_bScale && m_bVertical ? scaleEndInset : 0) + m_iSpace; }
    
private:
    bool m_bVertical;
    bool m_bInverted;
    bool m_bScale = false;
    
    float m_fMindB;
    float m_fMaxdB;
    float m_fLeveldB;
    float m_fPeakLeveldB;
    float m_fDecrementdB;
    float m_fLastLeveldB;
    
    int	m_iPeakHoldDuration;
    int	m_iPeakHold;
    int m_iSpace;
    
    std::unique_ptr<LevelComponent> m_pLevelComponent;
    juce::Rectangle<int> mRect;
    juce::Rectangle<int> m_barArea;   // the bounds less the scale column
    
    void drawScale (Graphics& g);
    

    
    static const float reverse_linear(const float fVal, const float fMin, const float fMax)
    {
        return fabsf(fVal - fMin) / fabsf(fMax - fMin);
    }
    
public:
    
    static const float gainToDecebel(const float fVolume)
    {
        return 20.f * log10f(fVolume);
    }
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
    
};


const int numChannels = 2;

class StereoMeter : public Component
{
public:
    StereoMeter();
    ~StereoMeter();
    
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
