#ifndef __COMPILE_H
#define __COMPILE_H

/* Compiler-specific stuff. */

#ifndef __GNUC_PREREQ
#if defined __GNUC__ && defined __GNUC_MINOR__
# define __GNUC_PREREQ(maj, min) \
        ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
# define __GNUC_PREREQ(maj, min) 0
#endif
#endif

/* For gcc 3 and up, this attribute lets the compiler know that the
 * function returns a newly allocated value, for pointer aliasing
 * optimizations.
 */
#if !defined(__attribute_malloc__) && __GNUC_PREREQ(2, 96)
#define __attribute_malloc__ __attribute__ ((__malloc__))
#elif !defined(__attribute_malloc__)
#define __attribute_malloc__
#endif

/* The C99 restrict keyword lets the compiler do some more pointer
 * aliasing optimizations. Essentially, a RESTRICT pointer function
 * argument can't share that pointer with ones passed via other
 * arguments. 
 * This should be a Configure check sometime.
 */
#if __GNUC_PREREQ(2, 92)
#define RESTRICT __restrict
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define RESTRICT restrict
#else
#define RESTRICT
#endif

/* If a compiler knows a function will never return, it can generate
   slightly better code for calls to it. */
#if defined(WIN32) && _MSC_VER >= 1200
#define NORETURN __declspec(noreturn)
#elif defined(HASATTRIBUTE)
#define NORETURN __attribute__ ((__noreturn__))
#else
#define NORETURN
#endif

/* Enable Win32 services support */
#ifdef WIN32
#define WIN32SERVICES
#endif

/* Disable Win32 services support due to it failing to run properly
   when compiling with MinGW32. Eventually I would like to correct
   the issue. - EEH */
#ifdef __MINGW32__
#undef WIN32SERVICES
#endif

/* --------------- Stuff for Win32 services ------------------ */
/*
   When "exit" is called to handle an error condition, we really want to
   terminate the game thread, not the whole process.
   MS VS.NET (_MSC_VER >= 1200) requires the weird noreturn stuff.
 */

#ifdef WIN32SERVICES
#ifndef NT_TCP
#define exit(arg) Win32_Exit (arg)
#endif
void NORETURN WIN32_CDECL Win32_Exit(int exit_code);
#endif

#endif				/* __COMPILE_H */
