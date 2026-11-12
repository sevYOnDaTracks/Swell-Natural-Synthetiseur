#include "SwellBank.h"
#include <set>

namespace swell
{
using namespace juce;

bool SwellBank::load (const File& folder, String& error)
{
    root = File();
    categories.clear();
    sounds.clear();

    const File index = folder.getChildFile (indexFileName);
    if (! index.existsAsFile())
    {
        error = "Swell Library introuvable dans " + folder.getFullPathName();
        return false;
    }

    if (! folder.getChildFile ("Samples").isDirectory())
    {
        error = "Extract the complete Swell Library ZIP. The Samples folder is missing.";
        return false;
    }

    const var json = JSON::parse (index);
    const auto* cats = json["categories"].getArray();
    if (cats == nullptr)
    {
        error = "Index de banque invalide";
        return false;
    }

    for (const auto& c : *cats)
    {
        CategoryInfo cat;
        cat.name = c["name"].toString();
        cat.folder = c["folder"].toString();
        const int catIndex = (int) categories.size();

        if (const auto* list = c["sounds"].getArray())
        {
            for (const auto& s : *list)
            {
                SoundInfo snd;
                snd.name = s["name"].toString();
                snd.id = s["id"].toString();
                snd.category = catIndex;
                snd.oneShot = snd.name.containsIgnoreCase ("Kit");
                snd.octave = (int) s.getProperty ("octave", 0);
                snd.tune = (float) (double) s.getProperty ("tune", 0.0);

                if (const auto* zones = s["zones"].getArray())
                {
                    for (const auto& z : *zones)
                    {
                        ZoneInfo zi;
                        zi.file = z["file"].toString();
                        zi.lowKey = (int) z["lk"];
                        zi.highKey = (int) z["hk"];
                        zi.lowVel = (int) z["lv"];
                        zi.highVel = (int) z["hv"];
                        zi.root = (int) z["root"];
                        zi.sampleRate = (int) z["sr"];
                        zi.numFrames = (int) z["n"];
                        zi.start = (int) z.getProperty ("start", 0);
                        zi.group = (int) z.getProperty ("group", 0);
                        const String role = z.getProperty ("role", "main").toString();
                        zi.role = role == "layer" ? ZoneInfo::layer : role == "slide" ? ZoneInfo::slide
                                : role == "mw" ? ZoneInfo::modwheel : role == "fx" ? ZoneInfo::fx : ZoneInfo::main;
                        if (const auto* loop = z["loop"].getArray(); loop != nullptr && loop->size() == 2)
                        {
                            zi.loopStart = (int) (*loop)[0];
                            zi.loopLength = (int) (*loop)[1];
                        }
                        snd.zones.push_back (zi);
                    }
                }
                if (snd.zones.empty())
                    continue;
                cat.sounds.push_back ((int) sounds.size());
                sounds.push_back (std::move (snd));
            }
        }
        categories.push_back (std::move (cat));
    }

    if (sounds.empty())
    {
        error = "La banque ne contient aucun son";
        return false;
    }
    if (! folder.getChildFile (sounds.front().zones.front().file).existsAsFile())
    {
        sounds.clear();
        categories.clear();
        error = "Sample files are missing. Extract the complete Swell Library ZIP.";
        return false;
    }
    root = folder;
    return true;
}

int SwellBank::findSound (const String& id) const
{
    for (size_t i = 0; i < sounds.size(); ++i)
        if (sounds[i].id == id)
            return (int) i;
    return -1;
}

static bool covers (const LoadedZone& z, int note, int vel)
{
    return note >= z.info.lowKey && note <= z.info.highKey && vel >= z.info.lowVel && vel <= z.info.highVel;
}

// Best zone of one role for note/velocity: exact match, else nearest velocity on the key,
// else nearest key. Restricted to one group when group >= 0.
static const LoadedZone* bestOf (const std::vector<LoadedZone>& zones, ZoneInfo::Role role, int group,
                                 int note, int vel, bool allowStretch)
{
    for (const auto& z : zones)
        if (z.info.role == role && (group < 0 || z.info.group == group) && covers (z, note, vel))
            return &z;

    const LoadedZone* best = nullptr;
    int bestDist = 1 << 20;
    for (const auto& z : zones)
    {
        if (z.info.role != role || (group >= 0 && z.info.group != group)) continue;
        if (note < z.info.lowKey || note > z.info.highKey) continue;
        const int d = vel < z.info.lowVel ? z.info.lowVel - vel : vel - z.info.highVel;
        if (d < bestDist) { bestDist = d; best = &z; }
    }
    if (best != nullptr || ! allowStretch)
        return best;

    for (const auto& z : zones)
    {
        if (z.info.role != role || (group >= 0 && z.info.group != group)) continue;
        const int d = note < z.info.lowKey ? z.info.lowKey - note : note - z.info.highKey;
        if (d < bestDist) { bestDist = d; best = &z; }
    }
    return best;
}

int LoadedInstrument::pickZones (int note, int vel, bool slideArt, bool modUp, unsigned int rr,
                                 const LoadedZone** out, int maxOut) const
{
    int n = 0;
    const LoadedZone* primary = nullptr;

    if (slideArt && hasSlide && vel >= 100)
        primary = bestOf (zones, ZoneInfo::slide, -1, note, 127, false);
    if (primary == nullptr && modUp && hasModWheel)
        primary = bestOf (zones, ZoneInfo::modwheel, -1, note, vel, true);

    if (primary == nullptr)
    {
        // Alternate groups (round-robin takes): every group with an exact main match
        int groups[16], ng = 0;
        for (const auto& z : zones)
        {
            if (z.info.role != ZoneInfo::main || ! covers (z, note, vel)) continue;
            bool seen = false;
            for (int i = 0; i < ng; ++i) seen |= groups[i] == z.info.group;
            if (! seen && ng < 16) groups[ng++] = z.info.group;
        }
        if (ng > 0)
            primary = bestOf (zones, ZoneInfo::main, groups[rr % (unsigned int) ng], note, vel, false);
        if (primary == nullptr)
            primary = bestOf (zones, ZoneInfo::main, -1, note, vel, true);
    }
    if (primary != nullptr && n < maxOut)
        out[n++] = primary;

    if (hasLayers)
    {
        int seenGroups[16], ns = 0;
        for (const auto& z : zones)
        {
            if (z.info.role != ZoneInfo::layer || ! covers (z, note, vel) || n >= maxOut) continue;
            bool seen = false;
            for (int i = 0; i < ns; ++i) seen |= seenGroups[i] == z.info.group;
            if (seen) continue;
            if (ns < 16) seenGroups[ns++] = z.info.group;
            out[n++] = &z;
        }
    }
    return n;
}

const LoadedZone* LoadedInstrument::findZone (int note, int velocity) const
{
    return bestOf (zones, ZoneInfo::main, -1, note, velocity, true);
}

std::shared_ptr<LoadedInstrument> loadInstrument (const SwellBank& bank, int soundIndex,
                                                  const std::atomic<bool>& cancel, String& error)
{
    if (soundIndex < 0 || soundIndex >= (int) bank.sounds.size())
    {
        error = "Son inconnu";
        return nullptr;
    }

    const auto& snd = bank.sounds[(size_t) soundIndex];
    auto inst = std::make_shared<LoadedInstrument>();
    inst->soundIndex = soundIndex;
    inst->soundId = snd.id;
    inst->name = snd.name;
    inst->categoryName = bank.categories[(size_t) snd.category].name;
    inst->oneShot = snd.oneShot;
    inst->octave = snd.octave;
    inst->tune = snd.tune;

    AudioFormatManager formats;
    formats.registerBasicFormats();

    // Samples shared by several zones are decoded once.
    std::map<String, std::shared_ptr<const AudioBuffer<float>>> cache;
    inst->zones.reserve (snd.zones.size());

    for (const auto& zi : snd.zones)
    {
        if (cancel.load())
            return nullptr;
        if (zi.role == ZoneInfo::fx)
            continue; // script-only noises: never played

        LoadedZone zone;
        zone.info = zi;

        if (auto it = cache.find (zi.file); it != cache.end())
        {
            zone.audio = it->second;
            zone.info.numFrames = it->second->getNumSamples() - 4;
        }
        else
        {
            const File f = bank.root.getChildFile (zi.file);
            std::unique_ptr<AudioFormatReader> reader (formats.createReaderFor (f));
            if (reader == nullptr)
            {
                error = "Sample manquant : " + f.getFileName();
                return nullptr;
            }
            const int frames = (int) reader->lengthInSamples;
            auto buf = std::make_shared<AudioBuffer<float>> (2, frames + 4);
            buf->clear();
            reader->read (buf.get(), 0, frames, 0, true, true);
            if (reader->numChannels == 1)
                buf->copyFrom (1, 0, *buf, 0, 0, frames);
            zone.info.numFrames = frames;
            zone.info.sampleRate = (int) reader->sampleRate;
            inst->memoryBytes += (size_t) buf->getNumSamples() * 2 * sizeof (float);
            zone.audio = buf;
            cache[zi.file] = buf;
        }

        if (zone.info.loopStart >= 0 && zone.loopEnd() > zone.info.numFrames)
            zone.info.loopLength = zone.info.numFrames - zone.info.loopStart;

        // Start point: Kontakt offset, then skip any remaining silence (< -60 dBFS),
        // keeping 2 ms of lead-in so attacks stay intact.
        {
            const auto& a = *zone.audio;
            int st = jlimit (0, jmax (0, zone.info.numFrames - 1), zone.info.start);
            const int limit = zone.looped() ? zone.info.loopStart : zone.info.numFrames / 2;
            int i = st;
            while (i < limit && std::abs (a.getSample (0, i)) < 0.001f && std::abs (a.getSample (1, i)) < 0.001f)
                ++i;
            if (i < limit)
                st = jmax (st, i - zone.info.sampleRate / 500);
            zone.playStart = st;
        }
        switch (zone.info.role)
        {
            case ZoneInfo::slide:    inst->hasSlide = true; break;
            case ZoneInfo::modwheel: inst->hasModWheel = true; break;
            case ZoneInfo::layer:    inst->hasLayers = true; break;
            default: break;
        }

        inst->zones.push_back (std::move (zone));
    }

    // Display data
    std::set<int> roots, layers;
    for (const auto& z : inst->zones)
    {
        if (z.info.role != ZoneInfo::main) continue;
        roots.insert (z.info.root);
        if (z.info.lowKey <= 60 && z.info.highKey >= 60)
            layers.insert (z.info.lowVel);
        if (z.looped())
            ++inst->numLoops;
    }
    inst->rootKeys.assign (roots.begin(), roots.end());

    // Round-robin = two main zones of different groups covering the same key and velocity
    for (size_t a = 0; a < inst->zones.size() && ! inst->roundRobin; ++a)
        for (size_t b = a + 1; b < inst->zones.size(); ++b)
        {
            const auto& za = inst->zones[a].info;
            const auto& zb = inst->zones[b].info;
            if (za.role == ZoneInfo::main && zb.role == ZoneInfo::main && za.group != zb.group
                && za.lowKey <= zb.highKey && zb.lowKey <= za.highKey && za.lowVel <= zb.highVel && zb.lowVel <= za.highVel)
            { inst->roundRobin = true; break; }
        }
    inst->velocityLayers = jmax (1, (int) layers.size());

    if (const auto* z = inst->findZone (60, 100))
    {
        inst->displaySampleRate = z->info.sampleRate;
        const int off = z->playStart;
        const int n = z->info.numFrames - off;
        const int points = 240;
        inst->peaks.assign (points, 0.0f);
        for (int p = 0; p < points; ++p)
        {
            const int s0 = off + (int) ((int64) n * p / points);
            const int s1 = jmax (s0 + 1, off + (int) ((int64) n * (p + 1) / points));
            float m = 0.0f;
            for (int s = s0; s < s1; s += 4)
                m = jmax (m, std::abs (z->audio->getSample (0, s)), std::abs (z->audio->getSample (1, s)));
            inst->peaks[(size_t) p] = m;
        }
        const float mx = *std::max_element (inst->peaks.begin(), inst->peaks.end());
        if (mx > 0.0f)
            for (auto& v : inst->peaks) v /= mx;
    }
    return inst;
}
} // namespace swell
