//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "LevelMeter.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"


void LevelComponent::paint(Graphics& g)
{
    LevelMeter* parent = dynamic_cast<LevelMeter*>(getParentComponent());
    jassert(parent);
    if (parent == nullptr) return;
    
    const float level = parent->getDisplayLevel();
    const bool bVertical = parent->getVertical();
    const bool bInverted = parent->getInverted();
    
    const int h = roundToInt(static_cast<float>(getHeight()) * level);
    const int w = roundToInt(static_cast<float>(getWidth()) * level);
    
    // the unlit part: the colour zones shine through faintly
    g.setOpacity (0.16f);
    g.drawImageAt (mImage, 0, 0);
    g.setOpacity (1.0f);
    
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

void LevelComponent::resized()
{
    redrawLevels();
}

void LevelComponent::redrawLevels()
{
    if (getWidth() <= 0 || getHeight() <= 0)   // not laid out yet
        return;
    mImage = Image(Image::ARGB, getWidth(), getHeight(), true);
    Graphics g(mImage);
    drawLevels(g);
}

void LevelComponent::setGrayscale(bool bGrayscale)
{
    if (m_bGrayscale != bGrayscale) {
        m_bGrayscale = bGrayscale;
        redrawLevels();
    }
}

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
    
    auto grey = juce::Colours::grey;
    auto greenLevel = m_bGrayscale ? grey : MeterColours::green;
    auto greenLightLevel = m_bGrayscale ? grey.brighter() : MeterColours::greenLight;
    auto orangeLevel = m_bGrayscale ? grey.brighter().brighter() : MeterColours::orange;
    auto redLevel = MeterColours::red;
    
    if(bVertical)
    {
        if(bInverted)
        {
            g.setColour(greenLevel);
            g.fillRect(0, 0, getWidth(), height20Db);
            
            g.setColour(greenLightLevel);
            g.fillRect(0, height20Db , getWidth(), height6Db - height20Db);
            
            g.setColour(orangeLevel);
            g.fillRect(0, height6Db, getWidth(), height0Db - height6Db);
            
            g.setColour(redLevel);
            g.fillRect(0, height0Db, getWidth(), getHeight() - height0Db);
        }
        else
        {
            g.setColour(greenLevel);
            g.fillRect(0, getHeight() - height20Db, getWidth(), height20Db);
            
            g.setColour(greenLightLevel);
            g.fillRect(0, getHeight() - height6Db , getWidth(), height6Db - height20Db);
            
            g.setColour(orangeLevel);
            g.fillRect(0, getHeight() - height0Db , getWidth(), height0Db - height6Db);
            
            g.setColour(redLevel);
            g.fillRect(0, 0, getWidth(), getHeight() - height0Db);
        }
    }
    else // horizontal
    {
        if(bInverted)
        {
            g.setColour(greenLevel);
            g.fillRect(getWidth() - width20Db, 0, width20Db, getHeight());
            
            g.setColour(greenLightLevel);
            g.fillRect(getWidth() - width6Db, 0, width6Db - width20Db, getHeight());
            
            g.setColour(orangeLevel);
            g.fillRect(getWidth() - width0Db, 0, width0Db - width6Db, getHeight());
            
            g.setColour(redLevel);
            g.fillRect(0, 0, getWidth() - width0Db, getHeight());
        }
        else
        {
            g.setColour(greenLevel);
            g.fillRect(0, 0, width20Db, getHeight());
        
            g.setColour(greenLightLevel);
            g.fillRect(width20Db, 0, width6Db - width20Db, getHeight());
            
            g.setColour(orangeLevel);
            g.fillRect(width6Db, 0, width0Db - width6Db, getHeight());
            
            g.setColour(redLevel);
            g.fillRect(width0Db, 0, getWidth() - width0Db, getHeight());
        }
    }
}

