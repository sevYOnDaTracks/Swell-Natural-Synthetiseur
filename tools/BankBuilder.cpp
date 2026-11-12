// Swell Bank Builder
// Converts the PSR-S900 NCW samples into the standalone "Swell Library" sound bank:
//   <out>/Samples/<category>/<sound>/<file>.flac   (lossless 24-bit FLAC)
//   <out>/Swell Library.swellbank                   (JSON index: categories, sounds, zones, loops)
//
// Usage: SwellBankBuilder <psr_bank_index.json> <PSR "Samples" folder> <output library folder>

#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include "../Source/NcwDecoder.h"

using namespace juce;

static bool convertOne (const File& src, const File& dst, String& err)
{
    swell::NcwAudio audio;
    std::string e;
    if (! swell::NcwDecoder::decodeFile (src.getFullPathName().toStdString(), audio, e))
    {
        err = e;
        return false;
    }

    dst.getParentDirectory().createDirectory();
    const File tmp = dst.withFileExtension ("tmp");
    tmp.deleteFile();

    FlacAudioFormat flac;
    std::unique_ptr<OutputStream> stream (tmp.createOutputStream());
    if (stream == nullptr) { err = "cannot write " + tmp.getFullPathName(); return false; }

    const int bits = audio.bitsPerSample >= 24 ? 24 : 16;
    std::unique_ptr<AudioFormatWriter> writer (flac.createWriterFor (stream.get(), audio.sampleRate,
                                                                     (unsigned int) audio.channels, bits, {}, 5));
    if (writer == nullptr) { err = "FLAC writer failed"; return false; }
    stream.release();

    std::vector<const float*> ptrs;
    for (auto& ch : audio.data) ptrs.push_back (ch.data());
    AudioBuffer<float> buf (const_cast<float**> (ptrs.data()), audio.channels, audio.numFrames);
    if (! writer->writeFromAudioSampleBuffer (buf, 0, audio.numFrames)) { err = "FLAC write failed"; return false; }
    writer.reset();
    return tmp.moveFileTo (dst);
}

int main (int argc, char* argv[])
{
    if (argc < 4)
    {
        std::cout << "Usage: SwellBankBuilder <psr_bank_index.json> <PSR Samples folder> <output library folder>\n";
        return 1;
    }

    const File indexFile (File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    const File srcRoot (File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    const File outRoot (File::getCurrentWorkingDirectory().getChildFile (argv[3]));

    var bank = JSON::parse (indexFile);
    if (! bank.isObject()) { std::cerr << "Cannot parse " << indexFile.getFullPathName() << "\n"; return 1; }

    outRoot.getChildFile ("Samples").createDirectory();

    // Collect unique sample files and rewrite paths to .flac
    StringArray jobs;
    for (auto& cat : *bank["categories"].getArray())
        for (auto& sound : *cat["sounds"].getArray())
            for (auto& zone : *sound["zones"].getArray())
            {
                const String src = zone["src"].toString();
                jobs.addIfNotAlreadyThere (src);
                zone.getDynamicObject()->setProperty ("file", "Samples/" + src.upToLastOccurrenceOf (".", false, false) + ".flac");
                zone.getDynamicObject()->removeProperty ("src");
            }

    std::cout << "Converting " << jobs.size() << " samples to FLAC...\n";
    std::atomic<int> done { 0 }, failed { 0 }, skipped { 0 };
    CriticalSection logLock;

    ThreadPool pool (jmax (2, SystemStats::getNumCpus()));
    for (auto& rel : jobs)
    {
        pool.addJob ([&, rel]
        {
            const File src = srcRoot.getChildFile (rel);
            const File dst = outRoot.getChildFile ("Samples/" + rel.upToLastOccurrenceOf (".", false, false) + ".flac");
            if (dst.existsAsFile() && dst.getSize() > 0)
                ++skipped;
            else
            {
                String err;
                if (! convertOne (src, dst, err))
                {
                    ++failed;
                    const ScopedLock sl (logLock);
                    std::cerr << "FAILED " << rel << " : " << err << "\n";
                }
            }
            const int n = ++done;
            if (n % 250 == 0)
            {
                const ScopedLock sl (logLock);
                std::cout << "  " << n << " / " << jobs.size() << "\n" << std::flush;
            }
        });
    }
    while (pool.getNumJobs() > 0)
        Thread::sleep (200);

    bank.getDynamicObject()->setProperty ("name", "Swell Library");
    bank.getDynamicObject()->setProperty ("product", "Swell - Natural Synthetiser");
    const File index = outRoot.getChildFile ("Swell Library.swellbank");
    index.replaceWithText (JSON::toString (bank, true));

    std::cout << "Done. converted " << (done - skipped - failed) << ", skipped " << skipped.load()
              << ", failed " << failed.load() << "\nIndex: " << index.getFullPathName() << "\n";
    return failed > 0 ? 2 : 0;
}
