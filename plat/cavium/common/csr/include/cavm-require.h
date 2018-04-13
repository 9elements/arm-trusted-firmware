/**
 * @file
 *
 * Functions and macros to control what parts of the CAVM are linked in
 *
 * <hr>$Revision: 49448 $<hr>
 * @defgroup require Component linking control
 * @{
 */

/**
 * Optional parts of the CAVM code are pulled in by adding
 * REQUIRE() lines to the function cavm_require_depends().
 * Component symbols are defined as weak so that they are not
 * linked in unless a REQUIRE() pulls them in.
 */
#define REQUIRE(component)                                  \
    do                                                          \
    {                                                           \
        extern char __cavm_require_symbol_##component;           \
        cavm_warn_if(__cavm_require_symbol_##component,           \
            "Require of %s failed\n", #component);              \
    } while (0)

/**
 * The following macro defines a special symbol in a C file to
 * define it as a require component. Referencing this symbol
 * causes all objects defined in the C file to be pulled in. This
 * symbol should only be referenced by using the REQUIRE()
 * macro in the function cavm_require_depends().
 */
#define REQUIRE_DEFINE(component)           \
    char __cavm_require_symbol_##component;      \
    char __cavm_is_required_symbol_##component

/**
 * Return if a component has been required. Useful for if
 * statements around referencing of weak symbols.
 */
#define IS_REQUIRED(component)                                          \
    ({int is_required;                                                      \
    do                                                                      \
    {                                                                       \
        extern char __cavm_is_required_symbol_##component __attribute__((weak));\
        is_required = (&__cavm_is_required_symbol_##component != NULL);      \
    } while (0);                                                            \
    is_required;})


/**
 * The require macros use weak symbols to control if components
 * are linked in. All directly referenced symbols in a component
 * must be defined a weak. This causes the component to only be
 * pulled in by the linker if the symbol defined by
 * REQUIRE_DEFINE is used.
 */
#define WEAK __attribute__((weak))

/**
 * This function is not defined by the CAVM libraries. It must be
 * defined by all CAVM applications. It should be empty except for
 * containing REQUIRE() lines. The cavm-init code has a strong
 * reference to cavm_requires_depends() which then contains strong
 * references to all needed components.
 */
extern void __cavm_require_depends(void);

/** @} */