LevelMeter::LevelMeter(bool bVertical, bool bInverted) :
    m_bVertical(bVertical),
    m_bInverted(bInverted),
    m_fMindB(-80.f),
    m_fMaxdB(3.f),
    m_fLeveldB(m_fMindB),
    m_fPeakLeveldB(m_fMindB),
    m_fDecrementdB(1.f),
    m_fLastLeveldB(m_fMindB),
    m_iPeakHoldDuration(50*4),
    m_iPeakHold(0),
    m_iSpace(1)
{
    m_pLevelComponent.reset (new LevelComponent());
    addAndMakeVisible(m_pLevelComponent.get());
    setInterceptsMouseClicks(false, false);
}

LevelMeter::~LevelMeter()
{
}

const float LevelMeter::getDecibelScaled(const float db)
{
    return powf(reverse_linear(db, m_fMindB, m_fMaxdB), 2.f);
}

void LevelMeter::resized()
{
    m_barArea = getLocalBounds();
    if (m_bScale && m_bVertical) {
        m_barArea.removeFromLeft (scaleWidth);
        m_barArea.reduce (0, scaleEndInset);   // room for the top and bottom figures
    }
    mRect = m_barArea.reduced (m_iSpace).withSize (std::max (m_barArea.getWidth() - 2 * m_iSpace, 1),
                                                   std::max (m_barArea.getHeight() - 2 * m_iSpace, 1));
    m_pLevelComponent->setBounds(mRect);
}

void LevelMeter::paint(Graphics& g)
{
    // a recessed slot, as the fader's: dark, with a faint light edge around it
    const auto bar = m_barArea.toFloat();
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (bar.expanded (0.5f), 2.5f, 1.0f);
    
    if (m_bScale && m_bVertical)
        drawScale (g);
}

void LevelMeter::paintOverChildren (Graphics& g)
{
    // the inner shadow that makes the slot look deep: along the top (or the left, lying down)
    // and down the left edge, over lit and unlit parts alike
    const auto inner = mRect.toFloat();
    if (inner.isEmpty())
        return;
    
    const auto shadow = Colours::black.withAlpha (0.35f);
    const auto clear = Colours::transparentBlack;
    const auto depth = jmin (6.0f, (m_bVertical ? inner.getHeight() : inner.getWidth()) * 0.5f);
    
    if (m_bVertical) {
        g.setGradientFill (ColourGradient (shadow, inner.getX(), inner.getY(), clear, inner.getX(), inner.getY() + depth, false));
        g.fillRect (inner.withHeight (depth));
        g.setGradientFill (ColourGradient (shadow.withMultipliedAlpha (0.7f), inner.getX(), inner.getY(), clear, inner.getX() + 3.0f, inner.getY(), false));
        g.fillRect (inner.withWidth (3.0f));
    }
    else {
        g.setGradientFill (ColourGradient (shadow, inner.getX(), inner.getY(), clear, inner.getX() + depth, inner.getY(), false));
        g.fillRect (inner.withWidth (depth));
        g.setGradientFill (ColourGradient (shadow.withMultipliedAlpha (0.7f), inner.getX(), inner.getY(), clear, inner.getX(), inner.getY() + 3.0f, false));
        g.fillRect (inner.withHeight (3.0f));
    }
}

void LevelMeter::drawScale (Graphics& g)
{
    // a tick and the dB figure (without sign, as on a console) at the usual marks; a figure is
    // drawn only where it does not run into the one above
    const auto textColour = findColour (audium::defaultTextColourId);
    g.setFont (AudiumLookAndFeel::withTabularFigures (juce::FontOptions (7.5f)));
    const auto tickRight = static_cast<float> (m_barArea.getX()) - 1.0f;
    const auto figureWidth = tickRight - 4.0f;
    const auto top = static_cast<float> (mRect.getY());
    const auto height = static_cast<float> (mRect.getHeight());
    auto lastFigureY = -100.0f;
    for (const auto mark : { 0.f, -6.f, -12.f, -20.f, -30.f, -40.f, -60.f }) {
        if (mark > m_fMaxdB || mark < m_fMindB)
            continue;
        const auto position = getDecibelScaled (mark);
        const auto tickY = m_bInverted ? top + position * height : top + (1.0f - position) * height;
        const auto unity = mark == 0.f;
        g.setColour (textColour.withAlpha (unity ? 0.7f : 0.35f));
        g.fillRect (tickRight - 3.0f, tickY - 0.5f, 3.0f, 1.0f);
        if (tickY - lastFigureY >= 9.0f) {
            g.drawText (String (static_cast<int> (std::abs (mark))),
                        Rectangle<float> (0.0f, tickY - 5.0f, figureWidth, 10.0f),
                        Justification::centredRight, false);
            lastFigureY = tickY;
        }
    }
}

