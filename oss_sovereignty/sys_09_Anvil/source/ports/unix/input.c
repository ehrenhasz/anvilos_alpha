 

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "py/mpstate.h"
#include "py/mphal.h"
#include "input.h"

#if MICROPY_USE_READLINE == 1
#include "shared/readline/readline.h"
#endif

#if MICROPY_USE_READLINE == 0
char *prompt(char *p) {
    
    static char buf[256];
    fputs(p, stdout);
    fflush(stdout);
    char *s = fgets(buf, sizeof(buf), stdin);
    if (!s) {
        return NULL;
    }
    int l = strlen(buf);
    if (buf[l - 1] == '\n') {
        buf[l - 1] = 0;
    } else {
        l++;
    }
    char *line = malloc(l);
    memcpy(line, buf, l);
    return line;
}
#endif

void prompt_read_history(void) {
    #if MICROPY_USE_READLINE_HISTORY
    #if MICROPY_USE_READLINE == 1
    readline_init0(); 
    char *home = getenv("HOME");
    if (home != NULL) {
        vstr_t vstr;
        vstr_init(&vstr, 50);
        vstr_printf(&vstr, "%s/.micropython.history", home);
        int fd = open(vstr_null_terminated_str(&vstr), O_RDONLY);
        if (fd != -1) {
            vstr_reset(&vstr);
            for (;;) {
                char c;
                int sz = read(fd, &c, 1);
                if (sz < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    break;
                }
                if (sz == 0 || c == '\n') {
                    readline_push_history(vstr_null_terminated_str(&vstr));
                    if (sz == 0) {
                        break;
                    }
                    vstr_reset(&vstr);
                } else {
                    vstr_add_byte(&vstr, c);
                }
            }
            close(fd);
        }
        vstr_clear(&vstr);
    }
    #endif
    #endif
}

void prompt_write_history(void) {
    #if MICROPY_USE_READLINE_HISTORY
    #if MICROPY_USE_READLINE == 1
    char *home = getenv("HOME");
    if (home != NULL) {
        vstr_t vstr;
        vstr_init(&vstr, 50);
        vstr_printf(&vstr, "%s/.micropython.history", home);
        int fd = open(vstr_null_terminated_str(&vstr), O_CREAT | O_TRUNC | O_WRONLY, 0644);
        if (fd != -1) {
            for (int i = MP_ARRAY_SIZE(MP_STATE_PORT(readline_hist)) - 1; i >= 0; i--) {
                const char *line = MP_STATE_PORT(readline_hist)[i];
                if (line != NULL) {
                    while (write(fd, line, strlen(line)) == -1 && errno == EINTR) {
                    }
                    while (write(fd, "\n", 1) == -1 && errno == EINTR) {
                    }
                }
            }
            close(fd);
        }
    }
    #endif
    #endif
}
