/*
  ==============================================================================

    LevelMeter.cpp
    Created: 14 Jan 2015 4:11:51pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "LevelMeter.h"

//==============================================================================

static Colour GreenLevel = Colour (0xff23a101);         // green < 20dB
static Colour GreenLightLevel = Colour (0xff61ff02);    // green light > 20dB
static Colour OrangeLevel = Colour (0xfffbb203);        // orange > 6dB
static Colour RedLevel = Colour(0xffff0000);            // red > 0dB

//==============================================================================
void LevelComponent::paint(Graphics& g)
{
    /* background
    g.setOpacity(0.3f);
    g.drawImageAt(mImage, 0, 0);
    g.setOpacity(1.f);
    */
    
    
    LevelMeter* parent = dynamic_cast<LevelMeter*>(getParentComponent());
    jassert(parent);
    if(parent == NULL) return;
    
    const float level = parent->getDisplayLevel();
    const bool bVertical = parent->getVertical();
    const bool bInverted = parent->getInverted();
    
    const int h = roundToInt(static_cast<float>(getHeight()) * level);
    const int w = roundToInt(static_cast<float>(getWidth()) * level);
    
	/*
    const float db0 = parent->getDecibelScaled(0.f);
    if(level >= db0)
    {
        g.setColour(RedLevel);
        g.fillRect(0, getHeight() - h, getWidth(), h);
        return;
    }
    */
    
    // get clipped image area, also see: drawLevels
    Rectangle<int> area = mImage.getBounds();
    
    if(bVertical)
    {
        area.setHeight(h);
        area.setY(bInverted ? 0 : getHeight() - h);
    }
    else
    {
        area.setWidth(w);
        area.setX(bInverted ? getWidth() - w : 0);
    }
    
    Image clippedImage = mImage.getClippedImage(area);
    
    if(bVertical)
    {
        g.drawImageAt(clippedImage, 0, bInverted ? 0 : getHeight() - h);
    }
    else
    {
        g.drawImageAt(clippedImage, bInverted ? getWidth() - w : 0, 0);
    }
    
    // draw peak
    if(parent->getPeakHold() > 0)
    {
        const float peak = parent->getDisplayPeak();
        const int ph = roundToInt(static_cast<float>(getHeight()) * peak);
        const int pw = roundToInt(static_cast<float>(getWidth()) * peak);

        Rectangle<int> areaPeak = mImage.getBounds();
        if(bVertical)
        {
            areaPeak.setHeight(1);
            areaPeak.setY(bInverted ? ph - 1 : getHeight() - ph);
        }
        else
        {
            areaPeak.setWidth(1);
            areaPeak.setX(bInverted ? getWidth() - pw : pw - 1);
        }
        
        Image peakImage = mImage.getClippedImage(areaPeak);
        
        if(bVertical)
        {
            g.drawImageAt(peakImage, 0, bInverted ? ph - 1 : getHeight() - ph);
        }
        else
        {
            g.drawImageAt(peakImage, bInverted ? getWidth() - pw : pw - 1, 0);
        }
    }
}

//==============================================================================
void LevelComponent::resized()
{
    redrawLevels();
}

//==============================================================================
void LevelComponent::redrawLevels()
{
    mImage = Image(Image::ARGB, getWidth(), getHeight(), true);
    Graphics g(mImage);
    drawLevels(g);
}

