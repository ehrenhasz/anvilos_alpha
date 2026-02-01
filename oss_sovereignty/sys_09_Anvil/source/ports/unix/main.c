#include "py/persistentcode.h"
#if MICROPY_ENABLE_COMPILER
#error COMPILER_IS_ENABLED
#endif
 

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>

#include "py/compile.h"
#include "py/runtime.h"
#include "py/builtin.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/objstr.h"
#include "py/stackctrl.h"
#include "py/mphal.h"
#include "py/mpthread.h"
#include "extmod/misc.h"
#include "extmod/modplatform.h"
#include "extmod/vfs.h"
#include "extmod/vfs_posix.h"
#include "genhdr/mpversion.h"
#include "input.h"


static bool compile_only = false;
static uint emit_opt = MP_EMIT_OPT_NONE;

#if MICROPY_ENABLE_GC


long heap_size = 1024 * 1024 * (sizeof(mp_uint_t) / 4);
#endif


#ifndef MICROPY_GC_SPLIT_HEAP_N_HEAPS
#define MICROPY_GC_SPLIT_HEAP_N_HEAPS (1)
#endif

#if !MICROPY_PY_SYS_PATH
#error "The unix port requires MICROPY_PY_SYS_PATH=1"
#endif

#if !MICROPY_PY_SYS_ARGV
#error "The unix port requires MICROPY_PY_SYS_ARGV=1"
#endif

static void stderr_print_strn(void *env, const char *str, size_t len) {
    (void)env;
    ssize_t ret;
    MP_HAL_RETRY_SYSCALL(ret, write(STDERR_FILENO, str, len), {});
    mp_os_dupterm_tx_strn(str, len);
}

const mp_print_t mp_stderr_print = {NULL, stderr_print_strn};

#define FORCED_EXIT (0x100)



static int handle_uncaught_exception(mp_obj_base_t *exc) {
    
    if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(exc->type), MP_OBJ_FROM_PTR(&mp_type_SystemExit))) {
        
        mp_obj_t exit_val = mp_obj_exception_get_value(MP_OBJ_FROM_PTR(exc));
        mp_int_t val = 0;
        if (exit_val != mp_const_none && !mp_obj_get_int_maybe(exit_val, &val)) {
            val = 1;
        }
        return FORCED_EXIT | (val & 255);
    }

    
    mp_obj_print_exception(&mp_stderr_print, MP_OBJ_FROM_PTR(exc));
    return 1;
}

#define LEX_SRC_STR (1)
#define LEX_SRC_VSTR (2)
#define LEX_SRC_FILENAME (3)
#define LEX_SRC_STDIN (4)




static int execute_from_lexer(int source_kind, const void *source, mp_parse_input_kind_t input_kind, bool is_repl) {
#if !MICROPY_ENABLE_COMPILER
    if (source_kind == LEX_SRC_FILENAME) { nlr_buf_t nlr; if (nlr_push(&nlr) == 0) { const char *filename = (const char *)source; mp_compiled_module_t cm; cm.context = m_new_obj(mp_module_context_t); cm.context->module.base.type = &mp_type_module; cm.context->module.globals = mp_globals_get(); mp_raw_code_load_file(qstr_from_str(filename), &cm); mp_obj_t module_fun = mp_make_function_from_proto_fun(cm.rc, cm.context, NULL); mp_call_function_0(module_fun); nlr_pop(); return 0; } else { mp_obj_print_exception(&mp_stderr_print, (mp_obj_t)nlr.ret_val); return 1; } } mp_printf(&mp_stderr_print, "Error: Compiler disabled and not MPY.\n");
    return 1;
}
static int __attribute__((unused)) _unused_execute_from_lexer(int source_kind, const void *source, mp_parse_input_kind_t input_kind, bool is_repl) {
#endif
    mp_hal_set_interrupt_char(CHAR_CTRL_C);

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        
        mp_lexer_t *lex;
        if (source_kind == LEX_SRC_STR) {
            const char *line = source;
            lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, line, strlen(line), false);
        } else if (source_kind == LEX_SRC_VSTR) {
            const vstr_t *vstr = source;
            lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, vstr->buf, vstr->len, false);
        } else if (source_kind == LEX_SRC_FILENAME) {
            const char *filename = (const char *)source;
            lex = mp_lexer_new_from_file(qstr_from_str(filename));
        } else { 
            lex = mp_lexer_new_from_fd(MP_QSTR__lt_stdin_gt_, 0, false);
        }

        qstr source_name = lex->source_name;

        #if MICROPY_PY___FILE__
        if (input_kind == MP_PARSE_FILE_INPUT) {
            mp_store_global(MP_QSTR___file__, MP_OBJ_NEW_QSTR(source_name));
        }
        #endif

        mp_parse_tree_t parse_tree = mp_parse(lex, input_kind);

        #if defined(MICROPY_UNIX_COVERAGE)
        
        if (mp_verbose_flag >= 3) {
            printf("----------------\n");
            mp_parse_node_print(&mp_plat_print, parse_tree.root, 0);
            printf("----------------\n");
        }
        #endif

        mp_obj_t module_fun = mp_compile(&parse_tree, source_name, is_repl);

        if (!compile_only) {
            
            mp_call_function_0(module_fun);
        }

        mp_hal_set_interrupt_char(-1);
        mp_handle_pending(true);
        nlr_pop();
        return 0;

    } else {
        
        mp_hal_set_interrupt_char(-1);
        mp_handle_pending(false);
        return handle_uncaught_exception(nlr.ret_val);
    }
}