void LevelMeter::setScaleShown (bool shown)
{
    if (m_bScale == shown)
        return;
    m_bScale = shown;
    if (! getLocalBounds().isEmpty())
        resized();
    repaint();
}

float LevelMeter::getDisplayLevel()
{
    float level = m_fLeveldB;
    level = jlimit(m_fMindB, m_fMaxdB, level);
    
    if (level > m_fLastLeveldB) {
        m_fLastLeveldB = level;
    }
    else {
        m_fLastLeveldB -= m_fDecrementdB; // decrement
        level = m_fLastLeveldB;
    }
    
    return getDecibelScaled(level);
}

float LevelMeter::getDisplayPeak()
{
    float peak = getDecibelScaled(m_fPeakLeveldB);
    return jlimit(0.f, 1.f, peak);
}

void LevelMeter::setLevel(float level)
{
    if (level < juce::Decibels::decibelsToGain(m_fMindB))
    {
        m_fLeveldB = m_fMindB;
    }
    else
    {
        m_fLeveldB = gainToDecebel(level);
    }
    
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

void LevelMeter::setInverted(bool bInverted)
{
    m_bInverted = bInverted;
    m_pLevelComponent->redrawLevels();
}

void LevelMeter::setVertical(bool bVertical)
{
    m_bVertical = bVertical;
    m_pLevelComponent->redrawLevels();
}

void LevelMeter::setDbMin(float dbMin)
{
    m_fMindB = dbMin;
    m_pLevelComponent->redrawLevels();
}

void LevelMeter::setDbMax(float dbMax)
{
    m_fMaxdB = dbMax;
    m_pLevelComponent->redrawLevels();
}

void LevelMeter::setGrayscale(bool bGrayscale)
{
    m_pLevelComponent->setGrayscale(bGrayscale);
}


StereoMeter::StereoMeter() :
    m_bVertical(false),
    m_bInverted(false)
{
    m_pLevelMeter[0].reset (new LevelMeter(m_bVertical, m_bInverted));
    m_pLevelMeter[1].reset (new LevelMeter(m_bVertical, m_bInverted));
    addAndMakeVisible(m_pLevelMeter[0].get());
    addAndMakeVisible(m_pLevelMeter[1].get());
}

StereoMeter::~StereoMeter()
{
}

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

void StereoMeter::setLevel(int channel, float level)
{
    jassert(channel >= 0 && channel < numChannels);
    m_pLevelMeter[channel]->setLevel(level);
}

void StereoMeter::setInverted(bool bInverted)
{
    m_bInverted = bInverted;
    for (int i = 0; i < numChannels; i++)
    {
        m_pLevelMeter[i]->setInverted(bInverted);
    }
}

void StereoMeter::setVertical(bool bVertical)
{
    m_bVertical = bVertical;
    for (int i = 0; i < numChannels; i++)
    {
        m_pLevelMeter[i]->setVertical(bVertical);
    }
}

void StereoMeter::setDbMin(float dbMin)
{
    for (int i = 0; i < numChannels; i++)
    {
        m_pLevelMeter[i]->setDbMin(dbMin);
    }
}

void StereoMeter::setDbMax(float dbMax)
{
    for (int i = 0; i < numChannels; i++)
    {
        m_pLevelMeter[i]->setDbMax(dbMax);
    }
}


