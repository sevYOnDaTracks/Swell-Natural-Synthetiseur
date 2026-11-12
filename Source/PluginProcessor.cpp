#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// Transparent below -2 dBFS, smooth ceiling at 1.0 above.
static inline float softLimit (float x)
{
    constexpr float knee = 0.8f;
    const float a = std::abs (x);
    if (a <= knee) return x;
    const float over = (a - knee) / (1.0f - knee);
    return std::copysign (knee + (1.0f - knee) * std::tanh (over), x);
}

static File settingsFile()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile ("Swell/Natural Synthetiser/settings.xml");
}

SwellProcessor::SwellProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      Thread ("Swell loader"),
      apvts (*this, nullptr, "SwellNatural", createLayout())
{
    pAttack = apvts.getRawParameterValue ("attack");
    pDecay = apvts.getRawParameterValue ("decay");
    pSustain = apvts.getRawParameterValue ("sustain");
    pRelease = apvts.getRawParameterValue ("release");
    pCutoff = apvts.getRawParameterValue ("cutoff");
    pReso = apvts.getRawParameterValue ("reso");
    pVolume = apvts.getRawParameterValue ("volume");
    pPan = apvts.getRawParameterValue ("pan");
    pTune = apvts.getRawParameterValue ("tune");
    pVelSens = apvts.getRawParameterValue ("velsens");
    pChorus = apvts.getRawParameterValue ("chorus");
    pReverb = apvts.getRawParameterValue ("reverb");
    pDrive = apvts.getRawParameterValue ("drive");
    pGlide = apvts.getRawParameterValue ("glide");
    pStart = apvts.getRawParameterValue ("start");
    pMode = apvts.getRawParameterValue ("mode");
    pSlideArt = apvts.getRawParameterValue ("slideart");
    pOctave = apvts.getRawParameterValue ("octave");

    loadSettings();
    startThread (Thread::Priority::normal);
    startTimerHz (30);

    if (bank.isLoaded() && selectedSound < 0)
        selectSound (bank.categories.empty() || bank.categories[0].sounds.empty() ? 0 : bank.categories[0].sounds[0]);
}

SwellProcessor::~SwellProcessor()
{
    stopTimer();
    cancelLoad = true;
    signalThreadShouldExit();
    notify();
    stopThread (4000);
}

AudioProcessorValueTreeState::ParameterLayout SwellProcessor::createLayout()
{
    using P = AudioParameterFloat;
    auto time = [] (float lo, float hi, float def)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (0.4f);
        return std::make_pair (r, def);
    };
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto addTime = [&] (const char* id, const char* name, float lo, float hi, float def)
    {
        auto [r, d] = time (lo, hi, def);
        ps.push_back (std::make_unique<P> (ParameterID { id, 1 }, name, r, d,
            AudioParameterFloatAttributes().withLabel ("s")
                .withStringFromValueFunction ([] (float v, int) { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; })));
    };
    addTime ("attack", "Attack", 0.001f, 8.0f, 0.002f);
    addTime ("decay", "Decay", 0.01f, 10.0f, 1.0f);
    ps.push_back (std::make_unique<P> (ParameterID { "sustain", 1 }, "Sustain", NormalisableRange<float> (0.0f, 1.0f), 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
    addTime ("release", "Release", 0.01f, 10.0f, 0.35f);

    NormalisableRange<float> cut (40.0f, 20000.0f);
    cut.setSkewForCentre (1200.0f);
    ps.push_back (std::make_unique<P> (ParameterID { "cutoff", 1 }, "Cutoff", cut, 20000.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; })));
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    ps.push_back (std::make_unique<P> (ParameterID { "reso", 1 }, "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.1f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { "volume", 1 }, "Volume", NormalisableRange<float> (-48.0f, 6.0f, 0.0f, 2.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    ps.push_back (std::make_unique<P> (ParameterID { "pan", 1 }, "Pan", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            const int p = roundToInt (v * 100.0f);
            return p == 0 ? String ("C") : (p < 0 ? "L" + String (-p) : "R" + String (p));
        })));
    ps.push_back (std::make_unique<P> (ParameterID { "tune", 1 }, "Tune", NormalisableRange<float> (-12.0f, 12.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " st"; })));
    ps.push_back (std::make_unique<P> (ParameterID { "velsens", 1 }, "Velocity", NormalisableRange<float> (0.0f, 1.0f), 0.75f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { "chorus", 1 }, "Chorus", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { "reverb", 1 }, "Reverb", NormalisableRange<float> (0.0f, 1.0f), 0.15f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { "drive", 1 }, "Drive", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    {
        NormalisableRange<float> g (0.0f, 2.0f);
        g.setSkewForCentre (0.25f);
        ps.push_back (std::make_unique<P> (ParameterID { "glide", 1 }, "Slide", g, 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 0.001f ? String ("Off") : String (roundToInt (v * 1000.0f)) + " ms"; })));
    }
    ps.push_back (std::make_unique<P> (ParameterID { "start", 1 }, "Start", NormalisableRange<float> (-100.0f, 500.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            const int ms = roundToInt (v);
            return ms == 0 ? String ("Auto") : (ms > 0 ? "+" : "") + String (ms) + " ms";
        })));
    ps.push_back (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", -2, 2, 0,
        AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v == 0 ? String ("0") : (v > 0 ? "+" : "") + String (v); })));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Poly", "Mono", "Legato" }, 0));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "slideart", 1 }, "S.Art Slide", false));
    return { ps.begin(), ps.end() };
}

