#pragma once
#include "Bank.h"
#include <vector>
#include <cstdint>

namespace yugami
{
/** Compiled lookup tables for the 72 slots of one page (built on the message thread, read by audio). */
struct Tables
{
    float t[kSlots][kTab];
    float v[kSlots][kTab];
    float tLen[kSlots], vLen[kSlots];
};

/** Gross Beat-style time/volume manipulator over a ring buffer of the incoming audio.
    Time envelope: how far back in the buffer to read (bars). Volume envelope: gain. */
class Engine
{
public:
    struct Settings
    {
        float tMix = 1, vMix = 1, attack = 0.002f, release = 0.004f, smooth = 0.004f, out = 1;
        bool limiter = true;
        int baseT = 0, baseV = 0;
    };

    void prepare (double sampleRate);
    void reset();
    /** Momentary slot (-1 = none). restart: the envelope starts from its beginning now. */
    void setHold (bool time, int slot, bool restart);
    /** ppq = song position at the first sample (when known), else an internal clock is used. */
    void process (float* L, float* R, int n, const Tables& tabs, const Settings& s,
                  bool playing, bool hasPpq, double ppq, double bpm);

    // status (read by the editor)
    float phaseT = 0, phaseV = 0, delayBars = 0, peak = 0, level = 1;
    int holdT = -1, holdV = -1;

private:
    static float lookup (const float* t, double p);
    void read (double r, float& l, float& rr) const;

    double sr = 48000;
    std::vector<float> bufL, bufR;
    int64_t mask = 0, w = 0, written = 0;
    double beat = 0, offT = 0, offV = 0, lastPpqEnd = -1;
    bool pendT = false, pendV = false; int pendTSlot = -1, pendVSlot = -1; bool pendTRestart = false, pendVRestart = false;
    bool wasPlaying = false, hasPrev = false, jumping = false;
    double prevR = 0, lastStep = 1, oldR = 0;
    int xf = 0, xfN = 1;
    float vs = 1, lim = 1;
};
} // namespace yugami
