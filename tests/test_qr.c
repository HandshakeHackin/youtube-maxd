// QR encoder checks through a decoder-side reader written for this test: it
// has its own geometry, block layout, mask and field arithmetic tables, so a
// symbol passes only when both descriptions of the standard agree.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tube_qr.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* ---- Independent tables (ISO/IEC 18004 tables 1, 7, 9, C.1, D.1, E.1) ---- */
static const int k_total[11] = {0, 26, 44, 70, 100, 134, 172, 196, 242, 292, 346};
static const int k_remainder[11] = {0, 0, 7, 7, 7, 7, 7, 0, 0, 0, 0};
static const int k_align_count[11] = {0, 0, 2, 2, 2, 2, 2, 3, 3, 3, 3};
static const int k_align[11][3] = {{0}, {0}, {6, 18}, {6, 22}, {6, 26}, {6, 30}, {6, 34},
                                   {6, 22, 38}, {6, 24, 42}, {6, 26, 46}, {6, 28, 50}};
/* {check bytes per block, blocks, data bytes each, further blocks, data bytes each} */
static const int k_blocks[11][4][5] = {
    {{0}},
    {{7, 1, 19, 0, 0}, {10, 1, 16, 0, 0}, {13, 1, 13, 0, 0}, {17, 1, 9, 0, 0}},
    {{10, 1, 34, 0, 0}, {16, 1, 28, 0, 0}, {22, 1, 22, 0, 0}, {28, 1, 16, 0, 0}},
    {{15, 1, 55, 0, 0}, {26, 1, 44, 0, 0}, {18, 2, 17, 0, 0}, {22, 2, 13, 0, 0}},
    {{20, 1, 80, 0, 0}, {18, 2, 32, 0, 0}, {26, 2, 24, 0, 0}, {16, 4, 9, 0, 0}},
    {{26, 1, 108, 0, 0}, {24, 2, 43, 0, 0}, {18, 2, 15, 2, 16}, {22, 2, 11, 2, 12}},
    {{18, 2, 68, 0, 0}, {16, 4, 27, 0, 0}, {24, 4, 19, 0, 0}, {28, 4, 15, 0, 0}},
    {{20, 2, 78, 0, 0}, {18, 4, 31, 0, 0}, {18, 2, 14, 4, 15}, {26, 4, 13, 1, 14}},
    {{24, 2, 97, 0, 0}, {22, 2, 38, 2, 39}, {22, 4, 18, 2, 19}, {26, 4, 14, 2, 15}},
    {{30, 2, 116, 0, 0}, {22, 3, 36, 2, 37}, {20, 4, 16, 4, 17}, {24, 4, 12, 4, 13}},
    {{18, 2, 68, 2, 69}, {26, 4, 43, 1, 44}, {24, 6, 19, 2, 20}, {28, 6, 15, 2, 16}}};
static const size_t k_capacity[4][10] = {
    {17, 32, 53, 78, 106, 134, 154, 192, 230, 271},
    {14, 26, 42, 62, 84, 106, 122, 152, 180, 213},
    {11, 20, 32, 46, 60, 74, 86, 108, 130, 151},
    {7, 14, 24, 34, 44, 58, 64, 84, 98, 119}};
/* Format words by level (L, M, Q, H) and mask. */
static const unsigned k_format[4][8] = {
    {0x77C4, 0x72F3, 0x7DAA, 0x789D, 0x662F, 0x6318, 0x6C41, 0x6976},
    {0x5412, 0x5125, 0x5E7C, 0x5B4B, 0x45F9, 0x40CE, 0x4F97, 0x4AA0},
    {0x355F, 0x3068, 0x3F31, 0x3A06, 0x24B4, 0x2183, 0x2EDA, 0x2BED},
    {0x1689, 0x13BE, 0x1CE7, 0x19D0, 0x0762, 0x0255, 0x0D0C, 0x083B}};
static const unsigned k_version_word[11] = {0, 0, 0, 0, 0, 0, 0, 0x07C94, 0x085BC, 0x09A99, 0x0A4D3};

static uint8_t gf_exp[512], gf_log[256];

static void gf_init(void) {
    unsigned value = 1;
    for (int i = 0; i < 255; i++) {
        gf_exp[i] = (uint8_t)value; gf_log[value] = (uint8_t)i;
        value <<= 1;
        if (value & 0x100) value ^= 0x11D;
    }
    for (int i = 255; i < 512; i++) gf_exp[i] = gf_exp[i - 255];
}

