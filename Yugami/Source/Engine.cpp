#include "Engine.h"
#include <cmath>
#include <algorithm>

namespace yugami
{
void Engine::prepare (double sampleRate)
{
    sr = sampleRate;
    // two bars at 30 BPM (16 s) is the longest look-back
    int64_t n = 1;
    while (n < (int64_t) (sr * 17.0)) n <<= 1;
    bufL.assign ((size_t) n, 0.0f);
    bufR.assign ((size_t) n, 0.0f);
    mask = n - 1;
    reset();
}

void Engine::reset()
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    w = 0; written = 0; beat = 0; hasPrev = false; jumping = false; xf = 0; vs = 1; lim = 1; wasPlaying = false; lastPpqEnd = -1;
    holdT = holdV = -1;
}

void Engine::setHold (bool time, int slot, bool restart)
{
    if (time) { pendT = true; pendTSlot = slot; pendTRestart = restart; }
    else      { pendV = true; pendVSlot = slot; pendVRestart = restart; }
}

float Engine::lookup (const float* t, double p)
{
    p -= std::floor (p);
    const double f = p * (kTab - 1);
    const int i = (int) f;
    if (i >= kTab - 1) return t[kTab - 1];
    const float fr = (float) (f - i);
    return t[i] + (t[i + 1] - t[i]) * fr;
}

void Engine::read (double r, float& l, float& rr) const
{
    const double fl = std::floor (r);
    const int64_t ri = (int64_t) fl;
    const float fr = (float) (r - fl);
    const size_t a = (size_t) (ri & mask), b = (size_t) ((ri + 1) & mask);
    l = bufL[a] + (bufL[b] - bufL[a]) * fr;
    rr = bufR[a] + (bufR[b] - bufR[a]) * fr;
}

void Engine::process (float* L, float* R, int n, const Tables& T, const Settings& s,
                      bool playing, bool hasPpq, double ppq, double bpm)
{
    if (bufL.empty()) return;
    bpm = std::clamp (bpm, 30.0, 300.0);
    const double spb = sr * 60.0 / bpm, bar = 4.0 * spb;
    const float aA = s.attack > 0 ? 1.0f - std::exp (-1.0f / (s.attack * (float) sr)) : 1.0f;
    const float aR = s.release > 0 ? 1.0f - std::exp (-1.0f / (s.release * (float) sr)) : 1.0f;

    if (playing && ! wasPlaying) { hasPrev = false; jumping = false; xf = 0; if (! hasPpq) beat = 0; }
    if (playing && hasPpq)
    {
        // a jump in the song position (loop, click on the timeline) is handled by the read-head crossfade
        beat = ppq;
    }
    wasPlaying = playing;

    if (pendT) { pendT = false; holdT = pendTSlot; if (pendTRestart) offT = beat; else offT = 0; if (holdT < 0) offT = 0; }
    if (pendV) { pendV = false; holdV = pendVSlot; if (pendVRestart) offV = beat; else offV = 0; if (holdV < 0) offV = 0; }

    const int bt = std::clamp (s.baseT, 0, kSlots - 1), bv = std::clamp (s.baseV, 0, kSlots - 1);
    const int ti = holdT >= 0 ? holdT : bt, vi = holdV >= 0 ? holdV : bv;
    const float* tt = T.t[ti]; const double tLen = std::max (0.25f, T.tLen[ti]);
    const float* vt = T.v[vi]; const double vLen = std::max (0.25f, T.vLen[vi]);
    const double oT = holdT >= 0 ? offT : 0.0, oV = holdV >= 0 ? offV : 0.0;

    float pk = 0;
    for (int i = 0; i < n; ++i)
    {
        const float xL = L[i], xR = R[i];
        const size_t wi = (size_t) (w & mask);
        bufL[wi] = xL; bufR[wi] = xR;
        if (written < mask) ++written;

        float oL = xL, oR = xR;
        if (playing)
        {
            const double pT = (beat - oT) / tLen, pV = (beat - oV) / vLen;
            phaseT = (float) (pT - std::floor (pT)); phaseV = (float) (pV - std::floor (pV));

            // ---- time
            double d = lookup (tt, pT) * bar;
            d = std::clamp (d, 0.0, std::min ((double) written - 1.0, 2.0 * bar));
            delayBars = (float) (d / bar);
            const double r = (double) w - d;
            double step = hasPrev ? r - prevR : 1.0;
            if (hasPrev && std::abs (step) > 4.0)
            {
                if (! jumping) { if (! (xf > 0 && (double) xf / xfN < 0.5)) oldR = prevR + lastStep; jumping = true; xf = 0; }
                step = lastStep;
            }
            else if (jumping) { jumping = false; xf = 1; xfN = std::max (1, (int) (s.smooth * sr)); }

            float tL, tR;
            if (jumping) { read (oldR, tL, tR); oldR += lastStep; }
            else
            {
                read (r, tL, tR);
                if (xf > 0)
                {
                    float qL, qR; read (oldR, qL, qR); oldR += lastStep;
                    const float k = std::min (1.0f, (float) xf / (float) xfN);
                    tL = qL * (1 - k) + tL * k; tR = qR * (1 - k) + tR * k;
                    if (++xf > xfN) xf = 0;
                }
            }
            lastStep = std::clamp (step, -4.0, 4.0); prevR = r; hasPrev = true;
            oL = xL + (tL - xL) * s.tMix; oR = xR + (tR - xR) * s.tMix;

            // ---- volume
            const float v = lookup (vt, pV), tg = 1.0f - s.vMix * (1.0f - v);
            vs += (tg - vs) * (tg > vs ? aA : aR);
            oL *= vs; oR *= vs;
            beat += 1.0 / spb;
        }
        else
        {
            hasPrev = false;
            vs += (1.0f - vs) * aR;
        }

        oL *= s.out; oR *= s.out;
        if (s.limiter)
        {
            const float a = std::max (std::abs (oL), std::abs (oR));
            if (a * lim > 0.98f) lim = 0.98f / a; else lim += (1.0f - lim) * 0.0002f;
            oL *= lim; oR *= lim;
        }
        L[i] = oL; R[i] = oR;
        pk = std::max (pk, std::max (std::abs (oL), std::abs (oR)));
        ++w;
    }
    peak = std::max (pk, peak * 0.85f);
    level = vs;
    if (! playing) { phaseT = phaseV = 0; delayBars = 0; }
}
} // namespace yugami
