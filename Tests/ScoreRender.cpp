// Renders the promo soundtrack with the Swell engine itself (one processor per track).
// Usage: SwellScoreRender <out.wav> <events.json> [soul|afro]
#include <juce_audio_utils/juce_audio_utils.h>
#include <CoreFoundation/CoreFoundation.h>
#include "../Source/PluginProcessor.h"
using namespace juce;

static void pump (int ms) { CFRunLoopRunInMode (kCFRunLoopDefaultMode, ms / 1000.0, false); }

static double bpm = 90.0, beat = 60.0 / bpm, bar = 4.0 * beat;
static constexpr double sr = 48000.0;
static constexpr int bars = 12;
static double totalSec = bars * bar + 3.0;

struct Note { double t, len; int note; float vel; };
struct Track
{
    String sound;
    float gain, reverb;
    std::vector<Note> notes;
    std::function<float (double)> cutoff; // Hz over time, or null
};

static double T (int barIndex, double beatPos) { return barIndex * bar + beatPos * beat; } // barIndex from 0

// Afrobeats, 104 BPM: Am9 - Fmaj7 - Cadd9 - G6. Same arrangement map as the soul version:
// piano bars 1-6 and 9-12 (filter sweep on 5-6), drums from bar 3, guitar riff bar 7,
// brass stabs bar 8, drum break bar 9, bass + choir to the end.
static std::vector<Track> buildAfro()
{
    std::vector<Track> tracks;
    Random rnd (7);
    auto hv = [&] (float v) { return jlimit (0.05f, 1.0f, v + (rnd.nextFloat() - 0.5f) * 0.08f); };
    const std::vector<std::vector<int>> chords = { { 60, 64, 67, 71 }, { 60, 64, 65, 69 }, { 62, 64, 67, 72 }, { 59, 62, 64, 67 } };
    const int roots[] = { 45, 41, 48, 43 };
    const int bassRoots[] = { 33, 41, 36, 43 };

    Track piano { "GrandPiano Live!", 0.9f, 0.18f, {}, nullptr };
    const double stabs[] = { 0.0, 0.75, 1.5, 2.5, 3.25 };
    for (int b = 0; b < bars; ++b)
    {
        if (b == 6 || b == 7) continue;
        const auto& ch = chords[(size_t) (b % 4)];
        if (b == bars - 1)
        {
            piano.notes.push_back ({ T (b, 0.0), 3.6 * beat, roots[b % 4], 0.7f });
            for (size_t i = 0; i < ch.size(); ++i) piano.notes.push_back ({ T (b, 0.0) + 0.015 * i, 3.6 * beat, ch[i], 0.62f });
            continue;
        }
        piano.notes.push_back ({ T (b, 0.0), 0.7 * beat, roots[b % 4], hv (0.7f) });
        piano.notes.push_back ({ T (b, 2.0), 0.7 * beat, roots[b % 4], hv (0.6f) });
        for (double p : stabs)
            for (size_t i = 0; i < ch.size(); ++i)
                piano.notes.push_back ({ T (b, p) + 0.008 * i, 0.32 * beat, ch[i], hv (p == 0.0 ? 0.66f : 0.52f) });
    }
    piano.cutoff = [] (double t)
    {
        const double a = T (4, 0.0), z = T (5, 3.0);
        if (t < a || t > z + 0.4) return 20000.0f;
        const double x = jlimit (0.0, 1.0, (t - a) / (z - a));
        return (float) (300.0 * std::pow (20000.0 / 300.0, x * x));
    };
    tracks.push_back (piano);

    // Drums: tresillo kick, rim/clap on 2 and 4, 16th shaker with accents, congas
    Track drums { "StandardKit1", 0.85f, 0.06f, {}, nullptr };
    for (int b = 2; b < bars; ++b)
    {
        auto shaker = [&] (float level)
        {
            for (int s16 = 0; s16 < 16; ++s16)
                drums.notes.push_back ({ T (b, s16 * 0.25), 0.1, 70, hv ((s16 % 4 == 2 ? 0.55f : 0.32f) * level) });
        };
        if (b == 6 || b == 7)
        {
            shaker (1.0f);
            for (double p : { 0.5, 1.25, 2.5, 3.25 }) drums.notes.push_back ({ T (b, p), 0.2, 63, hv (0.55f) });
            drums.notes.push_back ({ T (b, 3.0), 0.2, 39, hv (0.6f) });
            continue;
        }
        if (b == 8)
        {
            drums.notes.push_back ({ T (b, 0.0), 0.5, 49, 0.9f });
            drums.notes.push_back ({ T (b, 0.0), 0.3, 36, 0.95f });
            const int fill[] = { 63, 63, 64, 62, 63, 64, 64, 62, 38, 47, 45, 43, 38, 38, 41, 43 };
            for (int i = 0; i < 16; ++i)
                drums.notes.push_back ({ T (b, 1.0 + i * 0.1875), 0.15, fill[i], hv (0.55f + 0.025f * i) });
            continue;
        }
        for (double p : { 0.0, 0.75, 1.5, 2.0, 2.75, 3.5 }) drums.notes.push_back ({ T (b, p), 0.3, 36, hv (p == 0.0 || p == 2.0 ? 0.95f : 0.72f) });
        drums.notes.push_back ({ T (b, 1.0), 0.2, 37, hv (0.7f) });
        drums.notes.push_back ({ T (b, 3.0), 0.2, 39, hv (0.8f) });
        drums.notes.push_back ({ T (b, 3.0), 0.2, 37, hv (0.6f) });
        shaker (1.0f);
        for (double p : { 0.5, 1.25, 1.75, 2.5, 3.25, 3.75 }) drums.notes.push_back ({ T (b, p), 0.2, p == 1.75 || p == 3.75 ? 64 : 63, hv (0.5f) });
        if (b == 9 || b == bars - 1) drums.notes.push_back ({ T (b, 0.0), 0.5, 49, 0.8f });
    }
    tracks.push_back (drums);

    // Highlife guitar riff (bar 7) over Cadd9
    Track guitar { "NylonGuitar S.Art!", 1.7f, 0.18f, {}, nullptr };
    {
        const int riff[] = { 72, 67, 69, 72, 74, 72, 69, 67, 64, 67, 69, 67, 64, 62, 64, 67 };
        for (int i = 0; i < 16; ++i)
            if (i % 4 != 3) guitar.notes.push_back ({ T (6, i * 0.25), 0.22, riff[i] - 12, hv (i % 2 ? 0.55f : 0.7f) });
    }
    tracks.push_back (guitar);

    // Brass stabs (bar 8) over G6
    Track brass { "HyperBrass Live!", 0.42f, 0.18f, {}, nullptr };
    for (double p : { 0.0, 0.75, 1.5, 2.5, 3.0 })
        for (int n : { 59, 62, 64, 67 })
            brass.notes.push_back ({ T (7, p), p == 3.0 ? 0.9 * beat : 0.3 * beat, n, hv (0.85f) });
    tracks.push_back (brass);

    // Round bass from bar 3 (rests under the guitar/brass bars)
    Track bass { "ElectricBass Cool!", 0.95f, 0.0f, {}, nullptr };
    for (int b = 2; b < bars; ++b)
    {
        if (b == 6 || b == 7 || b == 8) continue;
        const int r = bassRoots[b % 4];
        if (b == bars - 1) { bass.notes.push_back ({ T (b, 0.0), 3.0 * beat, r, 0.85f }); continue; }
        bass.notes.push_back ({ T (b, 0.0), 0.6 * beat, r, hv (0.9f) });
        bass.notes.push_back ({ T (b, 0.75), 0.4 * beat, r, hv (0.7f) });
        bass.notes.push_back ({ T (b, 1.5), 0.4 * beat, r + 7, hv (0.75f) });
        bass.notes.push_back ({ T (b, 2.5), 0.6 * beat, r + 12, hv (0.7f) });
        bass.notes.push_back ({ T (b, 3.25), 0.5 * beat, r + 10, hv (0.65f) });
    }
    tracks.push_back (bass);

    Track pad { "GospelVoices Live!", 0.3f, 0.35f, {}, nullptr };
    for (int b = 9; b < bars; ++b)
        for (int n : chords[(size_t) (b % 4)])
            pad.notes.push_back ({ T (b, 0.0), 3.9 * beat, n, 0.55f });
    tracks.push_back (pad);
    return tracks;
}

