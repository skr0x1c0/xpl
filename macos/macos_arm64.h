#ifndef macos_arm64_h
#define macos_arm64_h

struct arm_state_hdr64 {
    uint32_t flavor;
    uint32_t count;
};

struct arm_saved_state64 {
    uint64_t x[29];     /* General purpose registers x0-x28 */
    uint64_t fp;        /* Frame pointer x29 */
    uint64_t lr;        /* Link register x30 */
    uint64_t sp;        /* Stack pointer x31 */
    uint64_t pc;        /* Program counter */
    uint32_t cpsr;      /* Current program status register */
    uint32_t aspr;      /* Reserved padding */
    uint64_t far;       /* Virtual fault address */
    uint32_t esr;       /* Exception syndrome register */
    uint32_t exception; /* Exception number */
    uint64_t jophash;
};

struct arm_saved_state {
    struct arm_state_hdr64 ash;
    struct arm_saved_state64 uss;
};

typedef __uint128_t uint128_t;
typedef uint64_t uint64x2_t __attribute__((ext_vector_type(2)));
typedef uint32_t uint32x4_t __attribute__((ext_vector_type(4)));

struct arm_neon_saved_state64 {
    union {
        uint128_t  q[32];
        uint64x2_t d[32];
        uint32x4_t s[32];
    } v;
    uint32_t fpsr;
    uint32_t fpcr;
};

struct arm_neon_saved_state {
    struct arm_state_hdr64 nsh;
    struct arm_neon_saved_state64 uns;
};

struct arm_context {
    struct arm_saved_state ss;
    struct arm_neon_saved_state ns;
};

#endif

