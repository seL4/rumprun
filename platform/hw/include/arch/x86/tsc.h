
/* just read the tsc. This may be executed out of order as it is unserialised */
static inline uint64_t
rdtsc_pure(void)
{
    uint32_t high, low;

    __asm__ __volatile__ (
        "rdtsc"
        : "=a" (low),
        "=d" (high)
        : /* no input */
        : /* no clobbers */
    );

    return (((uint64_t) high) << 32llu) + (uint64_t) low;

}