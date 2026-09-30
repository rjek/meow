/* init: runs /etc/rc, then a shell on the console, again if the shell
   fails.  A shell that ends cleanly, by exit or at the end of input,
   shuts the system down.

   /etc/rc has one command a line, with # to the end of the line and no
   quoting; a command ending in & is started and left to run, which is
   how a server is started.  "mounted PATH" waits, two seconds at most,
   for a server to have mounted PATH before going on. */
#include <stdio.h>
#include <string.h>
#include "catflap.h"

#define MAXWORDS 16

static int mounted(const char *path)
{
    char line[64], type[16];
    FILE *f = fopen("/proc/mounts", "r");
    int found = 0;

    if (f == NULL) {
        return 0;
    }
    while (found == 0 && fscanf(f, "%63s %15s", line, type) == 2) {
        found = strcmp(line, path) == 0;
    }
    fclose(f);
    return found;
}

static void run(char *words[], int n, int background)
{
    char path[64];
    int pid, status;

    if (strcmp(words[0], "mounted") == 0 && n == 2) {
        int waited;

        for (waited = 0; waited < 200 && mounted(words[1]) == 0; waited++) {
            thread_sleep(1);
        }
        if (waited == 200) {
            printf("init: nothing mounted at %s\n", words[1]);
        }
        return;
    }
    if (strchr(words[0], '/') != NULL) {
        strcpy(path, words[0]);
    } else {
        sprintf(path, "/bin/%s", words[0]);
    }
    words[n] = NULL;
    pid = process_spawn(path, n, words);
    if (pid < 0) {
        printf("init: cannot run %s: %d\n", words[0], pid);
        return;
    }
    if (background == 0) {
        process_wait(pid, &status);
        if (status != 0) {
            printf("init: %s exited with %d\n", words[0], status);
        }
    }
}

static void rc(void)
{
    char line[128], *words[MAXWORDS + 1], *p;
    FILE *f = fopen("/etc/rc", "r");

    if (f == NULL) {
        return;                         /* nothing to do at boot */
    }
    while (fgets(line, sizeof line, f) != NULL) {
        int n = 0, background = 0;

        for (p = line; *p != '\0'; p++) {
            if (*p == '#' || *p == '\n' || *p == '\r') {
                *p = '\0';
                break;
            }
        }
        for (p = line; n < MAXWORDS;) {
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            if (*p == '\0') {
                break;
            }
            words[n++] = p;
            while (*p != '\0' && *p != ' ' && *p != '\t') {
                p++;
            }
            if (*p != '\0') {
                *p++ = '\0';
            }
        }
        if (n > 0 && strcmp(words[n - 1], "&") == 0) {
            background = 1;
            n--;
        }
        if (n > 0) {
            run(words, n, background);
        }
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    char *args[] = { "sh", NULL };
    int pid, status;

    (void)argc;
    (void)argv;
    rc();
    for (;;) {
        pid = process_spawn("/bin/sh", 1, args);
        if (pid < 0) {
            printf("init: cannot run /bin/sh: %d\n", pid);
            return 1;
        }
        process_wait(pid, &status);
        if (status == 0) {
            return 0;
        }
        printf("init: sh exited with %d, starting another\n", status);
    }
}
