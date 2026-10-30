/*
 * cppdate.so: LD_PRELOAD shim that pins __DATE__ / __TIME__ in q3cpp.
 *
 * id's q3cpp (src/tools/lcc/cpp/cpp.c) takes __DATE__ and __TIME__ from
 * ctime(time(NULL)): __DATE__ = curtime+4 (7 chars) + curtime+20 (4 chars),
 * __TIME__ = curtime+11 (8 chars). id's g_main.c stores __DATE__ in
 * the "gamedate" cvar, so a byte-exact build must reproduce the original date.
 *
 * Environment (both optional; the shim is a no-op when neither is set):
 *   Q3CPP_DATE  exactly 11 chars, e.g. "Sep 30 2002" (glibc style) or
 *               "Mar 04 2003" (MSVC runtime style, zero-padded day)
 *   Q3CPP_TIME  exactly 8 chars, e.g. "12:00:00"
 *
 * The weekday field of the ctime() string is not used by q3cpp and is left as
 * "Xxx". No id source is modified; the shim only replaces libc's ctime().
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

char *ctime(const time_t *t)
{
    static char buf[32];
    char *(*real)(const time_t *) = (char *(*)(const time_t *))dlsym(RTLD_NEXT, "ctime");
    const char *d = getenv("Q3CPP_DATE");
    const char *tm = getenv("Q3CPP_TIME");
    char *r = real ? real(t) : NULL;

    if ((!d || !*d) && (!tm || !*tm))
        return r;
    /* "Www Mmm dd hh:mm:ss yyyy\n" */
    memcpy(buf, r && strlen(r) >= 25 ? r : "Xxx Jan  1 00:00:00 1970\n", 26);
    memcpy(buf, "Xxx", 3);
    if ((d && *d && strlen(d) != 11) || (tm && *tm && strlen(tm) != 8)) {
        fprintf(stderr, "cppdate.so: Q3CPP_DATE must be 11 chars ('Sep 30 2002'), "
                        "Q3CPP_TIME 8 chars ('12:00:00')\n");
        exit(2);
    }
    if (d && *d) {
        memcpy(buf + 4, d, 6);       /* "Mmm dd" */
        memcpy(buf + 20, d + 7, 4);  /* "yyyy"   */
    }
    if (tm && *tm)
        memcpy(buf + 11, tm, 8);
    buf[24] = '\n';
    buf[25] = '\0';
    return buf;
}
