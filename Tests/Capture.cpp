// Captures real UI states for the promo film. Usage: SwellCapture <outDir>
#include <juce_audio_utils/juce_audio_utils.h>
#include <CoreFoundation/CoreFoundation.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
using namespace juce;

static void pump (int ms) { CFRunLoopRunInMode (kCFRunLoopDefaultMode, ms / 1000.0, false); }

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const bool licenceShot = argc > 2 && String (argv[2]) == "licence";
    if (! licenceShot) swell::Licence::unlockForTesting();
    const File out (argc > 1 ? argv[1] : "/tmp");
    out.createDirectory();
    auto proc = std::make_unique<SwellProcessor>();
    const auto& bank = proc->getBank();
    auto find = [&] (const String& n) { for (size_t i = 0; i < bank.sounds.size(); ++i) if (bank.sounds[i].name == n) return (int) i; return -1; };
    auto select = [&] (const String& n)
    {
        const int idx = find (n);
        proc->selectSound (idx);
        for (int i = 0; i < 600; ++i)
        {
            pump (10);
            auto inst = proc->getInstrument();
            if (! proc->isLoading() && inst != nullptr && inst->soundIndex == idx) break;
        }
        for (int i = 0; i < 10; ++i) pump (25);
    };
    auto setP = [&] (const char* id, float v) { auto* p = proc->apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };

    std::unique_ptr<AudioProcessorEditor> ed (proc->createEditor());
    auto* se = dynamic_cast<SwellEditor*> (ed.get());
    se->setTheme (0);
    ed->setSize (SwellContent::baseW, SwellContent::baseH);
    auto save = [&] (const String& name, Rectangle<int> area, float scale)
    {
        for (int i = 0; i < 6; ++i) pump (25);
        auto img = ed->createComponentSnapshot (area, true, scale);
        File f = out.getChildFile (name);
        f.deleteFile();
        FileOutputStream os (f);
        PNGImageFormat().writeImageToStream (img, os);
        std::cout << name << " " << img.getWidth() << "x" << img.getHeight() << "\n";
    };
    const auto full = ed->getLocalBounds();

    if (argc > 2 && String (argv[2]) == "noir")
    {
        setP ("cutoff", 20000.0f);
        select ("GrandPiano Live!");
        save ("default.png", full, 2.0f);
        se->setTheme (4);
        save ("noir.png", full, 2.0f);
        se->showSettings (true);
        save ("settings.png", full, 2.0f);
        se->showSettings (false);
        se->setTheme (0);
        return 0;
    }

    if (licenceShot)
    {
        select ("GrandPiano Live!");
        save ("licence.png", full, 2.0f);
        return 0;
    }

    if (argc > 2 && String (argv[2]) == "tour")
    {
        const char* names[] = { "GrandPiano Live!", "SuitcaseEP", "Strings", "NylonGuitar S.Art!", "ElectricBass Cool!", "JazzTenor Sweet!",
                                "Flute Sweet!", "JazzOrgan Cool!", "SoloTrumpet", "HyperBrass Live!", "GospelVoices Live!", "FunkyLead", "StandardKit1" };
        setP ("cutoff", 20000.0f);
        for (int i = 0; i < 13; ++i)
        {
            select (names[i]);
            save ("tour_" + String (i).paddedLeft ('0', 2) + ".png", full, 2.0f);
        }
        return 0;
    }

    setP ("cutoff", 20000.0f);
    select ("SuitcaseEP");               save ("ui_before.png", full, 2.0f);
    select ("GrandPiano Live!");         save ("ui_piano.png", full, 2.0f);
    // Cutoff sweep frames on the macro panel (content coords)
    const Rectangle<int> macro (602, 286, 648, 120);
    const float cuts[] = { 300, 420, 600, 850, 1200, 1700, 2400, 3400, 4800, 6800, 9600, 13500, 20000 };
    for (int i = 0; i < 13; ++i)
    {
        setP ("cutoff", cuts[i]);
        save ("cutoff_" + String (i).paddedLeft ('0', 2) + ".png", macro, 3.0f);
    }
    setP ("cutoff", 20000.0f);
    select ("NylonGuitar S.Art!");       save ("ui_guitar.png", full, 2.0f);
    select ("HyperBrass Live!");         save ("ui_brass.png", full, 2.0f);
    select ("StandardKit1");             save ("ui_drums.png", full, 2.0f);
    select ("GrandPiano Live!");
    return 0;
}
