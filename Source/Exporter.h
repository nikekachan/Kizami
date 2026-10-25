#pragma once

#include "Engine.h"

namespace kizami
{

/** Writes the pattern as a Standard MIDI File (one track per drum, channel 10). */
bool writeMidiFile (Engine& engine, int loops, double bpm, const juce::File& file);

/** Writes a 24-bit stereo WAV. */
bool writeWavFile (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::File& file);

/** Mix + one WAV per audible track + MIDI into a folder. Returns the number of files written. */
int exportAll (Engine& engine, int loops, double bpm, const juce::File& folder, juce::String& error);

juce::String safeFileName (const juce::String& s);

} // namespace kizami
