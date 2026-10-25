#include "PluginProcessor.h"
#include "PluginEditor.h"

KizamiProcessor::KizamiProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    startTimer (500);
}

KizamiProcessor::~KizamiProcessor()
{
    stopTimer();
}

void KizamiProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

bool KizamiProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo();
}

void KizamiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    engine.process (buffer, midi, getPlayHead());
}

juce::AudioProcessorEditor* KizamiProcessor::createEditor()
{
    return new KizamiEditor (*this);
}

void KizamiProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto v = engine.toValueTree (true);
    v.setProperty ("exportLoops", exportLoops, nullptr);
    if (auto xml = v.createXml())
        copyXmlToBinary (*xml, destData);
}

void KizamiProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto v = juce::ValueTree::fromXml (*xml);
        if (v.hasType ("KIZAMI"))
        {
            engine.fromValueTree (v);
            exportLoops = (int) v.getProperty ("exportLoops", 4);
            stateGeneration.fetch_add (1);
        }
    }
}

juce::File KizamiProcessor::presetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
        .getChildFile ("Kizami").getChildFile ("Presets");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KizamiProcessor();
}
