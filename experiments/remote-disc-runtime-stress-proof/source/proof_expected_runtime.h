#ifndef CH_REMOTE_RUNTIME_PROOF_EXPECTED_H
#define CH_REMOTE_RUNTIME_PROOF_EXPECTED_H

#include <stdint.h>

typedef struct ProofWindowExpected
{
    uint32_t offset;
    uint32_t length;
    uint32_t crc32;
} ProofWindowExpected;

typedef struct ProofFileExpected
{
    const char *path;
    uint32_t size;
    uint32_t prefix_length;
    uint32_t prefix_crc32;
    uint32_t suffix_length;
    uint32_t suffix_crc32;
} ProofFileExpected;

#define PROOF_PRIMARY_PATH "data/wad/tnt.wad"
#define PROOF_PRIMARY_SIZE 18195736u
#define PROOF_PRIMARY_CRC32 0x903DCC27u
#define PROOF_STREAM_PASSES 4u
#define PROOF_STREAM_TOTAL_BYTES 72782944ull
#define PROOF_TAIL_LENGTH 4093u
#define PROOF_TAIL_CRC32 0x9B673B63u

#define PROOF_WINDOW_COUNT 32u

static const ProofWindowExpected proof_windows[] =
{
    { 16953357u, 1u, 0xD202EF8Du },
    { 8611356u, 31u, 0x6D2ED61Eu },
    { 12108903u, 257u, 0xF7522684u },
    { 11747334u, 1023u, 0x49D93CC0u },
    { 1266937u, 4093u, 0xEE269B45u },
    { 2993268u, 8191u, 0xB51BAB5Du },
    { 7775275u, 16381u, 0xDE500025u },
    { 16413822u, 32749u, 0x46AFC000u },
    { 14713509u, 1u, 0x360C2086u },
    { 3291132u, 31u, 0x020AAD57u },
    { 9014751u, 257u, 0x184D6E1Eu },
    { 3182972u, 1023u, 0x65C8B780u },
    { 11781941u, 4093u, 0xD7167CC7u },
    { 15245176u, 8191u, 0x7BDD2290u },
    { 1332191u, 16381u, 0x683E1D57u },
    { 2025606u, 32749u, 0x6B9232CFu },
    { 17415101u, 1u, 0x73D37CF3u },
    { 8429140u, 31u, 0x8B4E04FAu },
    { 17934231u, 257u, 0x78D2F186u },
    { 4060736u, 1023u, 0x41A44979u },
    { 15236609u, 4093u, 0xA346DD22u },
    { 1319584u, 8191u, 0x222BCB96u },
    { 7500359u, 16381u, 0x9F49F444u },
    { 17399946u, 32749u, 0xD124EED2u },
    { 10973149u, 1u, 0x550A4C5Fu },
    { 10027018u, 31u, 0xB28B8364u },
    { 13484351u, 257u, 0x9CC3A294u },
    { 10298486u, 1023u, 0x75BABB42u },
    { 11463897u, 4093u, 0x6E078B2Fu },
    { 2602832u, 8191u, 0x9A42592Du },
    { 281743u, 16381u, 0x918AEC8Du },
    { 4175946u, 32749u, 0x2F7870D0u },
};

#define PROOF_FILE_COUNT 4u

static const ProofFileExpected proof_files[] =
{
    { "data/wad/plutonia.wad", 17420824u, 4093u, 0x6C6AF5EBu, 2053u, 0xFE188812u },
    { "data/timidity/instruments/belltree.pat", 66187u, 4093u, 0x77938C56u, 2053u, 0x64811064u },
    { "data/timidity/instruments/orchhit.pat", 28751u, 4093u, 0x6722F92Cu, 2053u, 0x620DAB54u },
    { "assets/controller/gamecube/toomai/bmp/ButtonIcon-GCN-R-P.bmp", 16506u, 4093u, 0x2A666E60u, 2053u, 0x708D9176u },
};

#endif
