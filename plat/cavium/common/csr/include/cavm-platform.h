/**
 * @file
 *
 * Functions for information about the run platform.
 *
 * <hr>$Revision: 49448 $<hr>
 * @addtogroup hal
 * @{
 */

/**
 * This typedef defines the possible platforms for the CAVM. The
 * numbers represent fuse setting in Fuses[197:195].
 */
typedef enum
{
    PLATFORM_HW = 0,
    PLATFORM_EMULATOR = 1,
    PLATFORM_RTL = 2,
    PLATFORM_ASIM = 3,
} cavm_platform_t;

/**
 * Check which platform we are currently running on. This allows a CAVM binary to
 * run on various platforms without a recompile.
 *
 * @param platform Platform to check for
 *
 * @return Non zero if we are on the platform
 */
static inline int cavm_is_platform(cavm_platform_t platform) __attribute__ ((pure, always_inline));
static inline int cavm_is_platform(cavm_platform_t platform)
{
    extern cavm_platform_t __cavm_platform;
    return (__cavm_platform == platform);
}

/**
 * Call to initialize the platform state
 */
extern void __cavm_platform_init(void);

/** @} */
