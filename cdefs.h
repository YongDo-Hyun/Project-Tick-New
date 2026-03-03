#ifndef SH_LOCAL_CDEFS_H
#define SH_LOCAL_CDEFS_H

#ifndef __dead2
#define __dead2 __attribute__((__noreturn__))
#endif

#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif

#ifndef __printflike
#define __printflike(fmtarg, firstvararg) \
	__attribute__((__format__(__printf__, fmtarg, firstvararg)))
#endif

#ifndef __printf0like
#define __printf0like(fmtarg, firstvararg) \
	__attribute__((__format__(__printf__, fmtarg, firstvararg)))
#endif

#ifndef __nonstring
#define __nonstring
#endif

#endif