// ------------------------------------------------------------------ settings / bank

void SwellProcessor::loadSettings()
{
    File folder;
    if (auto xml = parseXML (settingsFile()))
    {
        folder = File (xml->getStringAttribute ("library"));
        favourites.addTokens (xml->getStringAttribute ("favourites"), "|", "");
        favourites.removeEmptyStrings();
        theme = xml->getIntAttribute ("theme", 0);
        uiScale = (float) xml->getDoubleAttribute ("scale", 1.0);
    }

    String err;
    if (folder != File() && bank.load (folder, err))
        return;

    for (auto& f : swell::SwellBank::defaultLocations())
        if (bank.load (f, err))
        {
            saveSettings();
            bankError = {};
            return;
        }
    bankError = "Swell Library introuvable. Choisis le dossier de la banque.";
}

void SwellProcessor::saveSettings()
{
    // Merge with what other instances may have written meanwhile
    XmlElement xml ("SwellNatural");
    xml.setAttribute ("library", bank.root.getFullPathName());
    xml.setAttribute ("favourites", favourites.joinIntoString ("|"));
    xml.setAttribute ("theme", theme);
    xml.setAttribute ("scale", (double) uiScale);
    settingsFile().getParentDirectory().createDirectory();
    xml.writeTo (settingsFile());
}

void SwellProcessor::toggleFavourite (const String& id)
{
    if (favourites.contains (id)) favourites.removeString (id);
    else favourites.add (id);
    saveSettings();
}

bool SwellProcessor::setLibraryFolder (const File& folder)
{
    swell::SwellBank fresh;
    String err;
    if (! fresh.load (folder, err))
    {
        bankError = err;
        return false;
    }
    cancelLoad = true;
    {
        const ScopedLock sl (loadLock);
        loaded.reset();
    }
    {
        const ScopedLock bl (bankLock);
        bank = std::move (fresh);
    }
    bankError = {};
    saveSettings();
    selectedSound = -1;
    const int idx = pendingSoundId.isNotEmpty() ? bank.findSound (pendingSoundId) : -1;
    selectSound (idx >= 0 ? idx : bank.categories[0].sounds.empty() ? 0 : bank.categories[0].sounds[0]);
    return true;
}

