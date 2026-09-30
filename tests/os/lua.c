/* Stage 6: Lua as a Catflap program, reading a script from the host. */
#include "kernel.h"
#include "memfs.h"

void init_main(void)
{
    char *eval[] = { "lua", "-e", "print('hello from lua', _VERSION, 2^10)", NULL };
    char *script[] = { "lua", "/host/script.lua", "a", "b", NULL };
    int pid, status;

    start_memfs();
    pid = process_spawn("/bin/lua", 3, eval);
    process_wait(pid, &status);
    kprintf("lua -e exited %d\n", status);
    pid = process_spawn("/bin/lua", 4, script);
    process_wait(pid, &status);
    kprintf("lua script exited %d\n", status);
    kernel_halt(0);
}
