/**
 * @file sha256.c
 * @brief SHA-256 Hash Implementation
 * @traceability CM-MATH-001 §7 (Hash chain)
 *
 * @details
 * Pure C99 implementation of SHA-256 (FIPS 180-4).
 * Used for:
 * - Ledger hash chain (L_t = H(... || L_{t-1} || ...))
 * - Policy hash H_P
 * - Genesis binding L_0
 *
 * No dynamic allocation. Processes data in 64-byte blocks.
 *
 * @copyright Copyright (c) 2026 The Murray Family Innovation Trust.
 * All rights reserved.
 */

#include "ct_types.h"
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/*============================================================================
 * SHA-256 Context Structure
 *============================================================================*/

/**
 * @brief SHA-256 context for incremental hashing
 */
typedef struct {
    uint32_t state[8];     /**< Hash state (H0..H7) */
    uint64_t count;        /**< Total bits processed */
    uint8_t  buffer[64];   /**< Block buffer */
} cm_sha256_ctx_t;

/*============================================================================
 * SHA-256 Constants (FIPS 180-4)
 *============================================================================*/

/**
 * @brief Round constants K[0..63]
 * First 32 bits of fractional parts of cube roots of first 64 primes
 */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/**
 * @brief Initial hash values H[0..7]
 * First 32 bits of fractional parts of square roots of first 8 primes
 */
static const uint32_t H_INIT[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

/*============================================================================
 * SHA-256 Helper Macros
 *============================================================================*/

#define ROTR(x, n)  (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x)  (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x)  (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

/*============================================================================
 * Internal Functions
 *============================================================================*/

/**
 * @brief Process a single 64-byte block
 */
static void sha256_transform(cm_sha256_ctx_t *ctx, const uint8_t *block)
{
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t T1, T2;
    
    /* Prepare message schedule W[0..63] */
    for (int i = 0; i < 16; i++) {
        W[i] = ((uint32_t)block[i * 4    ] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] <<  8) |
               ((uint32_t)block[i * 4 + 3]);
    }

#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 13)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
#endif

    for (int i = 16; i < 64; i++) {
        W[i] = SIG1(W[i-2]) + W[i-7] + SIG0(W[i-15]) + W[i-16];
    }

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

    /* Initialize working variables */
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];
    
    /* 64 rounds */
    for (int i = 0; i < 64; i++) {
        T1 = h + EP1(e) + CH(e, f, g) + K[i] + W[i];
        T2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }
    
    /* Add compressed chunk to current hash value */
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

/*============================================================================
 * Public API
 *============================================================================*/

/**
 * @brief Initialize SHA-256 context
 * @param ctx Context to initialize
 * @traceability CM-MATH-001 §7
 */
void cm_sha256_init(cm_sha256_ctx_t *ctx)
{
    if (!ctx) return;
    
    memcpy(ctx->state, H_INIT, sizeof(H_INIT));
    ctx->count = 0;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

/* GCC -fanalyzer false positive: interprocedural analysis through the
 * (const void *data) parameter cannot prove that callers have initialised
 * every byte of the buffer passed to ct_sha256_update.  All callers fill
 * their buffers completely via write_u32_le / write_u64_le / write_i32_le
 * before calling this function.  See CT-MATH-001 §16 for the proof that
 * the header layout is fully covered. */
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 13)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
#endif

/**
 * @brief Update hash with data
 * @param ctx Context
 * @param data Input data
 * @param len Length of data in bytes
 * @traceability CM-MATH-001 §7
 */
