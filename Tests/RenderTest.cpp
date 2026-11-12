// Headless check of every sound: load, play, detect pitch vs pressed key, onset delay, NaN.
// Also snapshots the editor. Usage: SwellRenderTest <outDir> [maxSounds]
#include <juce_audio_utils/juce_audio_utils.h>
#include <CoreFoundation/CoreFoundation.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
using namespace juce;

static void pump (int ms) { CFRunLoopRunInMode (kCFRunLoopDefaultMode, ms / 1000.0, false); }

// YIN fundamental estimate (Hz), 0 when unvoiced
static double yin (const float* x, int n, double sr)
{
    const int minLag = (int) (sr / 2000.0), maxLag = jmin (n / 2, (int) (sr / 30.0));
    std::vector<double> d ((size_t) maxLag + 1, 0.0);
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        double s = 0;
        for (int i = 0; i < n - maxLag; ++i) { const double v = x[i] - x[i + tau]; s += v * v; }
        d[(size_t) tau] = s;
    }
    double run = 0;
    std::vector<double> cm ((size_t) maxLag + 1, 1.0);
    for (int tau = 1; tau <= maxLag; ++tau) { run += d[(size_t) tau]; cm[(size_t) tau] = run > 0 ? d[(size_t) tau] * tau / run : 1.0; }
    int best = -1;
    for (int tau = minLag; tau < maxLag; ++tau)
        if (cm[(size_t) tau] < 0.12) { while (tau + 1 < maxLag && cm[(size_t) tau + 1] < cm[(size_t) tau]) ++tau; best = tau; break; }
    if (best < 0) return 0.0;
    const double a = cm[(size_t) best - 1], b = cm[(size_t) best], c = cm[(size_t) best + 1];
    const double shift = (a - 2 * b + c) != 0 ? 0.5 * (a - c) / (a - 2 * b + c) : 0.0;
    return sr / (best + shift);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir (argc > 1 ? argv[1] : "/tmp");
    const int maxSounds = argc > 2 ? atoi (argv[2]) : 100000;
    auto proc = std::make_unique<SwellProcessor>();
    const auto& bank = proc->getBank();
    std::cout << "bank loaded: " << bank.isLoaded() << " sounds " << bank.sounds.size() << "\n";
    if (! bank.isLoaded()) return 1;

    for (auto* id : { "reverb", "chorus" })
        proc->apvts.getParameter (id)->setValueNotifyingHost (0.0f);

    const double sr = 48000.0;
    proc->setPlayConfigDetails (0, 2, sr, 512);
    proc->prepareToPlay (sr, 512);
    AudioBuffer<float> buf (2, 512);

    auto waitLoaded = [&] (int idx)
    {
        proc->selectSound (idx);
        for (int i = 0; i < 500; ++i)
        {
            pump (10);
            auto inst = proc->getInstrument();
            if (! proc->isLoading() && inst != nullptr && inst->soundIndex == idx) return true;
        }
        return false;
    };

    std::map<String, std::map<int, int>> octLog;
    var octaveFix (new DynamicObject()), tuneFix (new DynamicObject());
    int fails = 0, pitchBad = 0, octave = 0, unvoiced = 0, tested = 0, lateOnset = 0;
    StringArray report;
    const int total = jmin (maxSounds, (int) bank.sounds.size());
    for (int idx = 0; idx < total; ++idx)
    {
        const auto& snd = bank.sounds[(size_t) idx];
        if (! waitLoaded (idx)) { std::cout << "LOAD FAIL " << snd.name << " " << proc->getLoadError() << "\n"; ++fails; continue; }
        const bool drums = snd.oneShot;
        String line = String (bank.categories[(size_t) snd.category].name).paddedRight (' ', 22) + snd.name.paddedRight (' ', 28);
        bool soundFail = false;
        std::vector<int> octs;
        std::vector<double> clsList;

        for (int note : { 48, 60, 72 })
        {
            for (int vel : { 70, 110 })
            {
                MidiBuffer midi;
                std::vector<float> mono;
                bool nan = false;
                for (int b = 0; b < 60; ++b) // 0.64 s
                {
                    midi.clear();
                    if (b == 0) midi.addEvent (MidiMessage::noteOn (1, note, (uint8) vel), 0);
                    proc->processBlock (buf, midi);
                    for (int i = 0; i < 512; ++i)
                    {
                        const float l = buf.getSample (0, i), r = buf.getSample (1, i);
                        if (! std::isfinite (l) || ! std::isfinite (r)) nan = true;
                        mono.push_back (0.5f * (l + r));
                    }
                }
                MidiBuffer off;
                off.addEvent (MidiMessage::allSoundOff (1), 0);
                proc->processBlock (buf, off);
                for (int k = 0; k < 4; ++k) { MidiBuffer e; proc->processBlock (buf, e); }

                float peak = 0;
                int onset = -1;
                for (size_t i = 0; i < mono.size(); ++i)
                {
                    peak = jmax (peak, std::abs (mono[i]));
                    if (onset < 0 && std::abs (mono[i]) > 0.01f) onset = (int) i;
                }
                if (nan || peak < 1e-4f) { soundFail = true; line << "  [" << note << "/" << vel << (nan ? " NaN]" : " SILENT]"); continue; }
                const double onsetMs = onset * 1000.0 / sr;
                if (onsetMs > 40.0 && vel == 110) { ++lateOnset; line << "  [onset " << roundToInt (onsetMs) << "ms]"; }

                if (drums || vel != 110) continue;
                const int start = (int) (0.18 * sr);
                const double f = yin (mono.data() + start, 4096, sr);
                ++tested;
                if (f <= 0) { ++unvoiced; continue; }
                const double expected = 440.0 * std::pow (2.0, (note - 69) / 12.0);
                const double err = 12.0 * std::log2 (f / expected);
                const double cls = err - 12.0 * std::round (err / 12.0);
                if (std::abs (cls) > 0.6) { ++pitchBad; line << "  [" << note << " PITCH " << String (err, 1) << " st]"; }
                if (std::abs (cls) <= 0.6) octs.push_back (roundToInt (err / 12.0));
                clsList.push_back (cls);
                if (std::abs (cls) > 0.6) {}
                else if (std::abs (err) > 6.0) { ++octave; octLog[bank.categories[(size_t) snd.category].name][roundToInt (err / 12.0)]++; }
                else octLog[bank.categories[(size_t) snd.category].name][0]++;
            }
        }
        if (soundFail) ++fails;
        if (line.contains ("[")) report.add (line);
        if (clsList.size() == 3)
        {
            const double mean = (clsList[0] + clsList[1] + clsList[2]) / 3.0;
            const double spread = jmax (std::abs (clsList[0] - mean), std::abs (clsList[1] - mean), std::abs (clsList[2] - mean));
            if (spread < 0.3 && std::abs (mean) > 0.4)
                tuneFix.getDynamicObject()->setProperty (snd.id, -std::round (mean * 100.0) / 100.0);
        }
        if (octs.size() >= 2)
        {
            std::map<int, int> votes;
            for (int o : octs) votes[o]++;
            int best = 0, bestN = 0;
            for (auto& [o, n] : votes) if (n > bestN) { best = o; bestN = n; }
            if (bestN >= 2 && best != 0) octaveFix.getDynamicObject()->setProperty (snd.id, -best);
        }
        if ((idx + 1) % 50 == 0) std::cout << "  " << (idx + 1) << " / " << total << "\n" << std::flush;
    }

    std::cout << "\n" << report.joinIntoString ("\n") << "\n\n";
    std::cout << "tested pitches " << tested << "  wrong-note " << pitchBad << "  octave-only " << octave
              << "  unvoiced " << unvoiced << "  late-onset " << lateOnset << "  failures " << fails << "\n";

    for (auto& [cat, m] : octLog)
    {
        std::cout << cat.paddedRight (' ', 24);
        for (auto& [o, n] : m) std::cout << " oct" << (o > 0 ? "+" : "") << o << ":" << n;
        std::cout << "\n";
    }
    if (argc > 3)
    {
        File (argv[3]).replaceWithText (JSON::toString (octaveFix));
        File (argv[3]).getSiblingFile ("tune_fix.json").replaceWithText (JSON::toString (tuneFix));
        std::cout << "tune fixes: " << JSON::toString (tuneFix, true) << "\n";
        std::cout << "octave fixes: " << octaveFix.getDynamicObject()->getProperties().size() << " -> " << argv[3] << "\n";
        return 0;
    }
    // Editor snapshots
    waitLoaded (bank.categories[0].sounds[0]);
    {
        std::unique_ptr<AudioProcessorEditor> ed (proc->createEditor());
        ed->setSize (SwellContent::baseW, SwellContent::baseH);
        auto snap = [&] (const String& name)
        {
            for (int i = 0; i < 8; ++i) pump (30);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
            File f = outDir.getChildFile (name);
            f.deleteFile();
            FileOutputStream os (f);
            PNGImageFormat().writeImageToStream (img, os);
            std::cout << "snapshot " << f.getFullPathName() << "\n";
        };
        const int savedTheme = proc->getTheme();
        auto* se = dynamic_cast<SwellEditor*> (ed.get());
        se->setTheme (0);
        snap ("swell_ui.png");
        se->showSettings (true);
        snap ("swell_settings.png");
        se->showSettings (false);
        se->getContent().showPage (1);
        snap ("swell_edit.png");
        se->getContent().showPage (2);
        snap ("swell_effects.png");
        se->getContent().showPage (0);
        se->setTheme (0);
        for (int ci : { 4, 8, 13 })
        {
            if (ci >= (int) bank.categories.size()) continue;
            waitLoaded (bank.categories[(size_t) ci].sounds[0]);
            se->getContent().showPage (0);
            snap ("swell_cat" + String (ci) + ".png");
        }
        se->setTheme (savedTheme);
    }
    proc->releaseResources();
    std::cout << (fails == 0 && pitchBad == 0 ? "ALL OK" : "ISSUES") << "\n";
    return fails;
}