#if MICROPY_USE_READLINE == 1
#include "shared/readline/readline.h"
#else
static char *strjoin(const char *s1, int sep_char, const char *s2) {
    int l1 = strlen(s1);
    int l2 = strlen(s2);
    char *s = malloc(l1 + l2 + 2);
    memcpy(s, s1, l1);
    if (sep_char != 0) {
        s[l1] = sep_char;
        l1 += 1;
    }
    memcpy(s + l1, s2, l2);
    s[l1 + l2] = 0;
    return s;
}
#endif

static int do_repl(void) {
    mp_hal_stdout_tx_str(MICROPY_BANNER_NAME_AND_VERSION);
    mp_hal_stdout_tx_str("; " MICROPY_BANNER_MACHINE);
    mp_hal_stdout_tx_str("\nUse Ctrl-D to exit, Ctrl-E for paste mode\n");

    #if MICROPY_USE_READLINE == 1

    

    vstr_t line;
    vstr_init(&line, 16);
    for (;;) {
        mp_hal_stdio_mode_raw();

    input_restart:
        vstr_reset(&line);
        int ret = readline(&line, mp_repl_get_ps1());
        mp_parse_input_kind_t parse_input_kind = MP_PARSE_SINGLE_INPUT;

        if (ret == CHAR_CTRL_C) {
            
            mp_hal_stdout_tx_str("\r\n");
            goto input_restart;
        } else if (ret == CHAR_CTRL_D) {
            
            printf("\n");
            mp_hal_stdio_mode_orig();
            vstr_clear(&line);
            return 0;
        } else if (ret == CHAR_CTRL_E) {
            
            mp_hal_stdout_tx_str("\npaste mode; Ctrl-C to cancel, Ctrl-D to finish\n=== ");
            vstr_reset(&line);
            for (;;) {
                char c = mp_hal_stdin_rx_chr();
                if (c == CHAR_CTRL_C) {
                    
                    mp_hal_stdout_tx_str("\n");
                    goto input_restart;
                } else if (c == CHAR_CTRL_D) {
                    
                    mp_hal_stdout_tx_str("\n");
                    break;
                } else {
                    
                    vstr_add_byte(&line, c);
                    if (c == '\r') {
                        mp_hal_stdout_tx_str("\n=== ");
                    } else {
                        mp_hal_stdout_tx_strn(&c, 1);
                    }
                }
            }
            parse_input_kind = MP_PARSE_FILE_INPUT;
        } else if (line.len == 0) {
            if (ret != 0) {
                printf("\n");
            }
            goto input_restart;
        } else {
            
            while (mp_repl_continue_with_input(vstr_null_terminated_str(&line))) {
                vstr_add_byte(&line, '\n');
                ret = readline(&line, mp_repl_get_ps2());
                if (ret == CHAR_CTRL_C) {
                    
                    printf("\n");
                    goto input_restart;
                } else if (ret == CHAR_CTRL_D) {
                    
                    break;
                }
            }
        }

        mp_hal_stdio_mode_orig();

        ret = execute_from_lexer(LEX_SRC_VSTR, &line, parse_input_kind, true);
        if (ret & FORCED_EXIT) {
            return ret;
        }
    }

    #else

    

    for (;;) {
        char *line = prompt((char *)mp_repl_get_ps1());
        if (line == NULL) {
            
            return 0;
        }
        while (mp_repl_continue_with_input(line)) {
            char *line2 = prompt((char *)mp_repl_get_ps2());
            if (line2 == NULL) {
                break;
            }
            char *line3 = strjoin(line, '\n', line2);
            free(line);
            free(line2);
            line = line3;
        }

        int ret = execute_from_lexer(LEX_SRC_STR, line, MP_PARSE_SINGLE_INPUT, true);
        free(line);
        if (ret & FORCED_EXIT) {
            return ret;
        }
    }

    #endif
}