void SwellProcessor::selectSound (int soundIndex)
{
    if (! bank.isLoaded() || soundIndex < 0 || soundIndex >= (int) bank.sounds.size())
        return;
    selectedSound = soundIndex;
    pendingSoundId = bank.sounds[(size_t) soundIndex].id;
    cancelLoad = true;
    requestedSound = soundIndex;
    loadingFlag = true;
    notify();
}

void SwellProcessor::selectSoundById (const String& id)
{
    pendingSoundId = id;
    const int idx = bank.findSound (id);
    if (idx >= 0)
        selectSound (idx);
}

void SwellProcessor::run()
{
    while (! threadShouldExit())
    {
        const int req = requestedSound.exchange (-1);
        if (req < 0)
        {
            wait (-1);
            continue;
        }
        cancelLoad = false;
        String err;
        std::shared_ptr<swell::LoadedInstrument> inst;
        {
            const ScopedLock bl (bankLock);
            inst = swell::loadInstrument (bank, req, cancelLoad, err);
        }
        if (cancelLoad.load() && requestedSound.load() >= 0)
            continue; // superseded by a newer request
        const ScopedLock sl (loadLock);
        if (inst != nullptr)
            loaded = std::move (inst);
        else if (! cancelLoad.load())
        {
            loadError = err;
            loadingFlag = false;
        }
    }
}

void SwellProcessor::timerCallback()
{
    std::shared_ptr<swell::LoadedInstrument> fresh;
    {
        const ScopedLock sl (loadLock);
        fresh = std::move (loaded);
        loaded.reset();
    }
    if (fresh != nullptr)
    {
        if (current != nullptr)
            retired.push_back ({ current, blockCounter.load() });
        current = fresh;
        audioInstrument.store (current.get());
        if (requestedSound.load() < 0)
            loadingFlag = false;
        const ScopedLock sl (loadLock);
        loadError = {};
    }

    // Free instruments the audio thread can no longer be using.
    const auto now = blockCounter.load();
    const bool audioIdle = Time::getMillisecondCounter() - lastBlockTime.load() > 1000;
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [&] (const Retired& r) { return audioIdle || now >= r.block + 2; }),
                   retired.end());
}

// ------------------------------------------------------------------ audio

void SwellProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    engine.prepare (sampleRate);

    dsp::ProcessSpec spec { sampleRate, (uint32) samplesPerBlock, 2 };
    chorus.prepare (spec);
    chorus.setRate (0.8f);
    chorus.setDepth (0.35f);
    chorus.setCentreDelay (9.0f);
    chorus.setFeedback (0.1f);
    reverb.prepare (spec);
    fxBuffer.setSize (2, samplesPerBlock);

    volumeSmoothed.reset (sampleRate, 0.05);
    panSmoothed.reset (sampleRate, 0.05);
    volumeSmoothed.setCurrentAndTargetValue (Decibels::decibelsToGain (pVolume->load(), -48.0f));
    panSmoothed.setCurrentAndTargetValue (pPan->load());
    keyboardState.reset();
}

bool SwellProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == AudioChannelSet::mono();
}

void SwellProcessor::updateEngineParams()
{
    params.attack = pAttack->load();
    params.decay = pDecay->load();
    params.sustain = pSustain->load();
    params.release = pRelease->load();
    params.cutoff = pCutoff->load();
    params.resonance = pReso->load();
    params.tune = pTune->load();
    params.velSens = pVelSens->load();
    params.glide = pGlide->load();
    params.startOffsetMs = pStart->load();
    params.mode = (swell::PlayMode) jlimit (0, 2, (int) pMode->load());
    params.slideArticulation = pSlideArt->load() > 0.5f;
    params.octave = roundToInt (pOctave->load());
}

void SwellProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const auto t0 = Time::getHighResolutionTicks();
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    engine.setInstrument (audioInstrument.load());
    updateEngineParams();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    if (fxBuffer.getNumSamples() < numSamples)
        fxBuffer.setSize (2, numSamples, false, false, true);
    fxBuffer.clear (0, numSamples);

    int pos = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int when = jlimit (0, numSamples, meta.samplePosition);
        if (when > pos)
        {
            engine.render (fxBuffer, pos, when - pos, params);
            pos = when;
        }
        if (msg.isNoteOn())
            engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity(), params);
        else if (msg.isNoteOff())
            engine.noteOff (msg.getNoteNumber(), params);
        else if (msg.isSustainPedalOn())
            engine.setSustain (true);
        else if (msg.isSustainPedalOff())
            engine.setSustain (false);
        else if (msg.isPitchWheel())
            engine.setPitchBend (msg.getPitchWheelValue());
        else if (msg.isControllerOfType (1))
            engine.setModWheel ((float) msg.getControllerValue() / 127.0f);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            engine.allOff (msg.isAllSoundOff());
    }
    if (pos < numSamples)
        engine.render (fxBuffer, pos, numSamples - pos, params);

    AudioBuffer<float> work (fxBuffer.getArrayOfWritePointers(), 2, numSamples);
    dsp::AudioBlock<float> block (work);
    dsp::ProcessContextReplacing<float> ctx (block);

    // Drive: soft saturation with level compensation
    const float drive = pDrive->load();
    if (drive > 0.001f)
    {
        const float pre = 1.0f + drive * 9.0f;
        const float comp = 1.0f / std::sqrt (pre);
        for (int c = 0; c < 2; ++c)
        {
            float* d = work.getWritePointer (c);
            for (int i = 0; i < numSamples; ++i)
                d[i] += drive * (std::tanh (d[i] * pre) * comp - d[i]);
        }
    }

    const float chorusMix = pChorus->load();
    if (chorusMix > 0.001f)
    {
        chorus.setMix (chorusMix * 0.6f);
        chorus.process (ctx);
    }

    const float rev = pReverb->load();
    dsp::Reverb::Parameters rp;
    rp.roomSize = 0.55f + 0.35f * rev;
    rp.damping = 0.45f;
    rp.wetLevel = rev * 0.45f;
    rp.dryLevel = 1.0f - rev * 0.25f;
    rp.width = 1.0f;
    reverb.setParameters (rp);
    if (rev > 0.001f)
        reverb.process (ctx);

    volumeSmoothed.setTargetValue (Decibels::decibelsToGain (pVolume->load(), -48.0f));
    panSmoothed.setTargetValue (pPan->load());
    const int outCh = buffer.getNumChannels();
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = volumeSmoothed.getNextValue() * 0.7f;
        const float p = panSmoothed.getNextValue();
        const float gl = g * jmin (1.0f, 1.0f - p);
        const float gr = g * jmin (1.0f, 1.0f + p);
        const float l = softLimit (work.getSample (0, i) * gl);
        const float r = softLimit (work.getSample (1, i) * gr);
        if (outCh >= 2)
        {
            buffer.setSample (0, i, l);
            buffer.setSample (1, i, r);
        }
        else if (outCh == 1)
            buffer.setSample (0, i, 0.5f * (l + r));
    }

    activeVoices = engine.activeVoices();
    ++blockCounter;
    lastBlockTime = Time::getMillisecondCounter();

    const double elapsed = Time::highResolutionTicksToSeconds (Time::getHighResolutionTicks() - t0);
    const double budget = numSamples / currentRate;
    cpuLoad = cpuLoad.load() * 0.9f + 0.1f * (float) (elapsed / budget);
}

// ------------------------------------------------------------------ state

void SwellProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("sound", pendingSoundId, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SwellProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = ValueTree::fromXml (*xml);
        if (tree.isValid() && tree.hasType (apvts.state.getType()))
        {
            const String id = tree.getProperty ("sound").toString();
            apvts.replaceState (tree);
            if (id.isNotEmpty())
                selectSoundById (id);
        }
    }
}

AudioProcessorEditor* SwellProcessor::createEditor() { return new SwellEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SwellProcessor(); }
