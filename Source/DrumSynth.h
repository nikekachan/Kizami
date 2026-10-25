#pragma once

#include "Engine.h"

namespace kizami
{

/** Renders a one-shot for a built-in sound (or a pitched/trimmed user sample).
    Runs on the message thread; the audio thread only plays the result back. */
juce::AudioBuffer<float> renderSound (Sound sound, double sampleRate, float pitchSemitones,
                                      float decay, const SampleData* sample);

juce::AudioBuffer<float> renderClick (double sampleRate, bool accent);

} // namespace kizami
