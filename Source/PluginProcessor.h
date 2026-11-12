#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "SwellBank.h"
#include "SamplerEngine.h"

class SwellProcessor : public juce::AudioProcessor,
                       private juce::Thread,
                       private juce::Timer
{
public:
    SwellProcessor();
    ~SwellProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Swell - Natural Synthetiser"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- bank / sound selection (message thread)
    const swell::SwellBank& getBank() const { return bank; }
    bool setLibraryFolder (const juce::File& folder);
    juce::String getBankError() const { return bankError; }
    void selectSound (int soundIndex);
    void selectSoundById (const juce::String& id);
    int getSelectedSound() const { return selectedSound; }
    bool isLoading() const { return loadingFlag.load(); }
    juce::String getLoadError() const { const juce::ScopedLock sl (loadLock); return loadError; }
    std::shared_ptr<swell::LoadedInstrument> getInstrument() const { return current; }
    int getActiveVoices() const { return activeVoices.load(); }
    float getCpuLoad() const { return cpuLoad.load(); }

    // --- global preferences (shared by every instance, stored in settings.xml)
    bool isFavourite (const juce::String& id) const { return favourites.contains (id); }
    void toggleFavourite (const juce::String& id);
    const juce::StringArray& getFavourites() const { return favourites; }
    int getTheme() const { return theme; }
    void setTheme (int t) { theme = t; saveSettings(); }
    float getUiScale() const { return uiScale; }
    void setUiScale (float s) { uiScale = s; saveSettings(); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void run() override;          // loader thread
    void timerCallback() override; // installs loaded sounds, frees retired ones
    void loadSettings();
    void saveSettings();
    void updateEngineParams();

    juce::StringArray favourites;
    int theme = 0;
    float uiScale = 1.0f;

    swell::SwellBank bank;
    juce::String bankError;
    int selectedSound = -1;
    juce::String pendingSoundId;

    // Loader
    std::atomic<int> requestedSound { -1 };
    std::atomic<bool> cancelLoad { false }, loadingFlag { false };
    juce::CriticalSection loadLock, bankLock;
    std::shared_ptr<swell::LoadedInstrument> loaded; // produced by loader, guarded by loadLock
    juce::String loadError;

    // Hand-off to the audio thread
    std::shared_ptr<swell::LoadedInstrument> current;
    std::atomic<swell::LoadedInstrument*> audioInstrument { nullptr };
    struct Retired { std::shared_ptr<swell::LoadedInstrument> inst; juce::uint64 block; };
    std::vector<Retired> retired;
    std::atomic<juce::uint64> blockCounter { 0 };
    std::atomic<juce::uint32> lastBlockTime { 0 };

    // DSP
    swell::SamplerEngine engine;
    swell::EngineParams params;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> fxBuffer;
    juce::SmoothedValue<float> volumeSmoothed, panSmoothed;
    double currentRate = 44100.0;

    std::atomic<int> activeVoices { 0 };
    std::atomic<float> cpuLoad { 0.0f };

    std::atomic<float>* pAttack = nullptr; std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pSustain = nullptr; std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pCutoff = nullptr; std::atomic<float>* pReso = nullptr;
    std::atomic<float>* pVolume = nullptr; std::atomic<float>* pPan = nullptr;
    std::atomic<float>* pTune = nullptr; std::atomic<float>* pVelSens = nullptr;
    std::atomic<float>* pChorus = nullptr; std::atomic<float>* pReverb = nullptr;
    std::atomic<float>* pDrive = nullptr; std::atomic<float>* pGlide = nullptr;
    std::atomic<float>* pStart = nullptr; std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pSlideArt = nullptr; std::atomic<float>* pOctave = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwellProcessor)
};
