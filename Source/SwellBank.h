#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>
#include <vector>
#include <map>

// The Swell Library sound bank: a folder holding "Swell Library.swellbank" (JSON index)
// and the FLAC samples it references. Lives outside the plug-in binary.
namespace swell
{
struct ZoneInfo
{
    juce::String file;      // relative to the library root
    int lowKey = 0, highKey = 127, lowVel = 1, highVel = 127, root = 60;
    int sampleRate = 44100, numFrames = 0;
    int loopStart = -1, loopLength = 0;
    int start = 0;          // Kontakt sample-start offset (skips the pre-roll silence)
    int group = 0;
    enum Role { main, layer, slide, modwheel, fx } role = main;
};

struct SoundInfo
{
    juce::String name, id;
    int category = 0;
    std::vector<ZoneInfo> zones;
    bool oneShot = false;
    int octave = 0;         // correction so the played pitch matches the key
    float tune = 0.0f;      // fine correction in semitones
};

struct CategoryInfo
{
    juce::String name, folder;
    std::vector<int> sounds; // indices into SwellBank::sounds
};

class SwellBank
{
public:
    static constexpr const char* indexFileName = "Swell Library.swellbank";

    bool load (const juce::File& libraryFolder, juce::String& error);
    bool isLoaded() const { return ! sounds.empty(); }

    int findSound (const juce::String& id) const;

    juce::File root;
    std::vector<CategoryInfo> categories;
    std::vector<SoundInfo> sounds;

    static juce::Array<juce::File> defaultLocations();
};

// A sound decoded into RAM, ready for the audio thread. Immutable once built.
struct LoadedZone
{
    ZoneInfo info;
    int playStart = 0;      // start offset after automatic silence trimming
    std::shared_ptr<const juce::AudioBuffer<float>> audio; // 2 channels (mono duplicated), shared between zones
    int loopEnd() const { return info.loopStart + info.loopLength; }
    bool looped() const { return info.loopStart >= 0 && info.loopLength > 16; }
};

struct LoadedInstrument
{
    int soundIndex = -1;
    juce::String soundId, name, categoryName;
    bool oneShot = false;
    int octave = 0;
    float tune = 0.0f;
    std::vector<LoadedZone> zones;

    // Display data for the editor
    std::vector<float> peaks;       // waveform overview of the zone nearest middle C
    std::vector<int> rootKeys;      // distinct root keys
    int velocityLayers = 1, numLoops = 0, displaySampleRate = 44100;
    size_t memoryBytes = 0;

    bool hasSlide = false, hasModWheel = false, hasLayers = false, roundRobin = false;

    // Zones to start for a note: one main zone (round-robin across alternate groups)
    // plus any layer zones. Returns the number written to out.
    int pickZones (int note, int velocity, bool slideArticulation, bool modWheelUp,
                   unsigned int roundRobin, const LoadedZone** out, int maxOut) const;
    const LoadedZone* findZone (int note, int velocity) const; // display helper
};

std::shared_ptr<LoadedInstrument> loadInstrument (const SwellBank& bank, int soundIndex,
                                                  const std::atomic<bool>& cancel, juce::String& error);
} // namespace swell
