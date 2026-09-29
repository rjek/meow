/* sh: a small shell.  Words, 'quoted' or "quoted" to keep spaces,
   pipelines, < > >>, & at the end, # to the end of the line, and cd,
   pwd, exit as builtins.  Programs are found in /bin unless named with
   a slash. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "catflap.h"

#define MAXWORDS 32
#define MAXCMDS 8

struct cmd {
    char *argv[MAXWORDS];
    int argc;
    char *in, *out;
    int append;
};

static char *words[MAXWORDS];
static int nwords;

static void say(const char *s)
{
    fputs(s, stderr);
}

/* split the line into words, dropping comments */
static int tokenise(char *line)
{
    char *p = line;

    nwords = 0;
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            p++;
        }
        if (*p == '\0' || *p == '#') {
            return nwords;
        }
        if (nwords == MAXWORDS) {
            say("sh: too many words\n");
            return -1;
        }
        words[nwords++] = p;
        {
            char *w = p;                /* the word is rewritten in place, less its quotes */
            char quote = '\0';

            while (*p != '\0') {
                if (quote != '\0') {
                    if (*p == quote) {
                        quote = '\0';
                        p++;
                        continue;
                    }
                } else if (*p == '\'' || *p == '"') {
                    quote = *p++;
                    continue;
                } else if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
                    break;
                }
                *w++ = *p++;
            }
            if (quote != '\0') {
                say("sh: unmatched quote\n");
                return -1;
            }
            if (*p != '\0') {
                p++;
            }
            *w = '\0';
        }
    }
}

/* words into commands; returns the count, 0 for nothing, -1 for an error */
static int parse(struct cmd *cmds, int *background)
{
    int i, n = 0;
    struct cmd *c = NULL;

    *background = 0;
    for (i = 0; i < nwords; i++) {
        char *w = words[i];

        if (c == NULL) {
            if (n == MAXCMDS) {
                say("sh: pipeline too long\n");
                return -1;
            }
            c = &cmds[n++];
            memset(c, 0, sizeof *c);
        }
        if (strcmp(w, "|") == 0) {
            if (c->argc == 0) {
                say("sh: nothing before |\n");
                return -1;
            }
            c = NULL;
        } else if (strcmp(w, "&") == 0 && i == nwords - 1) {
            *background = 1;
        } else if (w[0] == '<' || w[0] == '>') {
            char *name = w[0] == '>' && w[1] == '>' ? w + 2 : w + 1;

            if (*name == '\0') {
                if (++i == nwords) {
                    say("sh: missing file name\n");
                    return -1;
                }
                name = words[i];
            }
            if (w[0] == '<') {
                c->in = name;
            } else {
                c->out = name;
                c->append = w[1] == '>';
            }
        } else if (c->argc == MAXWORDS - 1) {
            say("sh: too many arguments\n");
            return -1;
        } else {
            c->argv[c->argc++] = w;
        }
    }
    if (c != NULL && c->argc == 0) {
        say("sh: nothing after |\n");
        return -1;
    }
    return c == NULL && n > 0 && cmds[n - 1].argc == 0 ? -1 : n;
}

static int builtin(struct cmd *c)
{
    if (strcmp(c->argv[0], "cd") == 0) {
        int rc = vfs_chdir(c->argc > 1 ? c->argv[1] : "/");

        if (rc < 0) {
            fprintf(stderr, "cd: %s: error %d\n", c->argc > 1 ? c->argv[1] : "/", -rc);
        }
        return 1;
    }
    if (strcmp(c->argv[0], "pwd") == 0) {
        char cwd[128];

        vfs_getcwd(cwd, sizeof cwd);
        printf("%s\n", cwd);
        return 1;
    }
    if (strcmp(c->argv[0], "exit") == 0) {
        exit(c->argc > 1 ? atoi(c->argv[1]) : 0);
    }
    return 0;
}

/* run a pipeline: each command with its 0 and 1 arranged before the
   spawn copies them, then put back */
static void run(struct cmd *cmds, int n, int background)
{
    int pids[MAXCMDS], i, prev = -1;
    int save0 = vfs_dup(0), save1 = vfs_dup(1);

    if (n == 1 && builtin(&cmds[0])) {
        vfs_close(save0);
        vfs_close(save1);
        return;
    }
    for (i = 0; i < n; i++) {
        struct cmd *c = &cmds[i];
        char path[128];
        int pipefd[2] = { -1, -1 }, fd;

        pids[i] = -1;
        if (i < n - 1 && vfs_pipe(pipefd) < 0) {
            say("sh: cannot make a pipe\n");
            break;
        }
        if (prev >= 0) {
            vfs_dup2(prev, 0);
            vfs_close(prev);
        }
        if (c->in != NULL) {
            fd = vfs_open(c->in, CF_O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "sh: %s: error %d\n", c->in, -fd);
            } else {
                vfs_dup2(fd, 0);
                vfs_close(fd);
            }
        }
        if (pipefd[1] >= 0) {
            vfs_dup2(pipefd[1], 1);
            vfs_close(pipefd[1]);
        }
        if (c->out != NULL) {
            fd = vfs_open(c->out, CF_O_WRONLY | CF_O_CREAT | (c->append ? CF_O_APPEND : CF_O_TRUNC));
            if (fd < 0) {
                fprintf(stderr, "sh: %s: error %d\n", c->out, -fd);
            } else {
                vfs_dup2(fd, 1);
                vfs_close(fd);
            }
        }
        if (strchr(c->argv[0], '/') != NULL) {
            strcpy(path, c->argv[0]);
        } else {
            sprintf(path, "/bin/%s", c->argv[0]);
        }
        pids[i] = process_spawn(path, c->argc, c->argv);
        if (pids[i] < 0) {
            fprintf(stderr, "sh: %s: %s\n", c->argv[0],
                    pids[i] == -2 ? "not found" : pids[i] == -8 ? "not a program" :
                    pids[i] == -12 ? "out of memory" : "cannot run");
        }
        vfs_dup2(save0, 0);
        vfs_dup2(save1, 1);
        prev = pipefd[0];
    }
    if (prev >= 0) {
        vfs_close(prev);
    }
    vfs_close(save0);
    vfs_close(save1);
    for (i = 0; i < n; i++) {
        int status;

        if (pids[i] >= 0 && background == 0) {
            process_wait(pids[i], &status);
            if (status != 0 && i == n - 1) {
                fprintf(stderr, "sh: %s: exit %d\n", cmds[i].argv[0], status);
            }
        } else if (pids[i] >= 0 && i == n - 1) {
            fprintf(stderr, "[%d]\n", pids[i]);
        }
    }
}

int main(int argc, char **argv)
{
    char line[256];
    struct cmd cmds[MAXCMDS];
    int n, background;

    (void)argc;
    (void)argv;
    for (;;) {
        int pid, status;

        while ((pid = process_waitany(&status, 0)) > 0) {
            fprintf(stderr, "[%d] done, exit %d\n", pid, status);
        }
        fputs("$ ", stdout);
        fflush(stdout);
        if (fgets(line, sizeof line, stdin) == NULL) {
            fputs("\n", stdout);
            return 0;
        }
        if (tokenise(line) <= 0) {
            continue;
        }
        n = parse(cmds, &background);
        if (n > 0) {
            run(cmds, n, background);
        }
    }
}