static int do_file(const char *file) {
    return execute_from_lexer(LEX_SRC_FILENAME, file, MP_PARSE_FILE_INPUT, false);
}

static int do_str(const char *str) {
    return execute_from_lexer(LEX_SRC_STR, str, MP_PARSE_FILE_INPUT, false);
}

static void print_help(char **argv) {
    printf(
        "usage: %s [<opts>] [-X <implopt>] [-c <command> | -m <module> | <filename>]\n"
        "Options:\n"
        "-h : print this help message\n"
        "-i : enable inspection via REPL after running command/module/file\n"
        #if MICROPY_DEBUG_PRINTERS
        "-v : verbose (trace various operations); can be multiple\n"
        #endif
        "-O[N] : apply bytecode optimizations of level N\n"
        "\n"
        "Implementation specific options (-X):\n", argv[0]
        );
    int impl_opts_cnt = 0;
    printf(
        "  compile-only                 -- parse and compile only\n"
        #if MICROPY_EMIT_NATIVE
        "  emit={bytecode,native,viper} -- set the default code emitter\n"
        #else
        "  emit=bytecode                -- set the default code emitter\n"
        #endif
        );
    impl_opts_cnt++;
    #if MICROPY_ENABLE_GC
    printf(
        "  heapsize=<n>[w][K|M] -- set the heap size for the GC (default %ld)\n"
        , heap_size);
    impl_opts_cnt++;
    #endif
    #if defined(__APPLE__)
    printf("  realtime -- set thread priority to realtime\n");
    impl_opts_cnt++;
    #endif

    if (impl_opts_cnt == 0) {
        printf("  (none)\n");
    }
}

static int invalid_args(void) {
    fprintf(stderr, "Invalid command line arguments. Use -h option for help.\n");
    return 1;
}


static void pre_process_options(int argc, char **argv) {
    for (int a = 1; a < argc; a++) {
        if (argv[a][0] == '-') {
            if (strcmp(argv[a], "-c") == 0 || strcmp(argv[a], "-m") == 0) {
                break; 
            }
            if (strcmp(argv[a], "-h") == 0) {
                print_help(argv);
                exit(0);
            }
            if (strcmp(argv[a], "-X") == 0) {
                if (a + 1 >= argc) {
                    exit(invalid_args());
                }
                if (0) {
                } else if (strcmp(argv[a + 1], "compile-only") == 0) {
                    compile_only = true;
                } else if (strcmp(argv[a + 1], "emit=bytecode") == 0) {
                    emit_opt = MP_EMIT_OPT_BYTECODE;
                #if MICROPY_EMIT_NATIVE
                } else if (strcmp(argv[a + 1], "emit=native") == 0) {
                    emit_opt = MP_EMIT_OPT_NATIVE_PYTHON;
                } else if (strcmp(argv[a + 1], "emit=viper") == 0) {
                    emit_opt = MP_EMIT_OPT_VIPER;
                #endif
                #if MICROPY_ENABLE_GC
                } else if (strncmp(argv[a + 1], "heapsize=", sizeof("heapsize=") - 1) == 0) {
                    char *end;
                    heap_size = strtol(argv[a + 1] + sizeof("heapsize=") - 1, &end, 0);
                    
                    
                    
                    
                    
                    
                    bool word_adjust = false;
                    if ((*end | 0x20) == 'w') {
                        word_adjust = true;
                        end++;
                    }
                    if ((*end | 0x20) == 'k') {
                        heap_size *= 1024;
                    } else if ((*end | 0x20) == 'm') {
                        heap_size *= 1024 * 1024;
                    } else {
                        
                        --end;
                    }
                    if (*++end != 0) {
                        goto invalid_arg;
                    }
                    if (word_adjust) {
                        heap_size = heap_size * MP_BYTES_PER_OBJ_WORD / 4;
                    }
                    
                    if (heap_size < 700) {
                        goto invalid_arg;
                    }
                #endif
                #if defined(__APPLE__)
                } else if (strcmp(argv[a + 1], "realtime") == 0) {
                    #if MICROPY_PY_THREAD
                    mp_thread_is_realtime_enabled = true;
                    #endif
                    
                    
                    mp_thread_set_realtime();
                #endif
                } else {
                invalid_arg:
                    exit(invalid_args());
                }
                a++;
            }
        } else {
            break; 
        }
    }
}