//==============================================================================
void LevelComponent::drawLevels(Graphics& g)
{
    LevelMeter* parent = dynamic_cast<LevelMeter*>(getParentComponent());
    jassert(parent);
    if(parent == NULL) return;
    
    const bool bVertical = parent->getVertical();
    const bool bInverted = parent->getInverted();
    
    const float dB20 = parent->getDecibelScaled(-20.f);
    const int height20Db = roundToInt(static_cast<float>(getHeight()) * dB20);
    const int width20Db = roundToInt(static_cast<float>(getWidth()) * dB20);
    
    const float db6 = parent->getDecibelScaled(-6.f);
    const int height6Db = roundToInt(static_cast<float>(getHeight()) * db6);
    const int width6Db = roundToInt(static_cast<float>(getWidth()) * db6);
    
    const float db0 = parent->getDecibelScaled(0.f);
    const int height0Db = roundToInt(static_cast<float>(getHeight()) * db0);
    const int width0Db = roundToInt(static_cast<float>(getWidth()) * db0);
    
    if(bVertical)
    {
        if(bInverted)
        {
            g.setColour(GreenLevel);
            g.fillRect(0, 0, getWidth(), height20Db);
            
            g.setColour(GreenLightLevel);
            g.fillRect(0, height20Db , getWidth(), height6Db - height20Db);
            
            g.setColour(OrangeLevel);
            g.fillRect(0, height6Db, getWidth(), height0Db - height6Db);
            
            g.setColour(RedLevel);
            g.fillRect(0, height0Db, getWidth(), getHeight() - height0Db);
        }
        else
        {
            g.setColour(GreenLevel);
            g.fillRect(0, getHeight() - height20Db, getWidth(), height20Db);
            
            g.setColour(GreenLightLevel);
            g.fillRect(0, getHeight() - height6Db , getWidth(), height6Db - height20Db);
            
            g.setColour(OrangeLevel);
            g.fillRect(0, getHeight() - height0Db , getWidth(), height0Db - height6Db);
            
            g.setColour(RedLevel);
            g.fillRect(0, 0, getWidth(), getHeight() - height0Db);
        }
    }
    else // horizontal
    {
        if(bInverted)
        {
            g.setColour(GreenLevel);
            g.fillRect(getWidth() - width20Db, 0, width20Db, getHeight());
            
            g.setColour(GreenLightLevel);
            g.fillRect(getWidth() - width6Db, 0, width6Db - width20Db, getHeight());
            
            g.setColour(OrangeLevel);
            g.fillRect(getWidth() - width0Db, 0, width0Db - width6Db, getHeight());
            
            g.setColour(RedLevel);
            g.fillRect(0, 0, getWidth() - width0Db, getHeight());
        }
        else
        {
            g.setColour(GreenLevel);
            g.fillRect(0, 0, width20Db, getHeight());
            
            g.setColour(GreenLightLevel);
            g.fillRect(width20Db, 0, width6Db - width20Db, getHeight());
            
            g.setColour(OrangeLevel);
            g.fillRect(width6Db, 0, width0Db - width6Db, getHeight());
            
            g.setColour(RedLevel);
            g.fillRect(width0Db, 0, getWidth() - width0Db, getHeight());
        }
    }
}

//==============================================================================
LevelMeter::LevelMeter(bool bVertical, bool bInverted) :
    m_bVertical(bVertical),
    m_bInverted(bInverted),
    m_fMindB(-60.f),
    m_dMaxdB(6.f),
    m_fLeveldB(m_fMindB),
    m_fPeakLeveldB(m_fMindB),
    m_iPeakHoldDuration(50*4),
    m_iPeakHold(0),
    m_fLastLevel(0.f),
    m_iSpace(1)
{
    m_pLevelComponent.reset (new LevelComponent());
    addAndMakeVisible(m_pLevelComponent.get());
    setInterceptsMouseClicks(false, false);
}

//==============================================================================
LevelMeter::~LevelMeter()
{
}

//==============================================================================
const float LevelMeter::getDecibelScaled(const float db)
{
    return powf(reverse_linear(db, m_fMindB, m_dMaxdB), 2.f);
    //return reverse_linear(db, m_fMindB, m_dMaxdB);
}

//==============================================================================
void LevelMeter::resized()
{
    mRect.setBounds(m_iSpace, m_iSpace, getWidth() - (2 * m_iSpace), std::max(getHeight() - (2 * m_iSpace), 1));
    m_pLevelComponent->setBounds(mRect);
}

//==============================================================================
void LevelMeter::paint(Graphics& g)
{
    Path indent;
    indent.addRoundedRectangle (0, 0, getWidth(), getHeight(), 1.f);
    g.setColour(Colour (0xff5e6569).withAlpha(0.4f));
    g.fillPath (indent);
    
    // draw border
    g.setColour (juce::Colours::black.withAlpha(0.50f));
    g.drawRoundedRectangle (getLocalBounds().toFloat(), 3.0f, 2.0f);
}

//==============================================================================
float LevelMeter::getDisplayLevel()
{
    float level = getDecibelScaled(m_fLeveldB);
    level = jlimit(0.f, 1.f, level);
    
    if (level >= (m_fLastLevel - 0.02f)) // threshold
    {
        m_fLastLevel = level;
    }
    else
    {
        m_fLastLevel -= 0.02f; // decrement
        level = m_fLastLevel;
    }
    
    return level;
}

