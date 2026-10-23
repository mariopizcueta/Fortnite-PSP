#include "audio/sound_data.h"

#include <string.h>

namespace audio {
namespace {

const int kSteps[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,
    25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,
    88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,   253,   279,
    307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,   876,   963,
    1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,  3327,
    3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
const int kIndexTable[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

u32 readU32(const u8* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<u32>(p[3]) << 24); }
u16 readU16(const u8* p) { return static_cast<u16>(p[0] | (p[1] << 8)); }

}  // namespace

s16 imaDecodeNibble(int code, int* predictor, int* index) {
    const int step = kSteps[*index];
    int diff = step >> 3;
    if (code & 4) diff += step;
    if (code & 2) diff += step >> 1;
    if (code & 1) diff += step >> 2;
    int p = *predictor + ((code & 8) ? -diff : diff);
    if (p > 32767) p = 32767;
    if (p < -32768) p = -32768;
    *predictor = p;
    int i = *index + kIndexTable[code & 7];
    *index = i < 0 ? 0 : (i > 88 ? 88 : i);
    return static_cast<s16>(p);
}

bool parseWav(const void* data, size_t size, SoundClip* out) {
    const u8* p = static_cast<const u8*>(data);
    if (size < 44 || memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0) return false;
    size_t offset = 12;
    bool formatOk = false;
    while (offset + 8 <= size) {
        const u8* chunk = p + offset;
        const u32 length = readU32(chunk + 4);
        if (memcmp(chunk, "fmt ", 4) == 0 && length >= 16) {
            const u16 format = readU16(chunk + 8), channels = readU16(chunk + 10);
            const u16 bits = readU16(chunk + 22);
            out->rate = static_cast<int>(readU32(chunk + 12));
            formatOk = format == 1 && channels == 1 && bits == 16;
        } else if (memcmp(chunk, "data", 4) == 0) {
            // Los s16 deben estar alineados a 2 bytes (la CPU de la PSP no lee desalineado).
            if (!formatOk || ((offset + 8) & 1) || offset + 8 + length > size) return false;
            out->samples = reinterpret_cast<const s16*>(chunk + 8);
            out->count = static_cast<int>(length / 2);
            return true;
        }
        offset += 8 + length + (length & 1);
    }
    return false;
}

bool ImaStream::open(const void* data, size_t size) {
    const u8* p = static_cast<const u8*>(data);
    m_data = nullptr;
    if (size < 20 || memcmp(p, "IMA1", 4) != 0) return false;
    m_rate = static_cast<int>(readU32(p + 4));
    m_total = static_cast<int>(readU32(p + 8));
    m_blockBytes = static_cast<int>(readU32(p + 12));
    m_samplesPerBlock = static_cast<int>(readU32(p + 16));
    if (m_samplesPerBlock <= 0 || m_samplesPerBlock > 4096 || m_blockBytes < 4) return false;
    m_blocks = (m_total + m_samplesPerBlock - 1) / m_samplesPerBlock;
    if (20 + static_cast<size_t>(m_blocks) * m_blockBytes > size) return false;
    m_data = p + 20;
    rewind();
    return true;
}

void ImaStream::rewind() {
    m_position = 0;
    m_blockLoaded = -1;
}

void ImaStream::decodeBlock(int block) {
    const u8* b = m_data + static_cast<size_t>(block) * m_blockBytes;
    int predictor = static_cast<s16>(readU16(b));
    int index = b[2] > 88 ? 88 : b[2];
    m_buffer[0] = static_cast<s16>(predictor);
    for (int i = 1; i < m_samplesPerBlock; ++i) {
        const u8 byte = b[4 + (i - 1) / 2];
        const int code = ((i - 1) & 1) ? (byte >> 4) : (byte & 0x0F);
        m_buffer[i] = imaDecodeNibble(code, &predictor, &index);
    }
    m_blockLoaded = block;
}

s16 ImaStream::next() {
    if (!m_data || m_total <= 0) return 0;
    if (m_position >= m_total) m_position = 0;
    const int block = m_position / m_samplesPerBlock;
    if (block != m_blockLoaded) decodeBlock(block);
    return m_buffer[m_position++ - block * m_samplesPerBlock];
}

}  // namespace audio
