/**
 * @file
 *
 * Functions for reporting errors and warnings.
 *
 * <hr>$Revision: 49448 $<hr>
 *
 * @defgroup stdio Standard IO related functions
 * @{
 */

extern void __cavm_die(void) __attribute__ ((noreturn));
extern void cavm_fatal(const char *format, ...) __attribute__ ((noreturn, format(printf, 1, 2)));
extern void cavm_error(const char *format, ...) __attribute__ ((format(printf, 1, 2)));
extern void cavm_warn(const char *format, ...) __attribute__ ((format(printf, 1, 2)));
#define cavm_warn_if(expression, format, ...) if (cavm_unlikely(expression)) cavm_warn(format, ##__VA_ARGS__)

/* The following defines control detailed tracing of various parts of the
   CAVM. Each one can be enabled(1) or disabled(0) independently. These
   should be disabled unless you are trying to debug something specific */

typedef enum
{
    TRACE_ENABLE_BGX,               /* BGX networking block */
    TRACE_ENABLE_DRAM,              /* DRAM initialzation */
    TRACE_ENABLE_DRAM_TEST,         /* DRAM test code */
    TRACE_ENABLE_INIT,              /* Early initialization, before main() */
    TRACE_ENABLE_ECAM,              /* ECAM initialization */
    TRACE_ENABLE_QLM,               /* QLM related debug */
    TRACE_ENABLE_EMMC,              /* eMMC related debug */
    TRACE_ENABLE_PCIE,              /* PCIe link init */
    TRACE_ENABLE_PCIE_CONFIG,       /* PCIe config space reads / writes */
    TRACE_ENABLE_SATA,              /* SATA/AHCI related debug */
    TRACE_ENABLE_CCPI,              /* Multi-node related debug */
    TRACE_ENABLE_FATFS,             /* FatFs related debug */
    TRACE_ENABLE_MPI,               /* MPI related debug */
    TRACE_ENABLE_ENV,               /* Environment variables related debug */
    TRACE_ENABLE_FPA,               /* Free Pool Allocator */
    TRACE_ENABLE_PKI,               /* Packet Input */
    TRACE_ENABLE_PKO,               /* Packet Output */
    TRACE_ENABLE_SSO,               /* SSO */
    TRACE_ENABLE_DEVICE,            /* ECAM based device framework */
    TRACE_ENABLE_DEVICE_SCAN,       /* ECAM based device scanning detail */
    TRACE_ENABLE_NIC,               /* Virtual NIC */
    TRACE_ENABLE_FDT_OS,            /* Device tree passed to OS */
} cavm_trace_enable_t;

/* See cavm-config.c to change the trace level for before config files are loaded */
extern uint64_t cavm_trace_enables;

/**
 * Macro for low level tracing of CAVM functions. When enabled,
 * these translate to printf() calls. The "area" is a string
 * that is appended to "TRACE_ENABLE_" to figure out which
 * enable macro to use. The macro expects a ';' after it.
 */
#define TRACE(area, format, ...) do {                       \
    if (cavm_trace_enables & (1ull << TRACE_ENABLE_##area))  \
        printf(#area ": " format, ##__VA_ARGS__);               \
} while (0)

/** @} */
