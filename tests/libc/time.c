/* gmtime, localtime, mktime, asctime and strftime, on a machine whose
   local time is UTC: the expected output is the host's with TZ=UTC. */
#include <stdio.h>
#include <string.h>
#include <time.h>

static void show(time_t t)
{
    struct tm *g = gmtime(&t), copy = *g, *l;
    char buf[64];

    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S %a %j", g);
    printf("%lld: %s wday %d yday %d isdst %d back %lld", (long long)t, buf, g->tm_wday, g->tm_yday,
           g->tm_isdst, (long long)mktime(&copy));
    l = localtime(&t);
    printf(" local %02d:%02d %s", l->tm_hour, l->tm_min, l->tm_mday == g->tm_mday ? "same" : "differs");
    printf(" %s", asctime(g));
}

int main(void)
{
    struct tm tm;
    time_t t;
    long long times[] = { 0, 1, 86399, 86400, 951782400, 951868800, 1234567890, 2147483647,
                          -1, -86400, -2147483647 - 1, 4102444800LL, 253402300799LL };
    unsigned i;

    for (i = 0; i < sizeof times / sizeof times[0]; i++) {
        show((time_t)times[i]);
    }
    memset(&tm, 0, sizeof tm);
    tm.tm_year = 100; tm.tm_mon = 14; tm.tm_mday = 35; tm.tm_hour = 25; tm.tm_min = -5; tm.tm_sec = 70;
    t = mktime(&tm);
    printf("normalised: %lld %d-%02d-%02d %02d:%02d:%02d wday %d yday %d\n", (long long)t, tm.tm_year + 1900,
           tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_wday, tm.tm_yday);
    printf("difftime %.0f\n", difftime(86400, 0));
    return 0;
}
