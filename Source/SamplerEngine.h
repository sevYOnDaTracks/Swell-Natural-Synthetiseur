#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>
#include "SwellBank.h"

namespace swell
{
enum class PlayMode { poly = 0, mono = 1, legato = 2 };

struct EngineParams
{
    float attack = 0.002f, decay = 1.0f, sustain = 1.0f, release = 0.35f;
    float cutoff = 20000.0f, resonance = 0.1f;
    float tune = 0.0f;          // semitones
    float velSens = 0.75f;      // 0..1
    float glide = 0.0f;         // seconds (slide time)
    float startOffsetMs = 0.0f; // relative to the automatic start point
    int octave = 0;             // user octave shift
    PlayMode mode = PlayMode::poly;
    bool slideArticulation = false;
};

// Zero-delay-feedback state variable low-pass, stereo.
struct StereoSvf
{
    void reset() { s1[0] = s1[1] = s2[0] = s2[1] = 0.0f; }

    void setup (float cutoffHz, float resonance, double sr)
    {
        const float fc = juce::jlimit (20.0f, (float) (sr * 0.45), cutoffHz);
        g = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
        k = 2.0f - 1.96f * juce::jlimit (0.0f, 1.0f, resonance);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float process (float x, int c)
    {
        const float v3 = x - s2[c];
        const float v1 = a1 * s1[c] + a2 * v3;
        const float v2 = s2[c] + a2 * s1[c] + a3 * v3;
        s1[c] = 2.0f * v1 - s1[c];
        s2[c] = 2.0f * v2 - s2[c];
        return v2;
    }

    float g = 1.0f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0;
    float s1[2] {}, s2[2] {};
};

class SamplerVoice
{
public:
    bool isActive() const { return active; }
    int getNote() const { return note; }
    bool isHeld() const { return held; }
    bool isReleasing() const { return releasing; }
    juce::uint64 getAge() const { return age; }

    void start (const LoadedZone* z, int midiNote, int mappedNote, float fineTune, float velocity, float glideFrom,
                juce::uint64 order, const EngineParams& p, double hostRate, bool isOneShot, float layerGain)
    {
        zone = z;
        note = midiNote;
        mapOffset = mappedNote - midiNote;
        soundTune = fineTune;
        vel = velocity;
        age = order;
        sampleRate = hostRate;
        oneShot = isOneShot;
        held = true;
        sustained = false;
        releasing = false;
        glideOffset = glideFrom;
        fastRelease = false;
        filter.reset();

        const int n = z->info.numFrames;
        const int offset = (int) std::round (p.startOffsetMs * 0.001f * (float) z->info.sampleRate);
        int st = juce::jlimit (0, juce::jmax (0, n - 2), z->playStart + offset);
        if (z->looped()) st = juce::jmin (st, z->loopEnd() - 2);
        pos = (double) st;

        gain = ((1.0f - p.velSens) + p.velSens * velocity * velocity) * layerGain;

        env.setSampleRate (hostRate);
        env.setParameters ({ p.attack, p.decay, p.sustain, p.release });
        env.reset();
        env.noteOn();
        active = true;
    }

    // Legato slide: keep the sample running, glide the pitch to the new note.
    void slideTo (int newNote)
    {
        glideOffset += (float) (note - newNote);
        note = newNote;
        held = true;
        sustained = false;
    }

    void noteOff (bool sustainPedal)
    {
        held = false;
        if (oneShot && ! (zone != nullptr && zone->looped()))
            return; // drums ring out
        if (sustainPedal) { sustained = true; return; }
        beginRelease();
    }

    void pedalUp()
    {
        if (sustained && ! held)
        {
            sustained = false;
            beginRelease();
        }
    }

    void quickRelease()
    {
        fastRelease = true;
        env.setParameters ({ 0.001f, 0.1f, 1.0f, 0.03f });
        env.noteOff();
        releasing = true;
        held = false;
    }

    void kill() { active = false; zone = nullptr; }

    void updateParams (const EngineParams& p)
    {
        if (fastRelease) return;
        env.setParameters ({ p.attack, p.decay, p.sustain, p.release });
    }

