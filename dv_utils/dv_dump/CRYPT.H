#ifndef md5_INCLUDED
#define md5_INCLUDED

//#include "leoslang.h"

#ifdef __cplusplus
extern "C" 
{
#endif

typedef unsigned __int32 CARD32;
#ifndef BYTE
typedef unsigned char BYTE;
#endif

/* Define the state of the MD5 Algorithm. */
typedef struct md5_state_t md5_state_t;
struct md5_state_t
{
    size_t count[2];	/* message length in bits, lsw first */
    CARD32 abcd[4];		/* digest buffer */
    BYTE buf[64];		/* accumulate block */
};

typedef struct md5_hash md5_hash;
struct md5_hash
{
	BYTE hash[16];
};

md5_hash calculate_md5(const void *buf, size_t length);

#ifdef __cplusplus
};  /* end extern "C" */
#endif

#endif /* md5_INCLUDED */