static int dark(const tube_qr_code *code, int x, int y) { return code->modules[y * code->size + x]; }

static int is_function(int version, int x, int y) {
    int size = 17 + 4 * version;
    if (x < 9 && y < 9) return 1;
    if (x >= size - 8 && y < 9) return 1;
    if (x < 9 && y >= size - 8) return 1;
    if (x == 6 || y == 6) return 1;
    for (int a = 0; a < k_align_count[version]; a++) for (int b = 0; b < k_align_count[version]; b++) {
        int cx = k_align[version][a], cy = k_align[version][b];
        if ((cx == 6 && cy == 6) || (cx == 6 && cy == size - 7) || (cx == size - 7 && cy == 6)) continue;
        if (abs(x - cx) <= 2 && abs(y - cy) <= 2) return 1;
    }
    if (version >= 7 && ((x >= size - 11 && x < size - 8 && y < 6) || (y >= size - 11 && y < size - 8 && x < 6))) return 1;
    return 0;
}

static int reader_mask(int mask, int row, int column) {
    switch (mask) {
    case 0: return (row + column) % 2 == 0;
    case 1: return row % 2 == 0;
    case 2: return column % 3 == 0;
    case 3: return (row + column) % 3 == 0;
    case 4: return (row / 2 + column / 3) % 2 == 0;
    case 5: return (row * column) % 2 + (row * column) % 3 == 0;
    case 6: return ((row * column) % 2 + (row * column) % 3) % 2 == 0;
    case 7: return ((row + column) % 2 + (row * column) % 3) % 2 == 0;
    }
    return 0;
}

static unsigned polynomial_rest(unsigned value, unsigned generator, int generator_bits, int value_bits) {
    for (int bit = value_bits - 1; bit >= generator_bits - 1; bit--)
        if (value & (1u << bit)) value ^= generator << (bit - (generator_bits - 1));
    return value;
}

enum { READ_OK = 0, READ_STRUCTURE, READ_FORMAT, READ_VERSION, READ_COUNT, READ_SYNDROME, READ_PAYLOAD };

