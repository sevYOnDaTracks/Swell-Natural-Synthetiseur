#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "SwellTheme.h"
#include "SwellArt.h"

// Rotary knob with a caps caption that turns into the value while hovered or dragged.
class SwellKnob : public juce::Component
{
public:
    SwellKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& caption, float captionSize = 13.0f);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::Slider slider;
    juce::String caption;
    float captionSize;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Labelled horizontal fader (ADSR rows).
class SwellFader : public juce::Component
{
public:
    SwellFader (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& letter);
    void resized() override;
    void paint (juce::Graphics&) override;
    std::function<void()> onChange;

private:
    juce::Slider slider;
    juce::String letter;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Library column: Favoris, All Sounds, then the bank categories, each with an icon.
class CategoryList : public juce::Component
{
public:
    static constexpr int favourites = -2, all = -1, none = -3;
    std::function<void (int)> onSelect;
    void setCategories (const juce::StringArray& names) { categories = names; repaint(); }
    void setSelected (int i) { selected = i; repaint(); }
    void setFavouriteCount (int n) { favCount = n; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = none; repaint(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    int rowAt (int y) const;
    int rowY (int code) const;
    int maxScroll() const;
    juce::StringArray categories;
    int selected = 0, hover = none, favCount = 0, scroll = 0;
    static constexpr int headerH = 40, rowH = 22;
};

// Illustrated panel with the sound name.
class HeroPanel : public juce::Component
{
public:
    std::function<void()> onToggleFavourite;
    void setInstrument (std::shared_ptr<swell::LoadedInstrument> inst, const juce::String& categoryName);
    void setFavourite (bool f) { favourite = f; repaint(); }
    void setStatus (const juce::String& s) { if (s != status) { status = s; repaint(); } }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> starArea() const { return { getWidth() - 50, 14, 34, 34 }; }
    std::shared_ptr<swell::LoadedInstrument> instrument;
    juce::String status, category;
    bool favourite = false;
};

// Waveform screen, zone map and sample facts (Edit page).
class SampleView : public juce::Component
{
public:
    void setInstrument (std::shared_ptr<swell::LoadedInstrument> inst) { instrument = std::move (inst); repaint(); }
    void paint (juce::Graphics&) override;

private:
    std::shared_ptr<swell::LoadedInstrument> instrument;
};

// ADSR or filter-response drawing, reading the live parameters.
class EnvelopeGraph : public juce::Component
{
public:
    EnvelopeGraph (juce::AudioProcessorValueTreeState& s) : state (s) {}
    void setMode (int m) { mode = m; repaint(); }
    void paint (juce::Graphics&) override;

private:
    juce::AudioProcessorValueTreeState& state;
    int mode = 0; // 0 amp envelope, 1 filter
};

class SwellKeyboard : public juce::MidiKeyboardComponent
{
public:
    SwellKeyboard (juce::MidiKeyboardState& s) : MidiKeyboardComponent (s, horizontalKeyboard) {}
    void setRoots (std::vector<int> r) { roots = std::move (r); repaint(); }

protected:
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float>, bool down, bool over, juce::Colour, juce::Colour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float>, bool down, bool over, juce::Colour) override;

private:
    bool isRoot (int n) const { return std::find (roots.begin(), roots.end(), n) != roots.end(); }
    std::vector<int> roots;
};

class SwellEditor;

class SettingsPanel : public juce::Component
{
public:
    explicit SettingsPanel (SwellEditor& e);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();

private:
    SwellEditor& editor;
    juce::TextButton changeLibrary { "Change..." }, close { "Close" };
    juce::OwnedArray<juce::TextButton> sizeButtons, themeButtons;
    juce::Rectangle<int> card;
};

class IconButton : public juce::Button
{
public:
    explicit IconButton (const juce::String& kind) : Button (kind), icon (kind) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::String icon;
};

// Fixed-size content (1280x720) scaled into the editor.
class SwellContent : public juce::Component,
                     private juce::ListBoxModel,
                     private juce::Timer
{
public:
    static constexpr int baseW = 1280, baseH = 720;

    SwellContent (SwellProcessor&, SwellEditor&);
    ~SwellContent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void themeChanged();
    void chooseLibrary();
    void showPage (int p);

private:
    enum Page { soundsPage = 0, editPage = 1, effectsPage = 2 };

    int getNumRows() override { return (int) visibleSounds.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    void timerCallback() override;
    void rebuildCategories();
    void showCategory (int code);
    void refreshSoundList();
    void step (int delta);
    void syncToSelection();
    void toggleFavourite (const juce::String& id);
    void updateModeButtons();
    void setSearchVisible (bool v);
    void setEnvTab (int t);
    SwellKnob* knob (juce::OwnedArray<SwellKnob>& arr, const char* id, const char* caption, float size = 13.0f);

    SwellProcessor& proc;
    SwellEditor& editor;

    // Top bar
    juce::TextButton tabs[3];
    IconButton settingsButton { "settings" };
    juce::Image logo;
    int page = soundsPage;

    // Sounds page
    CategoryList categoryList;
    juce::ListBox soundList;
    juce::TextEditor searchBox;
    IconButton searchButton { "search" };
    bool searchVisible = false;
    HeroPanel hero;
    IconButton prevButton { "prev" }, nextButton { "next" };
    juce::OwnedArray<SwellKnob> macroKnobs, playKnobs, fxKnobs, editKnobs, bigFxKnobs, outKnobs;
    juce::TextButton envTabs[2];
    int envTab = 0;
    juce::OwnedArray<SwellFader> ampFaders, filterFaders;
    EnvelopeGraph envGraph;
    juce::TextButton modeButtons[3];
    juce::ToggleButton slideArtToggle { "Slide S.Art" };
    juce::TextButton octDown { "-" }, octUp { "+" };

    // Edit page
    SampleView sampleView;

    // Keyboard
    SwellKeyboard keyboard;

    std::unique_ptr<juce::ParameterAttachment> modeAttachment, octaveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> slideArtAttachment;
    std::unique_ptr<juce::FileChooser> chooser;

    std::vector<int> visibleSounds;
    int currentCategory = 0, currentMode = 0, currentOctave = 0;
    std::shared_ptr<swell::LoadedInstrument> shownInstrument;
    int shownSelection = -2;

    juce::Rectangle<int> topBar, libArea, listArea, heroArea, macroArea, envArea, playArea, fxArea,
                         keysBlock, wheelArea, octLabel, editSampleArea, editRightArea, fxPageArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwellContent)
};

class SwellEditor : public juce::AudioProcessorEditor
{
public:
    explicit SwellEditor (SwellProcessor&);
    ~SwellEditor() override;

    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (SwellTheme::bg); }

    void setScale (float s);
    void setTheme (int t);
    void showSettings (bool show);
    SwellProcessor& getProcessor() { return proc; }
    SwellContent& getContent() { return content; }

private:
    SwellProcessor& proc;
    SwellLookAndFeel lnf;
    SwellContent content;
    SettingsPanel settings;
    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwellEditor)
};
