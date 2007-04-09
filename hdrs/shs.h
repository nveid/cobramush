#ifndef _SHS_H
#define _SHS_H

#include "config.h"
#ifdef I_STDINT
#include <stdint.h>
#endif
#include "confmagic.h"

/* -------------- SHS.H --------------------------- */

#ifdef _WIN32
#define LONG SHS_LONG
#endif

#ifdef __MINGW32__
typedef unsigned char BYTE;
typedef long LONG;
#else
typedef unsigned char BYTE;
#ifdef HAS_UINT32_T
typedef uint32_t LONG;
#else
typedef unsigned int LONG;
#endif
#endif

#define SHS_BLOCKSIZE 64

#define SHS_DIGESTSIZE 20

/** An shs digest.
 */
typedef struct {
  LONG digest[5];		/**< message digest */
  LONG countLo;			/**< 64-bit bit count, low half */
  LONG countHi;			/**< 64-bit bit count, high half */
  LONG data[16];		/**< SHS data buffer */
  BYTE reverse_wanted;		/**< true to reverse (little_endian) false to not */
} SHS_INFO;

void shsInit(SHS_INFO *shsInfo);
void shsUpdate(SHS_INFO *shsInfo, const BYTE * buffer, int count);
void shsFinal(SHS_INFO *shsInfo);
#endif