/* Reads a symbol back to its payload. Returns READ_OK or the failed stage. */
static int read_symbol(const tube_qr_code *code, uint8_t *payload, size_t *length) {
    static const char *finder[7] = {"1111111", "1000001", "1011101", "1011101", "1011101", "1000001", "1111111"};
    int version = code->version, size = code->size;
    if (version < 1 || version > 10 || size != 17 + 4 * version) return READ_STRUCTURE;

    /* Finder patterns with their light separators, timing lines, dark module. */
    for (int corner = 0; corner < 3; corner++) {
        int ox = corner == 1 ? size - 7 : 0, oy = corner == 2 ? size - 7 : 0;
        for (int y = -1; y <= 7; y++) for (int x = -1; x <= 7; x++) {
            int px = ox + x, py = oy + y;
            if (px < 0 || py < 0 || px >= size || py >= size) continue;
            int expected = x >= 0 && x < 7 && y >= 0 && y < 7 && finder[y][x] == '1';
            if (dark(code, px, py) != expected) return READ_STRUCTURE;
        }
    }
    for (int i = 8; i < size - 8; i++)
        if (dark(code, i, 6) != (i % 2 == 0) || dark(code, 6, i) != (i % 2 == 0)) return READ_STRUCTURE;
    if (!dark(code, 8, 4 * version + 9)) return READ_STRUCTURE;
    for (int a = 0; a < k_align_count[version]; a++) for (int b = 0; b < k_align_count[version]; b++) {
        int cx = k_align[version][a], cy = k_align[version][b];
        if ((cx == 6 && cy == 6) || (cx == 6 && cy == size - 7) || (cx == size - 7 && cy == 6)) continue;
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
            int expected = abs(dx) == 2 || abs(dy) == 2 || (dx == 0 && dy == 0);
            if (dark(code, cx + dx, cy + dy) != expected) return READ_STRUCTURE;
        }
    }

    /* Both format copies, least significant bit first. */
    static const int first[15][2] = {{8, 0}, {8, 1}, {8, 2}, {8, 3}, {8, 4}, {8, 5}, {8, 7}, {8, 8},
                                     {7, 8}, {5, 8}, {4, 8}, {3, 8}, {2, 8}, {1, 8}, {0, 8}};
    unsigned format_a = 0, format_b = 0;
    for (int i = 0; i < 15; i++) {
        format_a |= (unsigned)dark(code, first[i][0], first[i][1]) << i;
        format_b |= (unsigned)(i < 8 ? dark(code, size - 1 - i, 8) : dark(code, 8, size - 15 + i)) << i;
    }
    if (format_a != format_b || polynomial_rest(format_a ^ 0x5412, 0x537, 11, 15)) return READ_FORMAT;
    static const int level_of[4] = {TUBE_QR_LEVEL_M, TUBE_QR_LEVEL_L, TUBE_QR_LEVEL_H, TUBE_QR_LEVEL_Q};
    int level = level_of[(format_a ^ 0x5412) >> 13], mask = (int)(((format_a ^ 0x5412) >> 10) & 7);
    if (level != (int)code->level || mask != code->mask || format_a != k_format[level][mask]) return READ_FORMAT;

    if (version >= 7) {
        unsigned top = 0, bottom = 0;
        for (int i = 0; i < 18; i++) {
            top |= (unsigned)dark(code, size - 11 + i % 3, i / 3) << i;
            bottom |= (unsigned)dark(code, i / 3, size - 11 + i % 3) << i;
        }
        if (top != bottom || polynomial_rest(top, 0x1F25, 13, 18) || (int)(top >> 12) != version ||
            top != k_version_word[version])
            return READ_VERSION;
    }

    /* Unmasked data bits in placement order. */
    static uint8_t bits[TUBE_QR_MAX_SIZE * TUBE_QR_MAX_SIZE];
    int count = 0, column = size - 1, row = size - 1, direction = -1;
    while (column > 0) {
        if (column == 6) column--;
        for (;;) {
            for (int side = 0; side < 2; side++) {
                int x = column - side;
                if (!is_function(version, x, row)) bits[count++] = (uint8_t)(dark(code, x, row) ^ reader_mask(mask, row, x));
            }
            if (row + direction < 0 || row + direction >= size) { direction = -direction; break; }
            row += direction;
        }
        column -= 2;
    }
    if (count != k_total[version] * 8 + k_remainder[version]) return READ_COUNT;
    for (int i = k_total[version] * 8; i < count; i++) if (bits[i]) return READ_COUNT;

    uint8_t words[346];
    for (int i = 0; i < k_total[version]; i++) {
        words[i] = 0;
        for (int bit = 0; bit < 8; bit++) words[i] = (uint8_t)(words[i] << 1 | bits[i * 8 + bit]);
    }

    /* De-interleave, then every block must be a Reed-Solomon code word. */
    const int *layout = k_blocks[version][level];
    int check = layout[0], blocks = layout[1] + layout[3], at = 0, longest = layout[3] ? layout[4] : layout[2];
    uint8_t block[8][160];
    int data_length[8];
    for (int b = 0; b < blocks; b++) data_length[b] = b < layout[1] ? layout[2] : layout[4];
    for (int i = 0; i < longest; i++) for (int b = 0; b < blocks; b++) if (i < data_length[b]) block[b][i] = words[at++];
    for (int i = 0; i < check; i++) for (int b = 0; b < blocks; b++) block[b][data_length[b] + i] = words[at++];
    if (at != k_total[version]) return READ_COUNT;
    for (int b = 0; b < blocks; b++)
        for (int power = 0; power < check; power++) {
            unsigned sum = 0;
            for (int i = 0; i < data_length[b] + check; i++) {
                if (sum) sum = gf_exp[gf_log[sum] + power];
                sum ^= block[b][i];
            }
            if (sum) return READ_SYNDROME;
        }

    /* Byte-mode segment: mode, count, bytes, terminator, alternating pads. */
    uint8_t stream[346];
    int total = 0;
    for (int b = 0; b < blocks; b++) { memcpy(stream + total, block[b], (size_t)data_length[b]); total += data_length[b]; }
    int position = 0;