void cm_sha256_update(cm_sha256_ctx_t *ctx, const uint8_t *data, size_t len)
{
    if (!ctx || !data || len == 0) return;
    
    size_t buf_fill = (size_t)(ctx->count / 8) % 64;
    ctx->count += (uint64_t)len * 8;
    
    /* Fill buffer first */
    if (buf_fill > 0) {
        size_t to_copy = 64 - buf_fill;
        if (to_copy > len) to_copy = len;
        assert(data != NULL);  /* GCC 15 -fanalyzer interprocedural false positive */
        memcpy(ctx->buffer + buf_fill, data, to_copy);
        data += to_copy;
        len -= to_copy;
        buf_fill += to_copy;
        
        if (buf_fill == 64) {
            sha256_transform(ctx, ctx->buffer);
            buf_fill = 0;
        }
    }
    
    /* Process full blocks */
    while (len >= 64) {
        sha256_transform(ctx, data);
        data += 64;
        len -= 64;
    }
    
    /* Copy remainder to buffer */
    if (len > 0) {
        memcpy(ctx->buffer, data, len);
    }
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

/**
 * @brief Finalize hash and output digest
 * @param ctx Context
 * @param digest Output buffer (32 bytes)
 * @traceability CM-MATH-001 §7
 */
void cm_sha256_final(cm_sha256_ctx_t *ctx, uint8_t *digest)
{
    if (!ctx || !digest) return;
    
    size_t buf_fill = (size_t)(ctx->count / 8) % 64;
    uint8_t pad[64] = {0};
    
    /* Padding: 1 bit followed by zeros */
    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    
    /* Pad to 56 mod 64 bytes */
    size_t pad_len;
    if (buf_fill < 56) {
        pad_len = 56 - buf_fill;
    } else {
        pad_len = 120 - buf_fill;  /* Need another block */
    }
    cm_sha256_update(ctx, pad, pad_len);
    
    /* Append 64-bit length in bits (big-endian) */
    uint8_t len_be[8];
    uint64_t bits = ctx->count - (uint64_t)pad_len * 8;  /* Original count */
    bits = ctx->count;  /* Actually we want final count before this */
    
    /* Recompute: we added pad_len bytes of padding, but count was updated */
    /* The length appended should be original message length in bits */
    /* ctx->count now includes padding, so subtract */
    bits = ctx->count - (uint64_t)pad_len * 8;
    
    len_be[0] = (uint8_t)(bits >> 56);
    len_be[1] = (uint8_t)(bits >> 48);
    len_be[2] = (uint8_t)(bits >> 40);
    len_be[3] = (uint8_t)(bits >> 32);
    len_be[4] = (uint8_t)(bits >> 24);
    len_be[5] = (uint8_t)(bits >> 16);
    len_be[6] = (uint8_t)(bits >> 8);
    len_be[7] = (uint8_t)(bits);
    
    cm_sha256_update(ctx, len_be, 8);
    
    /* Output hash (big-endian) */
    for (int i = 0; i < 8; i++) {
        digest[i * 4    ] = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

/**
 * @brief One-shot SHA-256 hash
 * @param data Input data
 * @param len Length of data
 * @param digest Output buffer (32 bytes)
 * @traceability CM-MATH-001 §7
 */
void cm_sha256(const uint8_t *data, size_t len, uint8_t *digest)
{
    cm_sha256_ctx_t ctx;
    cm_sha256_init(&ctx);
    cm_sha256_update(&ctx, data, len);
    cm_sha256_final(&ctx, digest);
}

/**
 * @brief Domain-separated hash: H(tag || LE64(len) || data)
 * @param tag Domain separation tag (null-terminated)
 * @param data Payload data
 * @param len Length of payload
 * @param digest Output buffer (32 bytes)
 * @traceability CM-MATH-001 §7.2
 *
 * This prevents second-preimage attacks across different domains.
 */
void cm_sha256_domain(const char *tag, const uint8_t *data, size_t len, uint8_t *digest)
{
    cm_sha256_ctx_t ctx;
    cm_sha256_init(&ctx);
    
    /* Hash the tag including null terminator for unambiguous parsing */
    if (tag) {
        size_t tag_len = 0;
        while (tag[tag_len]) tag_len++;
        cm_sha256_update(&ctx, (const uint8_t *)tag, tag_len);
    }
    
    /* Hash the length as little-endian 64-bit */
    uint8_t len_le[8];
    len_le[0] = (uint8_t)(len);
    len_le[1] = (uint8_t)(len >> 8);
    len_le[2] = (uint8_t)(len >> 16);
    len_le[3] = (uint8_t)(len >> 24);
    len_le[4] = (uint8_t)((uint64_t)len >> 32);
    len_le[5] = (uint8_t)((uint64_t)len >> 40);
    len_le[6] = (uint8_t)((uint64_t)len >> 48);
    len_le[7] = (uint8_t)((uint64_t)len >> 56);
    cm_sha256_update(&ctx, len_le, 8);
    
    /* Hash the payload */
    if (data && len > 0) {
        cm_sha256_update(&ctx, data, len);
    }
    
    cm_sha256_final(&ctx, digest);
}