// Sound tour: one solo phrase per sound, one bar each at 100 BPM, after a one-bar intro.
struct TourSlot { const char* sound; const char* category; float gain, reverb; std::vector<Note> notes; };
static std::vector<TourSlot> buildTour()
{
    auto P = [] (std::initializer_list<std::tuple<double, double, int, float>> l)
    {
        std::vector<Note> v;
        for (auto& [b, len, n, vel] : l) v.push_back ({ b * beat, len * beat, n, vel });
        return v;
    };
    auto chord = [] (double b, double len, std::initializer_list<int> ns, float vel, double strum = 0.012)
    {
        std::vector<Note> v; int i = 0;
        for (int n : ns) v.push_back ({ b * beat + strum * i++, len * beat, n, vel });
        return v;
    };
    auto cat = [] (std::vector<Note> a, const std::vector<Note>& b) { a.insert (a.end(), b.begin(), b.end()); return a; };
    std::vector<TourSlot> t;
    t.push_back ({ "GrandPiano Live!", "Piano", 1.0f, 0.2f,
        cat (P ({ { 0, 3.6, 48, 0.7f }, { 0.5, 3.1, 52, 0.55f }, { 1.0, 2.6, 55, 0.6f }, { 1.5, 2.1, 59, 0.6f }, { 2.0, 1.6, 62, 0.62f }, { 2.5, 1.1, 64, 0.65f } }),
             chord (3.0, 0.9, { 67, 71, 74 }, 0.5f)) });
    t.push_back ({ "SuitcaseEP", "E. Piano", 1.0f, 0.2f,
        cat (chord (0.0, 1.4, { 57, 60, 64, 67, 71 }, 0.7f), chord (2.0, 1.8, { 50, 53, 57, 60, 64 }, 0.65f)) });
    t.push_back ({ "Strings", "Strings", 1.0f, 0.25f, chord (0.0, 3.8, { 41, 53, 57, 60, 64 }, 0.7f, 0.0) });
    t.push_back ({ "NylonGuitar S.Art!", "Guitar", 1.6f, 0.2f,
        P ({ { 0, 1.5, 45, 0.7f }, { 0.5, 1.2, 52, 0.6f }, { 1.0, 1.2, 57, 0.6f }, { 1.5, 1.2, 60, 0.6f }, { 2.0, 1.2, 64, 0.65f }, { 2.5, 1.0, 60, 0.55f }, { 3.0, 0.9, 57, 0.55f } }) });
    t.push_back ({ "ElectricBass Cool!", "Bass", 1.1f, 0.0f,
        P ({ { 0, 0.9, 45, 0.85f }, { 1.0, 0.45, 45, 0.6f }, { 1.5, 0.45, 57, 0.7f }, { 2.0, 0.9, 55, 0.8f }, { 3.0, 0.45, 52, 0.7f }, { 3.5, 0.45, 50, 0.7f } }) });
    t.push_back ({ "JazzTenor Sweet!", "Saxophone", 1.0f, 0.22f,
        P ({ { 0, 0.45, 64, 0.75f }, { 0.5, 0.45, 67, 0.75f }, { 1.0, 0.45, 69, 0.8f }, { 1.5, 0.9, 72, 0.85f }, { 2.5, 0.45, 71, 0.7f }, { 3.0, 0.9, 69, 0.75f } }) });
    t.push_back ({ "Flute Sweet!", "Flute & Woodwind", 1.0f, 0.25f,
        P ({ { 0, 0.9, 76, 0.7f }, { 1.0, 0.45, 79, 0.7f }, { 1.5, 0.45, 81, 0.75f }, { 2.0, 0.45, 79, 0.7f }, { 2.5, 1.3, 76, 0.7f } }) });
    t.push_back ({ "JazzOrgan Cool!", "Organ", 0.9f, 0.2f,
        cat (chord (0.0, 0.8, { 48, 52, 58, 62, 64 }, 0.75f, 0.0), cat (chord (1.5, 0.4, { 48, 52, 58, 62, 64 }, 0.7f, 0.0), chord (2.0, 1.8, { 53, 57, 60, 63, 67 }, 0.75f, 0.0))) });
    t.push_back ({ "SoloTrumpet", "Trumpet", 1.0f, 0.22f,
        P ({ { 0, 0.45, 67, 0.8f }, { 0.5, 0.45, 69, 0.8f }, { 1.0, 0.9, 72, 0.85f }, { 2.0, 0.45, 74, 0.85f }, { 2.5, 1.3, 72, 0.8f } }) });
    t.push_back ({ "HyperBrass Live!", "Brass", 0.6f, 0.2f,
        cat (chord (0.0, 0.35, { 55, 60, 64, 67 }, 0.9f, 0.0), cat (chord (0.75, 0.35, { 55, 60, 64, 67 }, 0.85f, 0.0), chord (1.5, 2.2, { 57, 62, 65, 69 }, 0.9f, 0.0))) });
    t.push_back ({ "GospelVoices Live!", "Choir & Pad", 0.8f, 0.3f, chord (0.0, 3.8, { 53, 57, 60, 64, 67 }, 0.7f, 0.0) });
    t.push_back ({ "FunkyLead", "Synth & FX", 0.8f, 0.2f,
        P ({ { 0, 0.4, 69, 0.8f }, { 0.5, 0.4, 72, 0.8f }, { 0.75, 0.4, 74, 0.75f }, { 1.0, 0.9, 76, 0.85f }, { 2.0, 0.4, 74, 0.75f }, { 2.5, 0.4, 72, 0.75f }, { 3.0, 0.9, 69, 0.8f } }) });
    std::vector<Note> kit;
    for (int e = 0; e < 8; ++e) kit.push_back ({ e * 0.5 * beat, 0.2 * beat, 42, e % 2 ? 0.45f : 0.65f });
    for (double b : { 0.0, 1.5, 2.0 }) kit.push_back ({ b * beat, 0.3 * beat, 36, 0.95f });
    for (double b : { 1.0, 3.0 }) kit.push_back ({ b * beat, 0.3 * beat, 38, 0.9f });
    kit.push_back ({ 3.5 * beat, 0.3 * beat, 38, 0.7f });
    kit.push_back ({ 3.75 * beat, 0.3 * beat, 38, 0.8f });
    t.push_back ({ "StandardKit1", "Perc & Drum Kit", 0.9f, 0.08f, kit });
    return t;
}