static void set_sys_argv(char *argv[], int argc, int start_arg) {
    for (int i = start_arg; i < argc; i++) {
        mp_obj_list_append(mp_sys_argv, MP_OBJ_NEW_QSTR(qstr_from_str(argv[i])));
    }
}

#if MICROPY_PY_SYS_EXECUTABLE
extern mp_obj_str_t mp_sys_executable_obj;
static char executable_path[MICROPY_ALLOC_PATH_MAX];

static void sys_set_excecutable(char *argv0) {
    if (realpath(argv0, executable_path)) {
        mp_obj_str_set_data(&mp_sys_executable_obj, (byte *)executable_path, strlen(executable_path));
    }
}
#endif

#ifdef _WIN32
#define PATHLIST_SEP_CHAR ';'
#else
#define PATHLIST_SEP_CHAR ':'
#endif

MP_NOINLINE int main_(int argc, char **argv);

int main(int argc, char **argv) {
    #if MICROPY_PY_THREAD
    mp_thread_init();
    #endif
    
    
    
    
    
    mp_stack_ctrl_init();
    return main_(argc, argv);
}

MP_NOINLINE int main_(int argc, char **argv) {
    #ifdef SIGPIPE
    
    
    
    
    
    
    
    
    
    
    signal(SIGPIPE, SIG_IGN);
    #endif

    
    mp_uint_t stack_limit = 40000 * (sizeof(void *) / 4);
    #if defined(__arm__) && !defined(__thumb2__)
    
    stack_limit *= 2;
    #endif
    mp_stack_set_limit(stack_limit);

    pre_process_options(argc, argv);

    #if MICROPY_ENABLE_GC
    #if !MICROPY_GC_SPLIT_HEAP
    char *heap = malloc(heap_size);
    gc_init(heap, heap + heap_size);
    #else
    assert(MICROPY_GC_SPLIT_HEAP_N_HEAPS > 0);
    char *heaps[MICROPY_GC_SPLIT_HEAP_N_HEAPS];
    long multi_heap_size = heap_size / MICROPY_GC_SPLIT_HEAP_N_HEAPS;
    for (size_t i = 0; i < MICROPY_GC_SPLIT_HEAP_N_HEAPS; i++) {
        heaps[i] = malloc(multi_heap_size);
        if (i == 0) {
            gc_init(heaps[i], heaps[i] + multi_heap_size);
        } else {
            gc_add(heaps[i], heaps[i] + multi_heap_size);
        }
    }
    #endif
    #endif

    #if MICROPY_ENABLE_PYSTACK
    static mp_obj_t pystack[1024];
    mp_pystack_init(pystack, &pystack[MP_ARRAY_SIZE(pystack)]);
    #endif

    mp_init();

    #if MICROPY_EMIT_NATIVE
    
    #if MICROPY_ENABLE_COMPILER
    MP_STATE_VM(default_emit_opt) = emit_opt;
#endif
    #else
    (void)emit_opt;
    #endif

    #if MICROPY_VFS_POSIX
    {
        
        mp_obj_t args[2] = {
            MP_OBJ_TYPE_GET_SLOT(&mp_type_vfs_posix, make_new)(&mp_type_vfs_posix, 0, 0, NULL),
            MP_OBJ_NEW_QSTR(MP_QSTR__slash_),
        };
        mp_vfs_mount(2, args, (mp_map_t *)&mp_const_empty_map);
        MP_STATE_VM(vfs_cur) = MP_STATE_VM(vfs_mount_table);
    }
    #endif

    {
        
        mp_sys_path = mp_obj_new_list(0, NULL);
        mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR_));

        
        char *home = getenv("HOME");
        char *path = getenv("MICROPYPATH");
        if (path == NULL) {
            path = MICROPY_PY_SYS_PATH_DEFAULT;
        }
        if (*path == PATHLIST_SEP_CHAR) {
            
            ++path;
        }
        bool path_remaining = *path;
        while (path_remaining) {
            char *path_entry_end = strchr(path, PATHLIST_SEP_CHAR);
            if (path_entry_end == NULL) {
                path_entry_end = path + strlen(path);
                path_remaining = false;
            }
            if (path[0] == '~' && path[1] == '/' && home != NULL) {
                
                int home_l = strlen(home);
                vstr_t vstr;
                vstr_init(&vstr, home_l + (path_entry_end - path - 1) + 1);
                vstr_add_strn(&vstr, home, home_l);
                vstr_add_strn(&vstr, path + 1, path_entry_end - path - 1);
                mp_obj_list_append(mp_sys_path, mp_obj_new_str_from_vstr(&vstr));
            } else {
                mp_obj_list_append(mp_sys_path, mp_obj_new_str_via_qstr(path, path_entry_end - path));
            }
            path = path_entry_end + 1;
        }
    }

    mp_obj_list_init(MP_OBJ_TO_PTR(mp_sys_argv), 0);

    #if defined(MICROPY_UNIX_COVERAGE)
    {
        MP_DECLARE_CONST_FUN_OBJ_0(extra_coverage_obj);
        MP_DECLARE_CONST_FUN_OBJ_0(extra_cpp_coverage_obj);
        mp_store_global(MP_QSTR_extra_coverage, MP_OBJ_FROM_PTR(&extra_coverage_obj));
        mp_store_global(MP_QSTR_extra_cpp_coverage, MP_OBJ_FROM_PTR(&extra_cpp_coverage_obj));
    }
    #endif

    
    
    
    
    
    
    
    
    
    
    
    

     

    #if MICROPY_PY_SYS_EXECUTABLE
    sys_set_excecutable(argv[0]);
    #endif

    const int NOTHING_EXECUTED = -2;
    int ret = NOTHING_EXECUTED;
    bool inspect = false;
    for (int a = 1; a < argc; a++) {
        if (argv[a][0] == '-') {
            if (strcmp(argv[a], "-i") == 0) {
                inspect = true;
            } else if (strcmp(argv[a], "-c") == 0) {
                if (a + 1 >= argc) {
                    return invalid_args();
                }
                set_sys_argv(argv, a + 1, a); 
                set_sys_argv(argv, argc, a + 2); 
                ret = do_str(argv[a + 1]);
                break;
            } else if (strcmp(argv[a], "-m") == 0) {
                if (a + 1 >= argc) {
                    return invalid_args();
                }
                mp_obj_t import_args[4];
                import_args[0] = mp_obj_new_str(argv[a + 1], strlen(argv[a + 1]));
                import_args[1] = import_args[2] = mp_const_none;
                
                
                
                import_args[3] = mp_const_false;
                
                
                
                
                set_sys_argv(argv, argc, a + 1);

                mp_obj_t mod;
                nlr_buf_t nlr;

                
                
                
                static bool subpkg_tried;
                subpkg_tried = false;

            reimport:
                if (nlr_push(&nlr) == 0) {
                    mod = mp_builtin___import__(MP_ARRAY_SIZE(import_args), import_args);
                    nlr_pop();
                } else {
                    
                    return handle_uncaught_exception(nlr.ret_val) & 0xff;
                }

                
                mp_obj_t dest[2];
                mp_load_method_protected(mod, MP_QSTR___path__, dest, true);
                if (dest[0] != MP_OBJ_NULL && !subpkg_tried) {
                    subpkg_tried = true;
                    vstr_t vstr;
                    int len = strlen(argv[a + 1]);
                    vstr_init(&vstr, len + sizeof(".__main__"));
                    vstr_add_strn(&vstr, argv[a + 1], len);
                    vstr_add_strn(&vstr, ".__main__", sizeof(".__main__") - 1);
                    import_args[0] = mp_obj_new_str_from_vstr(&vstr);
                    goto reimport;
                }

                ret = 0;
                break;
            } else if (strcmp(argv[a], "-X") == 0) {
                a += 1;
            #if MICROPY_DEBUG_PRINTERS
            } else if (strcmp(argv[a], "-v") == 0) {
                mp_verbose_flag++;
            #endif
            } else if (strncmp(argv[a], "-O", 2) == 0) {
                if (unichar_isdigit(argv[a][2])) {
                    #if MICROPY_ENABLE_COMPILER
                    MP_STATE_VM(mp_optimise_value) = argv[a][2] & 0xf;
#endif
                } else {
                    #if MICROPY_ENABLE_COMPILER
                    MP_STATE_VM(mp_optimise_value) = 0;
#endif
                    for (char *p = argv[a] + 1; *p && *p == 'O'; p++) {
#if MICROPY_ENABLE_COMPILER
                        MP_STATE_VM(mp_optimise_value)++;
#endif
                    }
                }
            } else {
                return invalid_args();
            }
        } else {
            char *pathbuf = malloc(PATH_MAX);
            char *basedir = realpath(argv[a], pathbuf);
            if (basedir == NULL) {
                mp_printf(&mp_stderr_print, "%s: can't open file '%s': [Errno %d] %s\n", argv[0], argv[a], errno, strerror(errno));
                free(pathbuf);
                
                ret = 2;
                break;
            }

            
            char *p = strrchr(basedir, '/');
            mp_obj_list_store(mp_sys_path, MP_OBJ_NEW_SMALL_INT(0), mp_obj_new_str_via_qstr(basedir, p - basedir));
            free(pathbuf);

            set_sys_argv(argv, argc, a);
            ret = do_file(argv[a]);
            break;
        }
    }

    const char *inspect_env = getenv("MICROPYINSPECT");
    if (inspect_env && inspect_env[0] != '\0') {
        inspect = true;
    }
    if (ret == NOTHING_EXECUTED || inspect) {
        if (isatty(0) || inspect) {
            prompt_read_history();
            ret = do_repl();
            prompt_write_history();
        } else {
            ret = execute_from_lexer(LEX_SRC_STDIN, NULL, MP_PARSE_FILE_INPUT, false);
        }
    }

    #if MICROPY_PY_SYS_SETTRACE
    MP_STATE_THREAD(prof_trace_callback) = MP_OBJ_NULL;
    #endif

    #if MICROPY_PY_SYS_ATEXIT
    
    if (mp_obj_is_callable(MP_STATE_VM(sys_exitfunc))) {
        mp_call_function_0(MP_STATE_VM(sys_exitfunc));
    }
    #endif

    #if MICROPY_PY_MICROPYTHON_MEM_INFO
    if (mp_verbose_flag) {
        mp_micropython_mem_info(0, NULL);
    }
    #endif

    #if MICROPY_PY_BLUETOOTH
    void mp_bluetooth_deinit(void);
    mp_bluetooth_deinit();
    #endif

    #if MICROPY_PY_THREAD
    mp_thread_deinit();
    #endif

    #if defined(MICROPY_UNIX_COVERAGE)
    gc_sweep_all();
    #endif

    mp_deinit();

    #if MICROPY_ENABLE_GC && !defined(NDEBUG)
    
    
    #if !MICROPY_GC_SPLIT_HEAP
    free(heap);
    #else
    for (size_t i = 0; i < MICROPY_GC_SPLIT_HEAP_N_HEAPS; i++) {
        free(heaps[i]);
    }
    #endif
    #endif

    
    return ret & 0xff;
}

void nlr_jump_fail(void *val) {
    #if MICROPY_USE_READLINE == 1
    mp_hal_stdio_mode_orig();
    #endif
    fprintf(stderr, "FATAL: uncaught NLR %p\n", val);
    exit(1);
}
