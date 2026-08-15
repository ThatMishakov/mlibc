#ifndef NL_TYPES_H
#define NL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __MLIBC_ABI_ONLY

typedef void *nl_catd;
typedef int nl_item;

#define NL_SETD 1
#define NL_CAT_LOCALE 1

int catclose(nl_catd __catd);
char *catgets(nl_catd __catd, int __set_id, int __msg_id, const char *__s);
nl_catd catopen(const char *__name, int __oflag);

#endif /* !__MLIBC_ABI_ONLY */

#ifdef __cplusplus
}
#endif

#endif /* NL_TYPES_H */