#define TAKE(width, into) do { unsigned v_ = 0; for (int n_ = 0; n_ < (width); n_++, position++) { \
        if (position >= total * 8) return READ_PAYLOAD; \
        v_ = v_ << 1 | ((stream[position >> 3] >> (7 - (position & 7))) & 1u); } (into) = v_; } while (0)
    unsigned mode, declared, value;
    TAKE(4, mode);
    if (mode != 4) return READ_PAYLOAD;
    TAKE(version < 10 ? 8 : 16, declared);
    for (unsigned i = 0; i < declared; i++) { TAKE(8, value); payload[i] = (uint8_t)value; }
    TAKE(4, value);
    if (value || position % 8) return READ_PAYLOAD;
    for (int i = position / 8, pad = 0; i < total; i++, pad ^= 1) if (stream[i] != (pad ? 0x11 : 0xEC)) return READ_PAYLOAD;
#undef TAKE
    *length = declared;
    return READ_OK;
}

static uint32_t g_seed = 0x2545F491u;
static uint8_t next_byte(void) {
    g_seed ^= g_seed << 13; g_seed ^= g_seed >> 17; g_seed ^= g_seed << 5;
    return (uint8_t)(g_seed >> 11);
}

static tube_qr_code g_code;
static void round_trip(size_t length, tube_qr_level level, int min_version, int mask, int expected_version) {
    uint8_t input[TUBE_QR_MAX_PAYLOAD + 1], output[512];
    size_t read = 999;
    for (size_t i = 0; i < length; i++) input[i] = next_byte();
    if (length > 2) { input[0] = 0; input[length - 1] = 0xFF; }
    assert(tube_qr_encode_ex(input, length, level, min_version, mask, &g_code) == 0);
    assert(g_code.version == expected_version && g_code.size == 17 + 4 * expected_version && g_code.level == level);
    assert(mask == TUBE_QR_MASK_AUTO ? g_code.mask >= 0 && g_code.mask <= 7 : g_code.mask == mask);
    int status = read_symbol(&g_code, output, &read);
    if (status != READ_OK) {
        fprintf(stderr, "version %d level %d mask %d length %zu failed at stage %d\n", expected_version, (int)level, g_code.mask, length, status);
        abort();
    }
    assert(read == length && !memcmp(input, output, length));
}