static int renderTour (const File& outWav, const File& outJson)
{
    bpm = 100.0; beat = 60.0 / bpm; bar = 4.0 * beat;
    auto slots = buildTour();
    const double intro = bar;
    totalSec = intro + bar * (double) slots.size() + 4.0;
    const int total = (int) (totalSec * sr);
    AudioBuffer<float> mix (2, total);
    mix.clear();
    Array<var> jsSlots;
    for (size_t k = 0; k < slots.size(); ++k)
    {
        auto& sl = slots[k];
        const double t0 = intro + bar * (double) k;
        auto proc = std::make_unique<SwellProcessor>();
        const auto& bank = proc->getBank();
        int idx = -1;
        for (size_t i = 0; i < bank.sounds.size(); ++i) if (bank.sounds[i].name == sl.sound) idx = (int) i;
        if (idx < 0) { std::cerr << "missing " << sl.sound << "\n"; return 1; }
        proc->selectSound (idx);
        for (int i = 0; i < 600; ++i)
        {
            pump (10);
            auto inst = proc->getInstrument();
            if (! proc->isLoading() && inst != nullptr && inst->soundIndex == idx) break;
        }
        auto setP = [&] (const char* id, float v) { auto* p = proc->apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        setP ("reverb", sl.reverb);
        setP ("chorus", 0.0f);
        proc->setPlayConfigDetails (0, 2, sr, 256);
        proc->prepareToPlay (sr, 256);

        struct Ev { int sample; bool on; int note; float vel; };
        std::vector<Ev> evs;
        for (auto& n : sl.notes)
        {
            evs.push_back ({ (int) ((t0 + n.t) * sr), true, n.note, n.vel });
            evs.push_back ({ (int) ((t0 + n.t + n.len) * sr), false, n.note, 0.0f });
        }
        std::sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b) { return a.sample != b.sample ? a.sample < b.sample : (! a.on && b.on); });
        // Render the slot into its own buffer (until 0.9 s into the next slot), level it, then mix with a fade
        const int s0 = (int) (t0 * sr), s1 = jmin (total, (int) ((t0 + bar + 0.9) * sr));
        AudioBuffer<float> slotBuf (2, s1 - s0);
        slotBuf.clear();
        AudioBuffer<float> blk (2, 256);
        size_t e = 0;
        for (int pos = s0; pos < s1; pos += 256)
        {
            const int n = jmin (256, s1 - pos);
            MidiBuffer midi;
            while (e < evs.size() && evs[e].sample < pos + n)
            {
                midi.addEvent (evs[e].on ? MidiMessage::noteOn (1, evs[e].note, evs[e].vel) : MidiMessage::noteOff (1, evs[e].note), jmax (0, evs[e].sample - pos));
                ++e;
            }
            blk.setSize (2, n, false, false, true);
            proc->processBlock (blk, midi);
            for (int c = 0; c < 2; ++c) slotBuf.copyFrom (c, pos - s0, blk, c, 0, n);
        }
        const int barLen = jmin (slotBuf.getNumSamples(), (int) (bar * sr));
        const float rms = 0.5f * (slotBuf.getRMSLevel (0, 0, barLen) + slotBuf.getRMSLevel (1, 0, barLen));
        const float target = 0.12f; // ~ -18.4 dBFS RMS per solo
        const float lvl = rms > 1e-6f ? jmin (8.0f, target / rms) : 1.0f;
        std::cout << "  " << sl.sound << " rms " << Decibels::gainToDecibels (rms) << " dB, gain x" << lvl << "\n";
        const bool last = k + 1 == slots.size();
        const double fadeStart = bar + 0.3;
        for (int i = 0; i < slotBuf.getNumSamples(); ++i)
        {
            const double tt = i / sr;
            const float g = (last || tt < fadeStart) ? 1.0f : (float) jmax (0.0, 1.0 - (tt - fadeStart) / 0.6);
            mix.addSample (0, s0 + i, slotBuf.getSample (0, i) * lvl * g);
            mix.addSample (1, s0 + i, slotBuf.getSample (1, i) * lvl * g);
        }
        proc->releaseResources();
        auto* o = new DynamicObject();
        o->setProperty ("start", t0); o->setProperty ("sound", sl.sound); o->setProperty ("category", sl.category);
        Array<var> ns;
        for (auto& n : sl.notes) { auto* x = new DynamicObject(); x->setProperty ("t", t0 + n.t); x->setProperty ("len", n.len); x->setProperty ("note", n.note); ns.add (var (x)); }
        o->setProperty ("notes", ns);
        jsSlots.add (var (o));
        std::cout << "slot " << (int) k << " " << sl.sound << "\n";
    }
    for (int c = 0; c < 2; ++c) { float* d = mix.getWritePointer (c); for (int i = 0; i < total; ++i) d[i] = std::tanh (d[i] * 0.9f); }
    const float peak = mix.getMagnitude (0, total);
    if (peak > 0) mix.applyGain (0.891f / peak);
    outWav.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (outWav), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (mix, 0, total);
    w.reset();
    auto* root = new DynamicObject();
    root->setProperty ("bpm", bpm); root->setProperty ("bar", bar); root->setProperty ("intro", intro); root->setProperty ("slots", jsSlots);
    outJson.replaceWithText (JSON::toString (var (root)));
    std::cout << "wrote " << outWav.getFullPathName() << "\n";
    return 0;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outWav (argc > 1 ? argv[1] : "/tmp/swell_promo.wav");
    const File outJson (argc > 2 ? argv[2] : "/tmp/swell_promo_events.json");
    if (argc > 3 && String (argv[3]) == "tour")
        return renderTour (outWav, outJson);
    const bool afroStyle = argc > 3 && String (argv[3]) == "afro";
    if (afroStyle)
    {
        bpm = 104.0; beat = 60.0 / bpm; bar = 4.0 * beat; totalSec = bars * bar + 3.0;
    }

    // Neo-soul progression, one chord per bar: Dm9, G13, Cmaj9, A7(b13)
    const std::vector<std::vector<int>> chords = { { 53, 57, 60, 64 }, { 53, 57, 59, 64 }, { 52, 55, 59, 62 }, { 55, 61, 65, 69 } };
    const int roots[] = { 50, 43, 48, 45 };
    const int bassRoots[] = { 38, 43, 36, 33 };

    std::vector<Track> tracks;
    Random rnd (42);
    auto hv = [&] (float v) { return jlimit (0.05f, 1.0f, v + (rnd.nextFloat() - 0.5f) * 0.08f); };

    // Piano: bars 1-6 and 9-12 (index 0-5, 8-11)
    Track piano { "GrandPiano Live!", 0.95f, 0.25f, {}, nullptr };
    for (int b = 0; b < bars; ++b)
    {
        if (b == 6 || b == 7) continue;
        const auto& ch = chords[(size_t) (b % 4)];
        const bool last = b == bars - 1;
        piano.notes.push_back ({ T (b, 0.0), last ? 3.6 * beat : 1.4 * beat, roots[b % 4], hv (0.7f) });
        for (size_t i = 0; i < ch.size(); ++i)
            piano.notes.push_back ({ T (b, 0.0) + 0.018 * (double) i, last ? 3.6 * beat : 1.4 * beat, ch[i], hv (0.62f) });
        if (! last)
            for (size_t i = 0; i < ch.size(); ++i)
                piano.notes.push_back ({ T (b, 2.5) + 0.012 * (double) i, 1.3 * beat, ch[i], hv (0.5f) });
    }
    // Filter sweep on bars 5-6 (index 4-5): 300 Hz -> open
    piano.cutoff = [] (double t)
    {
        const double a = T (4, 0.0), z = T (5, 3.0);
        if (t < a || t > z + 0.4) return 20000.0f;
        const double x = jlimit (0.0, 1.0, (t - a) / (z - a));
        return (float) (300.0 * std::pow (20000.0 / 300.0, x * x));
    };
    tracks.push_back (piano);

    // Drums (StandardKit1, GM map) from bar 3, break on bar 9
    Track drums { "StandardKit1", 0.85f, 0.08f, {}, nullptr };
    for (int b = 2; b < bars; ++b)
    {
        if (b == 6 || b == 7) // light pattern under guitar/brass
        {
            for (int e = 0; e < 8; ++e) drums.notes.push_back ({ T (b, e * 0.5 + (e % 2 ? 0.08 : 0.0)), 0.2, 42, hv (e % 2 ? 0.35f : 0.5f) });
            drums.notes.push_back ({ T (b, 3.0), 0.3, 38, hv (0.55f) });
            continue;
        }
        if (b == 8) // drum break
        {
            drums.notes.push_back ({ T (b, 0.0), 0.5, 49, 0.9f });
            drums.notes.push_back ({ T (b, 0.0), 0.3, 36, 0.95f });
            const int fill[] = { 38, 38, 47, 47, 45, 45, 43, 43, 38, 41, 43, 45 };
            for (int i = 0; i < 12; ++i)
                drums.notes.push_back ({ T (b, 1.0 + i * 0.25), 0.2, fill[i], hv (0.55f + 0.03f * i) });
            continue;
        }
        drums.notes.push_back ({ T (b, 0.0), 0.3, 36, hv (0.95f) });
        drums.notes.push_back ({ T (b, 1.75), 0.3, 36, hv (0.7f) });
        drums.notes.push_back ({ T (b, 2.5), 0.3, 36, hv (0.85f) });
        drums.notes.push_back ({ T (b, 1.0), 0.3, 38, hv (0.85f) });
        drums.notes.push_back ({ T (b, 3.0), 0.3, 38, hv (0.9f) });
        for (int e = 0; e < 8; ++e)
            drums.notes.push_back ({ T (b, e * 0.5 + (e % 2 ? 0.08 : 0.0)), 0.2, e == 7 ? 46 : 42, hv (e % 2 ? 0.4f : 0.6f) });
        if (b == 9) drums.notes.push_back ({ T (b, 0.0), 0.5, 49, 0.8f });
        if (b == bars - 1) { drums.notes.push_back ({ T (b, 0.0), 0.5, 49, 0.9f }); }
    }
    tracks.push_back (drums);

    // Showcase: guitar arpeggio (bar 7), brass stabs (bar 8)
    Track guitar { "NylonGuitar S.Art!", 1.7f, 0.22f, {}, nullptr };
    {
        const int arp[] = { 48, 55, 59, 62, 64, 62, 59, 55 };
        for (int i = 0; i < 8; ++i) guitar.notes.push_back ({ T (6, i * 0.5), 1.2, arp[i], hv (0.6f) });
    }
    tracks.push_back (guitar);

    Track brass { "HyperBrass Live!", 0.42f, 0.2f, {}, nullptr };
    for (double p : { 0.0, 1.5, 2.5 })
        for (int n : { 55, 61, 64, 69 })
            brass.notes.push_back ({ T (7, p), p == 2.5 ? 1.2 * beat : 0.35 * beat, n, hv (0.85f) });
    tracks.push_back (brass);

    // Bass from bar 10
    Track bass { "ElectricBass Cool!", 0.9f, 0.0f, {}, nullptr };
    for (int b = 9; b < bars; ++b)
    {
        const int r = bassRoots[b % 4];
        if (b == bars - 1) { bass.notes.push_back ({ T (b, 0.0), 3.0 * beat, r, 0.85f }); continue; }
        bass.notes.push_back ({ T (b, 0.0), 1.2 * beat, r, hv (0.85f) });
        bass.notes.push_back ({ T (b, 1.75), 0.5 * beat, r + 12, hv (0.6f) });
        bass.notes.push_back ({ T (b, 2.5), 1.2 * beat, r, hv (0.8f) });
        bass.notes.push_back ({ T (b, 3.5), 0.45 * beat, r + 7, hv (0.65f) });
    }
    tracks.push_back (bass);

    // Choir pad under the last three bars
    Track pad { "GospelVoices Live!", 0.32f, 0.35f, {}, nullptr };
    for (int b = 9; b < bars; ++b)
        for (int n : chords[(size_t) (b % 4)])
            pad.notes.push_back ({ T (b, 0.0), b == bars - 1 ? 3.8 * beat : 3.9 * beat, n, 0.55f });
    tracks.push_back (pad);
    if (afroStyle) tracks = buildAfro();

    const int total = (int) (totalSec * sr);
    AudioBuffer<float> mix (2, total);
    mix.clear();

    for (auto& tr : tracks)
    {
        auto proc = std::make_unique<SwellProcessor>();
        const int idx = [&] { for (size_t i = 0; i < proc->getBank().sounds.size(); ++i) if (proc->getBank().sounds[i].name == tr.sound) return (int) i; return -1; }();
        if (idx < 0) { std::cerr << "missing sound " << tr.sound << "\n"; return 1; }
        proc->selectSound (idx);
        for (int i = 0; i < 600; ++i)
        {
            pump (10);
            auto inst = proc->getInstrument();
            if (! proc->isLoading() && inst != nullptr && inst->soundIndex == idx) break;
        }
        auto setP = [&] (const char* id, float v) { auto* p = proc->apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        setP ("reverb", tr.reverb);
        setP ("chorus", 0.0f);
        setP ("reso", tr.cutoff ? 0.35f : 0.1f);
        proc->setPlayConfigDetails (0, 2, sr, 256);
        proc->prepareToPlay (sr, 256);

        struct Ev { int sample; bool on; int note; float vel; };
        std::vector<Ev> evs;
        for (auto& n : tr.notes)
        {
            evs.push_back ({ (int) (n.t * sr), true, n.note, n.vel });
            evs.push_back ({ (int) ((n.t + n.len) * sr), false, n.note, 0.0f });
        }
        std::sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b) { return a.sample != b.sample ? a.sample < b.sample : (! a.on && b.on); });

        AudioBuffer<float> blk (2, 256);
        size_t e = 0;
        for (int pos = 0; pos < total; pos += 256)
        {
            const int n = jmin (256, total - pos);
            if (tr.cutoff) setP ("cutoff", tr.cutoff (pos / sr));
            MidiBuffer midi;
            while (e < evs.size() && evs[e].sample < pos + n)
            {
                const int off = jmax (0, evs[e].sample - pos);
                midi.addEvent (evs[e].on ? MidiMessage::noteOn (1, evs[e].note, evs[e].vel) : MidiMessage::noteOff (1, evs[e].note), off);
                ++e;
            }
            blk.setSize (2, n, false, false, true);
            proc->processBlock (blk, midi);
            for (int c = 0; c < 2; ++c) mix.addFrom (c, pos, blk, c, 0, n, tr.gain);
        }
        std::cout << "rendered " << tr.sound << " (" << tr.notes.size() << " notes)\n";
        proc->releaseResources();
    }

    // Master: soft limit and normalise to -1 dBFS
    for (int c = 0; c < 2; ++c)
    {
        float* d = mix.getWritePointer (c);
        for (int i = 0; i < total; ++i) d[i] = std::tanh (d[i] * 0.9f);
    }
    const float peak = mix.getMagnitude (0, total);
    if (peak > 0) mix.applyGain (0.891f / peak);
    const int fadeLen = (int) (1.5 * sr);
    mix.applyGainRamp (total - fadeLen, fadeLen, 1.0f, 0.0f);

    outWav.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (outWav), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (mix, 0, total);
    w.reset();

    // Events for the video: piano key presses and cutoff curve
    Array<var> keys;
    for (auto& n : tracks[0].notes)
    {
        auto* o = new DynamicObject();
        o->setProperty ("t", n.t); o->setProperty ("len", n.len); o->setProperty ("note", n.note);
        keys.add (var (o));
    }
    auto* root = new DynamicObject();
    root->setProperty ("bpm", bpm);
    root->setProperty ("bar", bar);
    root->setProperty ("piano", keys);
    outJson.replaceWithText (JSON::toString (var (root)));
    std::cout << "wrote " << outWav.getFullPathName() << " and " << outJson.getFullPathName() << "\n";
    return 0;
}