    void render (juce::AudioBuffer<float>& out, int start, int numSamples, const EngineParams& p,
                 float pitchSemis, float glideCoef)
    {
        if (! active || zone == nullptr) return;

        const auto& buf = *zone->audio;
        const float* L = buf.getReadPointer (0);
        const float* R = buf.getReadPointer (1);
        const int n = zone->info.numFrames;
        const bool loop = zone->looped();
        const double loopStart = zone->info.loopStart;
        const double loopEnd = zone->loopEnd();
        const double loopLen = zone->info.loopLength;

        float* outL = out.getWritePointer (0, start);
        float* outR = out.getWritePointer (1, start);

        const bool useFilter = p.cutoff < 19500.0f;
        const double baseRatio = (double) zone->info.sampleRate / sampleRate;
        constexpr int sub = 32;

        for (int s0 = 0; s0 < numSamples; s0 += sub)
        {
            const int count = juce::jmin (sub, numSamples - s0);
            glideOffset *= glideCoef;
            if (std::abs (glideOffset) < 0.001f) glideOffset = 0.0f;
            const double semis = (double) (note + mapOffset - zone->info.root) + p.tune + soundTune + pitchSemis + glideOffset;
            const double inc = baseRatio * std::exp2 (semis / 12.0);
            if (useFilter) filter.setup (p.cutoff * (0.6f + 0.4f * vel), p.resonance, sampleRate);

            for (int i = 0; i < count; ++i)
            {
                const int idx = (int) pos;
                const float f = (float) (pos - idx);

                if (! loop && idx + 2 >= n)
                {
                    active = false;
                    zone = nullptr;
                    return;
                }
                const int i0 = wrap (idx - 1, loop, loopStart, loopLen, loopEnd, n);
                const int i1 = wrap (idx, loop, loopStart, loopLen, loopEnd, n);
                const int i2 = wrap (idx + 1, loop, loopStart, loopLen, loopEnd, n);
                const int i3 = wrap (idx + 2, loop, loopStart, loopLen, loopEnd, n);
                float l = hermite (f, L[i0], L[i1], L[i2], L[i3]);
                float r = hermite (f, R[i0], R[i1], R[i2], R[i3]);

                if (useFilter)
                {
                    l = filter.process (l, 0);
                    r = filter.process (r, 1);
                }

                const float e = env.getNextSample() * gain;
                outL[s0 + i] += l * e;
                outR[s0 + i] += r * e;

                pos += inc;
                if (loop && pos >= loopEnd)
                    pos -= loopLen;

                if (! env.isActive())
                {
                    active = false;
                    zone = nullptr;
                    return;
                }
            }
        }
    }

private:
    void beginRelease()
    {
        releasing = true;
        env.noteOff();
    }

    static inline int wrap (int i, bool loop, double ls, double ll, double le, int n)
    {
        if (i < 0) return 0;
        if (loop && i >= (int) le)
            i = (int) ls + (int) std::fmod ((double) (i - (int) ls), ll);
        return i < n ? i : n - 1;
    }

    static inline float hermite (float t, float xm1, float x0, float x1, float x2)
    {
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + (x2 - x0) * 0.5f;
        const float b = w + a;
        return ((((a * t) - b) * t + c) * t + x0);
    }

    const LoadedZone* zone = nullptr;
    juce::ADSR env;
    StereoSvf filter;
    double pos = 0.0, sampleRate = 44100.0;
    float vel = 1.0f, gain = 1.0f, glideOffset = 0.0f, soundTune = 0.0f;
    int note = 60, mapOffset = 0;
    juce::uint64 age = 0;
    bool active = false, held = false, sustained = false, releasing = false, oneShot = false, fastRelease = false;
};

class SamplerEngine
{
public:
    static constexpr int maxVoices = 64;

    SamplerEngine() { heldStack.reserve (128); }

    void prepare (double sr) { sampleRate = sr; allOff (true); }

    void setInstrument (const LoadedInstrument* inst)
    {
        if (inst != instrument)
        {
            allOff (true);
            instrument = inst;
        }
    }

    void noteOn (int noteNumber, float velocity, const EngineParams& p)
    {
        if (instrument == nullptr) return;
        const int v127 = juce::jlimit (1, 127, (int) std::round (velocity * 127.0f));

        const bool monophonic = p.mode != PlayMode::poly && ! instrument->oneShot;
        if (monophonic)
        {
            const bool legatoSlide = p.mode == PlayMode::legato && ! heldStack.empty() && anyHeldVoice();
            pushHeld (noteNumber);
            if (legatoSlide)
            {
                for (auto& v : voices)
                    if (v.isActive() && v.isHeld()) v.slideTo (noteNumber);
                lastNote = noteNumber;
                return;
            }
            for (auto& v : voices)
                if (v.isActive()) v.quickRelease();
        }
        else
        {
            for (auto& v : voices)
                if (v.isActive() && v.getNote() == noteNumber && ! instrument->oneShot)
                    v.quickRelease();
        }

        // Octave correction shifts the key mapping (no extra transposition of the samples)
        const int mapped = juce::jlimit (0, 127, noteNumber + 12 * (p.octave + instrument->octave));
        const LoadedZone* zones[8];
        const int nz = instrument->pickZones (mapped, v127, p.slideArticulation, modWheel > 0.5f,
                                              roundRobin++, zones, 8);
        const float glideFrom = (p.glide > 0.0005f && lastNote >= 0) ? (float) (lastNote - noteNumber) : 0.0f;
        lastNote = noteNumber;

        for (int i = 0; i < nz; ++i)
            findFreeVoice().start (zones[i], noteNumber, mapped, instrument->tune, velocity, glideFrom, ++counter, p, sampleRate,
                                   instrument->oneShot, i == 0 ? 1.0f : 0.8f);
    }