int main(void) {
    gf_init();

    /* Table agreement: totals, block layouts, capacities, data module counts. */
    for (int version = 1; version <= 10; version++) {
        int free_modules = 0, size = 17 + 4 * version;
        for (int y = 0; y < size; y++) for (int x = 0; x < size; x++) free_modules += !is_function(version, x, y);
        assert(free_modules == k_total[version] * 8 + k_remainder[version]);
        assert(tube_qr_total_codewords(version) == k_total[version]);
        for (int level = 0; level < 4; level++) {
            const int *layout = k_blocks[version][level];
            assert(layout[1] * layout[2] + layout[3] * layout[4] + layout[0] * (layout[1] + layout[3]) == k_total[version]);
            assert(tube_qr_ec_per_block(version, (tube_qr_level)level) == layout[0]);
            assert(tube_qr_block_count(version, (tube_qr_level)level) == layout[1] + layout[3]);
            assert(tube_qr_capacity(version, (tube_qr_level)level) == k_capacity[level][version - 1]);
        }
    }
    assert(tube_qr_capacity(0, TUBE_QR_LEVEL_M) == 0 && tube_qr_capacity(11, TUBE_QR_LEVEL_M) == 0);
    assert(tube_qr_capacity(1, (tube_qr_level)4) == 0 && TUBE_QR_MAX_PAYLOAD == 271 && TUBE_QR_MAX_SIZE == 57);

    /* Published Reed-Solomon vector: version 1-M "HELLO WORLD" data words. */
    {
        static const uint8_t data[16] = {32, 91, 11, 120, 209, 114, 220, 77, 67, 64, 236, 17, 236, 17, 236, 17};
        static const uint8_t expected[10] = {196, 35, 39, 119, 235, 215, 231, 226, 93, 23};
        uint8_t check[10];
        tube_qr_rs_remainder(data, 16, 10, check);
        assert(!memcmp(check, expected, 10));
    }

    /* Every version and level at its boundaries, automatic mask. */
    for (int level = 0; level < 4; level++) {
        round_trip(0, (tube_qr_level)level, 1, TUBE_QR_MASK_AUTO, 1);
        round_trip(1, (tube_qr_level)level, 1, TUBE_QR_MASK_AUTO, 1);
        for (int version = 1; version <= 10; version++) {
            round_trip(k_capacity[level][version - 1], (tube_qr_level)level, 1, TUBE_QR_MASK_AUTO, version);
            round_trip(k_capacity[level][version - 1] - 1, (tube_qr_level)level, 1, TUBE_QR_MASK_AUTO, version);
            if (version > 1) round_trip(k_capacity[level][version - 2] + 1, (tube_qr_level)level, 1, TUBE_QR_MASK_AUTO, version);
            /* A short payload in a larger symbol exercises the pad words. */
            round_trip(3, (tube_qr_level)level, version, TUBE_QR_MASK_AUTO, version);
        }
        uint8_t long_input[TUBE_QR_MAX_PAYLOAD + 2] = {0};
        assert(tube_qr_encode(long_input, k_capacity[level][9] + 1, (tube_qr_level)level, &g_code) == -1);
        assert(g_code.size == 0 && g_code.version == 0);
    }

    /* Every mask on every version; the format word is checked by the reader. */
    for (int version = 1; version <= 10; version++) for (int mask = 0; mask < 8; mask++) for (int level = 0; level < 4; level++)
        round_trip(k_capacity[level][version - 1] / 2, (tube_qr_level)level, version, mask, version);

    /* Automatic selection takes the first mask with the lowest penalty. */
    for (int trial = 0; trial < 12; trial++) {
        uint8_t input[100];
        size_t length = 10 + (size_t)trial * 7;
        tube_qr_code forced, chosen;
        long best = -1; int best_mask = -1;
        for (size_t i = 0; i < length; i++) input[i] = next_byte();
        for (int mask = 0; mask < 8; mask++) {
            assert(tube_qr_encode_ex(input, length, TUBE_QR_LEVEL_M, 1, mask, &forced) == 0);
            long penalty = tube_qr_penalty(&forced);
            assert(penalty > 0);
            if (best < 0 || penalty < best) { best = penalty; best_mask = mask; }
        }
        assert(tube_qr_encode(input, length, TUBE_QR_LEVEL_M, &chosen) == 0 && chosen.mask == best_mask);
        assert(tube_qr_penalty(&chosen) == best);
    }

    /* Penalty rules on hand-computed 21x21 grids. */
    {
        tube_qr_code grid;
        memset(&grid, 0, sizeof grid);
        assert(tube_qr_penalty(&grid) == -1 && tube_qr_penalty(NULL) == -1);
        grid.version = 1; grid.size = 21;
        /* All light: 42 runs of 21 (19 each), 400 blocks, dark share 0%. */
        assert(tube_qr_penalty(&grid) == 42 * 19 + 400 * 3 + 100);
        /* One finder-like run in row 10 with four light modules on each side. */
        static const int pattern[5] = {4, 6, 7, 8, 10};
        for (int i = 0; i < 5; i++) grid.modules[10 * 21 + pattern[i]] = 1;
        assert(tube_qr_penalty(&grid) == 772 + 1152 + 80 + 90);
        for (int y = 0; y < 21; y++) for (int x = 0; x < 21; x++) grid.modules[y * 21 + x] = (uint8_t)((x + y) % 2 == 0);
        assert(tube_qr_penalty(&grid) == 0);
        /* All dark mirrors all light. */
        memset(grid.modules, 1, 21 * 21);
        assert(tube_qr_penalty(&grid) == 42 * 19 + 400 * 3 + 100);
    }

    /* The reader rejects damage: a flipped data module breaks a syndrome, a
     * flipped format module breaks the format check. */
    {
        uint8_t output[512]; size_t read;
        const char *text = "https://youtube.com/qr/activate/ABC-DEF-GHI";
        assert(tube_qr_encode_text(text, &g_code) == 0);
        assert(g_code.version == 4 && g_code.level == TUBE_QR_LEVEL_M);
        assert(read_symbol(&g_code, output, &read) == READ_OK && read == strlen(text) && !memcmp(output, text, read));
        int flipped = 0;
        for (int y = 9; y < g_code.size - 9 && flipped < 40; y++) for (int x = 9; x < g_code.size - 9 && flipped < 40; x++) {
            if (is_function(g_code.version, x, y)) continue;
            g_code.modules[y * g_code.size + x] ^= 1;
            assert(read_symbol(&g_code, output, &read) == READ_SYNDROME);
            g_code.modules[y * g_code.size + x] ^= 1;
            flipped++;
        }
        assert(flipped == 40);
        g_code.modules[8 * g_code.size + 2] ^= 1;
        assert(read_symbol(&g_code, output, &read) == READ_FORMAT);
        g_code.modules[8 * g_code.size + 2] ^= 1;
        g_code.modules[0] ^= 1;
        assert(read_symbol(&g_code, output, &read) == READ_STRUCTURE);
        g_code.modules[0] ^= 1;
        assert(read_symbol(&g_code, output, &read) == READ_OK);
        assert(tube_qr_module(&g_code, 0, 0) == 1 && tube_qr_module(&g_code, 1, 1) == 0 && tube_qr_module(&g_code, 3, 3) == 1);
        assert(!tube_qr_module(&g_code, -1, 0) && !tube_qr_module(&g_code, 0, -1) && !tube_qr_module(NULL, 0, 0));
        assert(!tube_qr_module(&g_code, g_code.size, 0) && !tube_qr_module(&g_code, 0, g_code.size));
    }

    /* Text helper: level M while it fits, then L, then refusal. */
    {
        char text[TUBE_QR_MAX_PAYLOAD + 2];
        uint8_t output[512]; size_t read;
        memset(text, 'a', sizeof text);
        text[213] = 0;
        assert(tube_qr_encode_text(text, &g_code) == 0 && g_code.level == TUBE_QR_LEVEL_M && g_code.version == 10);
        text[213] = 'a'; text[214] = 0;
        assert(tube_qr_encode_text(text, &g_code) == 0 && g_code.level == TUBE_QR_LEVEL_L && g_code.version == 9);
        assert(read_symbol(&g_code, output, &read) == READ_OK && read == 214);
        text[214] = 'a'; text[271] = 0;
        assert(tube_qr_encode_text(text, &g_code) == 0 && g_code.level == TUBE_QR_LEVEL_L && g_code.version == 10);
        text[271] = 'a'; text[272] = 0;
        assert(tube_qr_encode_text(text, &g_code) == -1 && g_code.size == 0);
        assert(tube_qr_encode_text("", &g_code) == 0 && g_code.version == 1);
        assert(read_symbol(&g_code, output, &read) == READ_OK && read == 0);
    }

    /* Invalid arguments leave a cleared result. */
    {
        tube_qr_code code;
        const uint8_t byte = 'x';
        assert(tube_qr_encode(&byte, 1, TUBE_QR_LEVEL_M, NULL) == -1);
        assert(tube_qr_encode_text(NULL, NULL) == -1);
        memset(&code, 0xAA, sizeof code);
        assert(tube_qr_encode_text(NULL, &code) == -1 && code.size == 0);
        memset(&code, 0xAA, sizeof code);
        assert(tube_qr_encode(NULL, 1, TUBE_QR_LEVEL_M, &code) == -1 && code.size == 0);
        assert(tube_qr_encode(NULL, 0, TUBE_QR_LEVEL_M, &code) == 0 && code.version == 1);
        assert(tube_qr_encode(&byte, 1, (tube_qr_level)4, &code) == -1 && code.size == 0);
        assert(tube_qr_encode(&byte, 1, (tube_qr_level)-1, &code) == -1);
        assert(tube_qr_encode_ex(&byte, 1, TUBE_QR_LEVEL_M, 0, TUBE_QR_MASK_AUTO, &code) == -1);
        assert(tube_qr_encode_ex(&byte, 1, TUBE_QR_LEVEL_M, 11, TUBE_QR_MASK_AUTO, &code) == -1);
        assert(tube_qr_encode_ex(&byte, 1, TUBE_QR_LEVEL_M, 1, 8, &code) == -1);
        assert(tube_qr_encode_ex(&byte, 1, TUBE_QR_LEVEL_M, 1, -2, &code) == -1 && code.size == 0);
        assert(tube_qr_encode(&byte, (size_t)-1, TUBE_QR_LEVEL_L, &code) == -1);
    }

    puts("PASS: QR symbols for versions 1-10 at levels L/M/Q/H read back through an independent reader; masks, penalties, limits and invalid input covered");
    return 0;
}
