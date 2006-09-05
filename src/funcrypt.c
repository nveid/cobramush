/**
 * \file funcrypt.c
 *
 * \brief Functions for cryptographic stuff in softcode
 *
 *
 */
#include "copyrite.h"

#include "config.h"
#include <time.h>
#include <string.h>
#include <ctype.h>
#include "conf.h"
#include "case.h"
#include "externs.h"
#include "version.h"
#include "extchat.h"
#include "htab.h"
#include "flags.h"
#include "dbdefs.h"
#include "parse.h"
#include "function.h"
#include "command.h"
#include "game.h"
#include "attrib.h"
#include "ansi.h"
#include "match.h"
#ifdef HAS_OPENSSL
#include <openssl/sha.h>
#include <openssl/evp.h>
#else
#include "shs.h"
#endif
#include "confmagic.h"


static char *crunch_code _((char *code));
static char *crypt_code _((char *code, char *text, int type));

#ifdef HAS_OPENSSL
static void safe_hexchar(unsigned char c, char *buff, char **bp);
#endif

/* Copy over only alphanumeric chars */
static char *
crunch_code(char *code)
{
  char *in;
  char *out;
  static char output[BUFFER_LEN];

  out = output;
  in = code;
  while (*in) {
    while (*in == ESC_CHAR) {
      while (*in && *in != 'm')
	in++;
      in++;			/* skip 'm' */
    }
    if ((*in >= 32) && (*in <= 126)) {
      *out++ = *in;
    }
    in++;
  }
  *out = '\0';
  return output;
}

static char *
crypt_code(char *code, char *text, int type)
{
  static char textbuff[BUFFER_LEN];
  char codebuff[BUFFER_LEN];
  int start = 32;
  int end = 126;
  int mod = end - start + 1;
  char *p, *q, *r;

  if (!text || !*text)
    return (char *) "";
  if (!code || !*code)
    return text;
  strcpy(codebuff, crunch_code(code));
  if (!*codebuff)
    return text;
  textbuff[0] = '\0';

  p = text;
  q = codebuff;
  r = textbuff;
  /* Encryption: Simply go through each character of the text, get its ascii
   * value, subtract start, add the ascii value (less start) of the
   * code, mod the result, add start. Continue  */
  while (*p) {
    if ((*p < start) || (*p > end)) {
      p++;
      continue;
    }
    if (type)
      *r++ = (((*p++ - start) + (*q++ - start)) % mod) + start;
    else
      *r++ = (((*p++ - *q++) + 2 * mod) % mod) + start;
    if (!*q)
      q = codebuff;
  }
  *r = '\0';
  return textbuff;
}

FUNCTION(fun_encrypt)
{
  safe_str(crypt_code(args[1], args[0], 1), buff, bp);
}

FUNCTION(fun_decrypt)
{
  safe_str(crypt_code(args[1], args[0], 0), buff, bp);
}

FUNCTION(fun_checkpass)
{
  dbref it = match_thing(executor, args[0]);
  if (!(GoodObject(it) && IsPlayer(it))) {
    safe_str(T("#-1 NO SUCH PLAYER"), buff, bp);
    return;
  }
  safe_boolean(password_check(it, args[1]), buff, bp);
}

FUNCTION(fun_sha0)
{
#ifdef HAS_OPENSSL
  unsigned char hash[SHA_DIGEST_LENGTH];
  int n;

  SHA((unsigned char *) args[0], arglens[0], hash);

  for (n = 0; n < SHA_DIGEST_LENGTH; n++)
    safe_hexchar(hash[n], buff, bp);
#else
  SHS_INFO shsInfo;
  shsInfo.reverse_wanted = (BYTE) options.reverse_shs;
  shsInit(&shsInfo);
  shsUpdate(&shsInfo, (const BYTE *) args[0], arglens[0]);
  shsFinal(&shsInfo);
  safe_format(buff, bp, "%0lx%0lx%0lx%0lx%0lx", shsInfo.digest[0],
	      shsInfo.digest[1], shsInfo.digest[2], shsInfo.digest[3],
	      shsInfo.digest[4]);
#endif
}

FUNCTION(fun_digest)
{
#ifdef HAS_OPENSSL
  EVP_MD_CTX ctx;
  const EVP_MD *mp;
  unsigned char md[EVP_MAX_MD_SIZE];
  size_t n, len = 0;

  if ((mp = EVP_get_digestbyname(args[0])) == NULL) {
    safe_str(T("#-1 UNSUPPORTED DIGEST TYPE"), buff, bp);
    return;
  }

  EVP_DigestInit(&ctx, mp);
  EVP_DigestUpdate(&ctx, args[1], arglens[1]);
  EVP_DigestFinal(&ctx, md, &len);

  for (n = 0; n < len; n++) {
    safe_hexchar(md[n], buff, bp);
  }

#else
  if (strcmp(args[0], "sha") == 0) {
    SHS_INFO shsInfo;
    shsInfo.reverse_wanted = (BYTE) options.reverse_shs;
    shsInit(&shsInfo);
    shsUpdate(&shsInfo, (const BYTE *) args[0], arglens[0]);
    shsFinal(&shsInfo);
    safe_format(buff, bp, "%0lx%0lx%0lx%0lx%0lx", shsInfo.digest[0],
		shsInfo.digest[1], shsInfo.digest[2], shsInfo.digest[3],
		shsInfo.digest[4]);
  } else {
    safe_str(T("#-1 UNSUPPORTED DIGEST TYPE"), buff, bp);
  }
#endif
}

#ifdef HAS_OPENSSL
static void
safe_hexchar(unsigned char c, char *buff, char **bp)
{
  const char *digits = "0123456789abcdef";
  if (*bp - buff < BUFFER_LEN - 1) {
    **bp = digits[c >> 4];
    (*bp)++;
  }
  if (*bp - buff < BUFFER_LEN - 1) {
    **bp = digits[c & 0x0F];
    (*bp)++;
  }
}
#endif
