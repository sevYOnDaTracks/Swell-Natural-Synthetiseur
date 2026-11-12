// Renders the promo soundtrack with the Swell engine itself (one processor per track).
// Usage: SwellScoreRender <out.wav> <events.json>
#include <juce_audio_utils/juce_audio_utils.h>
#include <CoreFoundation/CoreFoundation.h>
#include "../Source/PluginProcessor.h"
using namespace juce;

static void pump (int ms) { CFRunLoopRunInMode (kCFRunLoopDefaultMode, ms / 1000.0, false); }

static constexpr double bpm = 90.0, beat = 60.0 / bpm, bar = 4.0 * beat, sr = 48000.0;
static constexpr int bars = 12;
static constexpr double totalSec = bars * bar + 3.0;

struct Note { double t, len; int note; float vel; };
struct Track
{
    String sound;
    float gain, reverb;
    std::vector<Note> notes;
    std::function<float (double)> cutoff; // Hz over time, or null
};

static double T (int barIndex, double beatPos) { return barIndex * bar + beatPos * beat; } // barIndex from 0

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outWav (argc > 1 ? argv[1] : "/tmp/swell_promo.wav");
    const File outJson (argc > 2 ? argv[2] : "/tmp/swell_promo_events.json");

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
    Track guitar { "NylonGuitar S.Art!", 1.0f, 0.22f, {}, nullptr };
    {
        const int arp[] = { 48, 55, 59, 62, 64, 62, 59, 55 };
        for (int i = 0; i < 8; ++i) guitar.notes.push_back ({ T (6, i * 0.5), 1.2, arp[i], hv (0.6f) });
    }
    tracks.push_back (guitar);

    Track brass { "HyperBrass Live!", 0.75f, 0.2f, {}, nullptr };
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