//==============================================================================
float LevelMeter::getDisplayPeak()
{
    float peak = getDecibelScaled(m_fPeakLeveldB);
    return jlimit(0.f, 1.f, peak);
}

//==============================================================================
void LevelMeter::setLevel(float level)
{
    if (level < decebelToGain(m_fMindB))
    {
        m_fLeveldB = m_fMindB;
    }
    else
    {
        m_fLeveldB = gainToDecebel(level);
    }
    
    //printf("lvl %f\n", m_fLeveldB);
    
    if (m_fLeveldB > m_fPeakLeveldB)
    {
        m_fPeakLeveldB = m_fLeveldB;
        m_iPeakHold = m_iPeakHoldDuration;
    }
    else if(m_iPeakHold > 0)
    {
        m_iPeakHold--;
    }
    else
    {
        m_fPeakLeveldB = m_fMindB;
    }
    
    repaint();
}

//==============================================================================
void LevelMeter::setPeakLevel(float level)
{
    m_fPeakLeveldB = gainToDecebel(level);
    m_iPeakHold = m_iPeakHoldDuration;
}

//==============================================================================
void LevelMeter::setInverted(bool bInverted)
{
    m_bInverted = bInverted;
    m_pLevelComponent->redrawLevels();
}

//==============================================================================
void LevelMeter::setVertical(bool bVertical)
{
    m_bVertical = bVertical;
    m_pLevelComponent->redrawLevels();
}

//==============================================================================
void LevelMeter::setDbMin(float dbMin)
{
    m_fMindB = dbMin;
    m_pLevelComponent->redrawLevels();
}

//==============================================================================
void LevelMeter::setDbMax(float dbMax)
{
    m_dMaxdB = dbMax;
    m_pLevelComponent->redrawLevels();
}

//==============================================================================

//==============================================================================
StereoMeter::StereoMeter() :
    m_bVertical(false),
    m_bInverted(false)
{
    m_pLevelMeter[0].reset (new LevelMeter(m_bVertical, m_bInverted));
    m_pLevelMeter[1].reset (new LevelMeter(m_bVertical, m_bInverted));
    addAndMakeVisible(m_pLevelMeter[0].get());
    addAndMakeVisible(m_pLevelMeter[1].get());
}

//==============================================================================
StereoMeter::~StereoMeter()
{
}

/*
//==============================================================================
void StereoMeter::paint(Graphics& g)
{
    g.fillAll (Colours::black);
}
*/

//==============================================================================
void StereoMeter::resized()
{
    if(m_bVertical)
    {
        int channelWidth = (getWidth() + m_pLevelMeter[0]->getSpacing()) / 2;
        m_pLevelMeter[0]->setBounds(0, 0, channelWidth, getHeight());
        m_pLevelMeter[1]->setBounds(getWidth() - channelWidth, 0, channelWidth, getHeight());
    }
    else
    {
        int channelHeight = (getHeight() - m_pLevelMeter[0]->getSpacing()) / 2;
        m_pLevelMeter[0]->setBounds(0, 0, getWidth(), channelHeight);
        m_pLevelMeter[1]->setBounds(0, getHeight() - channelHeight, getWidth(), channelHeight);
    }
}

//==============================================================================
void StereoMeter::setLevel(int channel, float level)
{
    jassert(channel >= 0 && channel < numChannels);
    m_pLevelMeter[channel]->setLevel(level);
}

//==============================================================================
void StereoMeter::setInverted(bool bInverted)
{
    m_bInverted = bInverted;
    for (int i = 0; i < numChannels; i++)
    {
        m_pLevelMeter[i]->setInverted(bInverted);
    }
}

//==============================================================================
void StereoMeter::setVertical(bool bVertical)
{
    m_bVertical = bVertical;
    for (int i = 0; i < numChannels; i++)
    {
        m_pLevelMeter[i]->setVertical(bVertical);
    }
}

//==============================================================================
void StereoMeter::setDbMin(float dbMin)
{
    for (int i = 0; i < numChannels; i++)
    {
        m_pLevelMeter[i]->setDbMin(dbMin);
    }
}

//==============================================================================
void StereoMeter::setDbMax(float dbMax)
{
    for (int i = 0; i < numChannels; i++)
    {
        m_pLevelMeter[i]->setDbMax(dbMax);
    }
}


