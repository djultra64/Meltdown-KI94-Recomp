#include "ki/frame_asset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint32_t asset_address = UINT32_C(0x88090100);
static const uint32_t main_ram_physical_base = UINT32_C(0x08000000);
static const uint8_t expected_sha256[32] = {
    0xca,0x38,0x0a,0x6c,0xfb,0xef,0xd0,0x93,0xc7,0x4a,0x7c,0xcc,0x82,0x82,0x7d,0x0a,
    0xf3,0xd9,0x1e,0x23,0x39,0x8a,0x9f,0x1b,0x0f,0x14,0x90,0xf3,0x5f,0x66,0x14,0x5e
};

static uint32_t rotate_right(uint32_t value, unsigned shift)
{
    return (value >> shift) | (value << (32u - shift));
}

static void sha256(const uint8_t *data, size_t size, uint8_t digest[32])
{
    static const uint32_t constants[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t state[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                         0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    const uint64_t bit_size = (uint64_t)size * 8u;
    const size_t padded = ((size + 9u + 63u) / 64u) * 64u;
    for (size_t block = 0; block < padded; block += 64) {
        uint32_t words[64];
        for (unsigned i = 0; i < 16; ++i) {
            uint32_t word = 0;
            for (unsigned j = 0; j < 4; ++j) {
                const size_t at = block + i * 4u + j;
                uint8_t byte = at < size ? data[at] : at == size ? 0x80 : 0;
                if (at >= padded - 8u) byte = (uint8_t)(bit_size >> ((padded - 1u - at) * 8u));
                word = (word << 8) | byte;
            }
            words[i] = word;
        }
        for (unsigned i = 16; i < 64; ++i) {
            const uint32_t a = words[i - 15], b = words[i - 2];
            const uint32_t s0 = rotate_right(a,7)^rotate_right(a,18)^(a>>3);
            const uint32_t s1 = rotate_right(b,17)^rotate_right(b,19)^(b>>10);
            words[i] = words[i-16] + s0 + words[i-7] + s1;
        }
        uint32_t a=state[0],b=state[1],c=state[2],d=state[3];
        uint32_t e=state[4],f=state[5],g=state[6],h=state[7];
        for (unsigned i = 0; i < 64; ++i) {
            const uint32_t s1=rotate_right(e,6)^rotate_right(e,11)^rotate_right(e,25);
            const uint32_t t1=h+s1+((e&f)^((~e)&g))+constants[i]+words[i];
            const uint32_t s0=rotate_right(a,2)^rotate_right(a,13)^rotate_right(a,22);
            const uint32_t t2=s0+((a&b)^(a&c)^(b&c));
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
        state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
    }
    for (unsigned i = 0; i < 32; ++i)
        digest[i] = (uint8_t)(state[i/4] >> (24u - (i%4u)*8u));
}

KiFrameAssetResult ki_frame_asset_load_bytes(KiMemory *memory,
                                             const uint8_t *bytes,
                                             size_t size)
{
    uint8_t digest[32];
    if (memory == NULL || bytes == NULL) return KI_FRAME_ASSET_ARGUMENT;
    if (size != KI_FRAME_ASSET_SIZE) return KI_FRAME_ASSET_SIZE_ERROR;
    sha256(bytes, size, digest);
    if (memcmp(digest, expected_sha256, sizeof(digest)) != 0)
        return KI_FRAME_ASSET_IDENTITY;
    const uint32_t physical = ki_physical_address(asset_address);
    if (memory->main_ram == NULL || physical < main_ram_physical_base ||
        (size_t)(physical - main_ram_physical_base) + size > memory->main_ram_size)
        return KI_FRAME_ASSET_MEMORY;
    memcpy(memory->main_ram + physical - main_ram_physical_base, bytes, size);
    return KI_FRAME_ASSET_OK;
}

KiFrameAssetResult ki_frame_asset_load(KiMemory *memory, const char *path)
{
    uint8_t *bytes;
    FILE *file;
    size_t got;
    int extra;
    KiFrameAssetResult result;
    if (memory == NULL || path == NULL) return KI_FRAME_ASSET_ARGUMENT;
    file = fopen(path, "rb");
    if (file == NULL) return KI_FRAME_ASSET_IO;
    bytes = malloc(KI_FRAME_ASSET_SIZE);
    if (bytes == NULL) { fclose(file); return KI_FRAME_ASSET_IO; }
    got = fread(bytes, 1, KI_FRAME_ASSET_SIZE, file);
    extra = fgetc(file);
    if (ferror(file)) result = KI_FRAME_ASSET_IO;
    else if (got != KI_FRAME_ASSET_SIZE || extra != EOF) result = KI_FRAME_ASSET_SIZE_ERROR;
    else result = ki_frame_asset_load_bytes(memory, bytes, got);
    free(bytes);
    fclose(file);
    return result;
}
