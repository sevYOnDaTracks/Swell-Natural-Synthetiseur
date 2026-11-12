#pragma once

// Decoder for Native Instruments NCW compressed samples (lossless, 512-frame blocks).
// Block modes: delta (bits > 0), absolute (bits < 0), raw PCM (bits == 0).
// Stereo blocks flagged 1 are mid/side coded: L = M + S, R = M - S.

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iterator>

namespace swell
{
struct NcwAudio
{
    int channels = 0;
    int bitsPerSample = 0;
    int sampleRate = 0;
    int numFrames = 0;
    std::vector<std::vector<float>> data; // [channel][frame], normalised to +-1
};

class NcwDecoder
{
public:
    static bool decodeFile (const std::string& path, NcwAudio& out, std::string& error)
    {
        std::ifstream f (path, std::ios::binary);
        if (! f) { error = "cannot open " + path; return false; }
        std::vector<uint8_t> bytes ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
        return decode (bytes.data(), bytes.size(), out, error);
    }

    static bool decode (const uint8_t* d, size_t size, NcwAudio& out, std::string& error)
    {
        if (size < 120) { error = "file too small"; return false; }
        const uint32_t sigA = rd32 (d), sigB = rd32 (d + 4);
        if (sigA != 0xD69EA801u || (sigB != 0x0131u && sigB != 0x0130u)) { error = "not an NCW file"; return false; }

        const int channels   = rd16 (d + 8);
        const int bits       = rd16 (d + 10);
        const int sampleRate = (int) rd32 (d + 12);
        const int numFrames  = (int) rd32 (d + 16);
        const uint32_t blockDefOffset = rd32 (d + 20);
        const uint32_t blocksOffset   = rd32 (d + 24);

        if (channels < 1 || channels > 8 || (bits != 16 && bits != 24 && bits != 32) || numFrames <= 0
            || blocksOffset > size || blockDefOffset > blocksOffset)
        { error = "bad NCW header"; return false; }

        const int numBlocks = (numFrames + 511) / 512;
        if (blockDefOffset + (size_t) numBlocks * 4 > size) { error = "bad block table"; return false; }

        out.channels = channels;
        out.bitsPerSample = bits;
        out.sampleRate = sampleRate;
        out.numFrames = numFrames;
        out.data.assign ((size_t) channels, std::vector<float> ((size_t) numFrames, 0.0f));

        const float scale = 1.0f / (float) (1u << (bits - 1));
        std::vector<int32_t> block ((size_t) channels * 512);
        bool midSide = false;

        for (int b = 0; b < numBlocks; ++b)
        {
            size_t p = blocksOffset + rd32 (d + blockDefOffset + (size_t) b * 4);
            midSide = false;

            for (int c = 0; c < channels; ++c)
            {
                if (p + 16 > size || rd32 (d + p) != 0x3E9A0C16u)
                {
                    // Damaged tail (seen in some rips): keep the valid part and fade it out.
                    if (b == 0) { error = "bad block signature"; return false; }
                    finishTruncated (out, b * 512);
                    return true;
                }
                const int32_t base  = (int32_t) rd32 (d + p + 4);
                const int16_t nbits = (int16_t) rd16 (d + p + 8);
                const uint16_t flags = rd16 (d + p + 10);
                p += 16;
                if (c == 0) midSide = (flags == 1);

                int32_t* s = block.data() + (size_t) c * 512;
                const int width = nbits > 0 ? nbits : (nbits < 0 ? -nbits : bits);
                const size_t len = (size_t) 512 * (size_t) width / 8;
                if (width > 32 || p + len > size)
                {
                    if (b == 0) { error = "truncated block data"; return false; }
                    finishTruncated (out, b * 512);
                    return true;
                }

                if (nbits > 0)
                {
                    s[0] = base;
                    BitReader br (d + p);
                    for (int i = 1; i < 512; ++i)
                        s[i] = s[i - 1] + br.readSigned (width);
                }
                else
                {
                    BitReader br (d + p);
                    for (int i = 0; i < 512; ++i)
                        s[i] = br.readSigned (width);
                }
                p += len;
            }

            const int start = b * 512;
            const int count = std::min (512, numFrames - start);

            if (midSide && channels == 2)
            {
                const int32_t* m  = block.data();
                const int32_t* sd = block.data() + 512;
                float* l = out.data[0].data() + start;
                float* r = out.data[1].data() + start;
                for (int i = 0; i < count; ++i)
                {
                    l[i] = (float) ((int64_t) m[i] + sd[i]) * scale;
                    r[i] = (float) ((int64_t) m[i] - sd[i]) * scale;
                }
            }
            else
            {
                for (int c = 0; c < channels; ++c)
                {
                    const int32_t* s = block.data() + (size_t) c * 512;
                    float* o = out.data[(size_t) c].data() + start;
                    for (int i = 0; i < count; ++i)
                        o[i] = (float) s[i] * scale;
                }
            }
        }
        return true;
    }

private:
    static void finishTruncated (NcwAudio& out, int validEnd)
    {
        const int fade = std::min (256, validEnd);
        for (auto& ch : out.data)
        {
            for (int i = 0; i < fade; ++i)
                ch[(size_t) (validEnd - fade + i)] *= 1.0f - (float) (i + 1) / (float) fade;
            std::fill (ch.begin() + validEnd, ch.end(), 0.0f);
        }
    }

    static uint32_t rd32 (const uint8_t* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); }
    static uint16_t rd16 (const uint8_t* p) { return (uint16_t) (p[0] | (p[1] << 8)); }

    struct BitReader
    {
        explicit BitReader (const uint8_t* p) : data (p) {}

        int32_t readSigned (int width)
        {
            while (avail < width)
            {
                acc |= (uint64_t) data[pos++] << avail;
                avail += 8;
            }
            const uint64_t mask = (width == 64) ? ~0ull : ((1ull << width) - 1);
            uint64_t v = acc & mask;
            acc >>= width;
            avail -= width;
            if (v & (1ull << (width - 1)))
                v |= ~mask;
            return (int32_t) (int64_t) v;
        }

        const uint8_t* data;
        size_t pos = 0;
        uint64_t acc = 0;
        int avail = 0;
    };
};
} // namespace swell