    void noteOff (int noteNumber, const EngineParams& p)
    {
        const bool monophonic = p.mode != PlayMode::poly && instrument != nullptr && ! instrument->oneShot;
        if (monophonic)
        {
            const bool wasTop = ! heldStack.empty() && heldStack.back() == noteNumber;
            removeHeld (noteNumber);
            if (wasTop && ! heldStack.empty())
            {
                // Return to the previous held note with a slide
                const int back = heldStack.back();
                for (auto& v : voices)
                    if (v.isActive() && v.isHeld() && v.getNote() == noteNumber) v.slideTo (back);
                lastNote = back;
                return;
            }
            if (! heldStack.empty())
                return;
        }

        for (auto& v : voices)
            if (v.isActive() && v.isHeld() && v.getNote() == noteNumber)
                v.noteOff (sustainDown);
    }

    void setSustain (bool down)
    {
        sustainDown = down;
        if (! down)
            for (auto& v : voices)
                if (v.isActive()) v.pedalUp();
    }

    void allOff (bool hard)
    {
        for (auto& v : voices)
            if (v.isActive()) { if (hard) v.kill(); else v.quickRelease(); }
        sustainDown = false;
        heldStack.clear();
    }

    void setPitchBend (int value14) { bendSemis = ((float) value14 - 8192.0f) / 8192.0f * 2.0f; }
    void setModWheel (float v) { modWheel = v; }

    int activeVoices() const
    {
        int n = 0;
        for (auto& v : voices) n += v.isActive() ? 1 : 0;
        return n;
    }

    void render (juce::AudioBuffer<float>& out, int start, int numSamples, const EngineParams& p)
    {
        // Vibrato from the mod wheel (instruments without a mod-wheel articulation): 5.5 Hz, +-0.5 st
        const bool wheelIsArticulation = instrument != nullptr && instrument->hasModWheel;
        vibPhase += 2.0 * juce::MathConstants<double>::pi * 5.5 * numSamples / sampleRate;
        if (vibPhase > juce::MathConstants<double>::twoPi) vibPhase -= juce::MathConstants<double>::twoPi;
        const float vib = wheelIsArticulation ? 0.0f : (float) std::sin (vibPhase) * 0.5f * modWheel;

        const float glideCoef = p.glide > 0.0005f ? (float) std::exp (-32.0 / (p.glide * 0.3 * sampleRate)) : 0.0f;

        for (auto& v : voices)
        {
            if (! v.isActive()) continue;
            v.updateParams (p);
            v.render (out, start, numSamples, p, bendSemis + vib, glideCoef);
        }
    }

private:
    bool anyHeldVoice() const
    {
        for (auto& v : voices)
            if (v.isActive() && v.isHeld()) return true;
        return false;
    }

    void pushHeld (int n) { removeHeld (n); heldStack.push_back (n); }
    void removeHeld (int n) { heldStack.erase (std::remove (heldStack.begin(), heldStack.end(), n), heldStack.end()); }

    SamplerVoice& findFreeVoice()
    {
        for (auto& v : voices)
            if (! v.isActive()) return v;

        SamplerVoice* oldest = nullptr;
        for (auto& v : voices)
            if (v.isReleasing() && (oldest == nullptr || v.getAge() < oldest->getAge())) oldest = &v;
        if (oldest == nullptr)
            for (auto& v : voices)
                if (oldest == nullptr || v.getAge() < oldest->getAge()) oldest = &v;
        oldest->kill();
        return *oldest;
    }

    SamplerVoice voices[maxVoices];
    const LoadedInstrument* instrument = nullptr;
    std::vector<int> heldStack;
    double sampleRate = 44100.0, vibPhase = 0.0;
    float bendSemis = 0.0f, modWheel = 0.0f;
    int lastNote = -1;
    unsigned int roundRobin = 0;
    bool sustainDown = false;
    juce::uint64 counter = 0;
};
} // namespace swell
