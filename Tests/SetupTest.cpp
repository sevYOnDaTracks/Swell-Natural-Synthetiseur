#include "../Source/PluginProcessor.h"
#include <iostream>
#include <stdexcept>

using namespace juce;

static void check (bool ok, const char* message)
{
    if (! ok) throw std::runtime_error (message);
}

static bool showsChooser (Component& component)
{
    for (auto* child : component.getChildren())
    {
        if (! child->isVisible()) continue;
        if (auto* button = dynamic_cast<TextButton*> (child))
            if (button->getButtonText().startsWith ("Choose folder")) return true;
        if (showsChooser (*child)) return true;
    }
    return false;
}

int main()
{
    ScopedJuceInitialiser_GUI gui;
    swell::Licence::unlockForTesting();
    const TemporaryFile temp;
    const auto root = temp.getFile();
    const auto prefs = root.getChildFile ("prefs/settings.xml");
    const auto library = root.getChildFile ("Relocated library");
    try
    {
        check (library.createDirectory().wasOk(), "temporary library");
        const auto index = library.getChildFile (swell::SwellBank::indexFileName);
        check (index.replaceWithText (R"({"categories":[{"name":"Test","sounds":[{"name":"Test","id":"test","zones":[{"file":"Samples/tone.flac","lk":0,"hk":127,"lv":1,"hv":127,"root":60,"sr":44100,"n":4096}]}]}]})"), "write index");
        {
            SwellProcessor fresh (prefs), second (prefs);
            check (! fresh.getBank().isLoaded() && ! prefs.exists(), "clean scan must leave the bank unselected");
            std::unique_ptr<AudioProcessorEditor> editor (fresh.createEditor());
            check (showsChooser (*editor), "first-run prompt");
            check (! fresh.setLibraryFolder (library), "must reject an index without samples");
            const auto sample = library.getChildFile ("Samples/tone.flac");
            check (sample.getParentDirectory().createDirectory().wasOk(), "samples folder");
            check (! fresh.setLibraryFolder (library), "must reject missing audio");
            {
                FlacAudioFormat flac;
                auto stream = sample.createOutputStream();
                std::unique_ptr<AudioFormatWriter> writer (flac.createWriterFor (stream.get(), 44100, 1, 16, {}, 5));
                check (writer != nullptr, "FLAC writer");
                stream.release();
                AudioBuffer<float> audio (1, 4096);
                for (int i = 0; i < audio.getNumSamples(); ++i)
                    audio.setSample (0, i, 0.3f * std::sin (i * 0.05f));
                check (writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()), "write audio");
            }
            check (fresh.setLibraryFolder (library), "valid bank");
            second.setTheme (1); // An older instance must preserve another instance's choice.
        }
        {
            SwellProcessor reopened (prefs);
            check (reopened.getBank().root == library && reopened.getBank().sounds.size() == 1, "remembered location");
            std::unique_ptr<AudioProcessorEditor> editor (reopened.createEditor());
            check (! showsChooser (*editor), "no prompt after selection");
            std::atomic<bool> cancel { false };
            String error;
            auto instrument = swell::loadInstrument (reopened.getBank(), 0, cancel, error);
            check (instrument != nullptr && ! instrument->zones.empty(), "decode selected audio");
        }
        check (library.moveFileTo (root.getChildFile ("Moved again")), "move bank");
        SwellProcessor missing (prefs);
        std::unique_ptr<AudioProcessorEditor> editor (missing.createEditor());
        check (! missing.getBank().isLoaded() && showsChooser (*editor), "prompt again for a moved bank");
        std::cout << "PASS: fresh install, invalid folder, selection, FLAC decoding, persistence, moved bank\n";
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    return 0;
}
