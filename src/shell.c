#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "pmm.h"
#include "scheduler.h"
#include "thread.h"
#include "pit.h"
#include "types.h"

#define MAX_LINE  256
#define MAX_ARGS  16

/* ── String helpers ─────────────────────────────────────────────────────── */

static int sh_strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int sh_strncmp(const char* a, const char* b, int n) __attribute__((unused));
static int sh_strncmp(const char* a, const char* b, int n) {
    while (n-- && *a && *b && *a == *b) { a++; b++; }
    if (n < 0) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

static void sh_itoa(uint32_t n, char* buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[12]; int i = 0;
    while (n) { tmp[i++] = (char)('0' + n % 10); n /= 10; }
    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

/* ── Delay helper ───────────────────────────────────────────────────────── */
static void sh_delay(uint32_t n) {
    volatile uint32_t i;
    for (i = 0; i < n; i++) __asm__ volatile("nop");
}

/* ── Command implementations ────────────────────────────────────────────── */

static void cmd_help(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_print_color("\n=== Vamos OS Commands ===\n", 0x0B);
    vga_print_color("  help              ", 0x0E); vga_print("Show this help list\n");
    vga_print_color("  clear             ", 0x0E); vga_print("Clear the screen\n");
    vga_print_color("  echo <text>       ", 0x0E); vga_print("Print text to screen\n");
    vga_print_color("  ls                ", 0x0E); vga_print("List running processes and threads\n");
    vga_print_color("  time              ", 0x0E); vga_print("Show system uptime\n");
    vga_print_color("  ps                ", 0x0E); vga_print("List running processes\n");
    vga_print_color("  threads           ", 0x0E); vga_print("List kernel threads\n");
    vga_print_color("  meminfo           ", 0x0E); vga_print("Show memory usage\n");
    vga_print_color("  spawn             ", 0x0E); vga_print("Start a new demo process\n");
    vga_print_color("  thread            ", 0x0E); vga_print("Start a new kernel thread\n");
    vga_print_color("  reboot            ", 0x0E); vga_print("Reboot the system\n");
    vga_print_color("  exit              ", 0x0E); vga_print("Shut down Vamos OS\n");
    vga_print_color("  about             ", 0x0E); vga_print("About Vamos OS\n");
    vga_print_color("  banner            ", 0x0E); vga_print("Show OS banner\n");
    vga_print("\n");
}

static void cmd_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_clear();
    vga_draw_statusbar();
}

static void cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        vga_print(argv[i]);
        if (i < argc - 1) vga_putchar(' ');
    }
    vga_putchar('\n');
}

static void cmd_time(int argc, char** argv) {
    (void)argc; (void)argv;
    uint32_t ticks = get_tick_count();
    uint32_t secs  = ticks / 100;
    uint32_t mins  = secs / 60;
    uint32_t hrs   = mins / 60;
    secs %= 60; mins %= 60;

    char buf[8];
    vga_print_color("Uptime: ", 0x0B);
    sh_itoa(hrs,  buf); vga_print(buf); vga_print("h ");
    sh_itoa(mins, buf); vga_print(buf); vga_print("m ");
    sh_itoa(secs, buf); vga_print(buf); vga_print("s");
    vga_print_color("  (", 0x08);
    sh_itoa(ticks, buf); vga_print_color(buf, 0x08);
    vga_print_color(" ticks @ 100Hz)\n", 0x08);
}

static const char* state_name(process_state_t s) {
    switch (s) {
        case PROCESS_READY:   return "READY  ";
        case PROCESS_RUNNING: return "RUNNING";
        case PROCESS_BLOCKED: return "BLOCKED";
        case PROCESS_DEAD:    return "DEAD   ";
        default:              return "?      ";
    }
}

extern pcb_t* run_queue;

static void cmd_ps(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_print_color("\nPID  STATE    PRI  NAME\n", 0x0B);
    vga_print_color("---  -------  ---  ----\n", 0x08);
    pcb_t* p = run_queue;
    char buf[12];
    while (p) {
        if (p->pid != 0) {
            sh_itoa(p->pid, buf);
            vga_print(buf); vga_print("    ");
            vga_print_color(state_name(p->state), 0x0A);
            vga_print("  ");
            sh_itoa(p->priority, buf); vga_print(buf);
            vga_print("    kernel\n");
        }
        p = p->next;
    }
    vga_putchar('\n');
}

static void cmd_threads(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_print_color("\nTID  STATE    PARENT\n", 0x0B);
    vga_print_color("---  -------  ------\n", 0x08);
    pcb_t* p = run_queue;
    char buf[12];
    while (p) {
        if (p->pid == 0) {
            tcb_t* t = (tcb_t*)p;
            sh_itoa(t->tid, buf); vga_print(buf); vga_print("    ");
            vga_print_color(state_name(p->state), 0x0A); vga_print("  ");
            if (t->parent) { sh_itoa(t->parent->pid, buf); vga_print(buf); }
            else vga_print("none");
            vga_putchar('\n');
        }
        p = p->next;
    }
    vga_putchar('\n');
}

static void cmd_meminfo(int argc, char** argv) {
    (void)argc; (void)argv;
    uint32_t free_p  = pmm_get_free_page_count();
    uint32_t total_p = 32768; /* 128 MB / 4KB */
    uint32_t used_p  = total_p - free_p;
    char buf[12];

    vga_print_color("\n=== Memory Info ===\n", 0x0B);
    vga_print("  Total : "); sh_itoa(total_p * 4, buf); vga_print(buf); vga_print(" KB\n");
    vga_print("  Used  : "); sh_itoa(used_p  * 4, buf); vga_print(buf); vga_print(" KB\n");
    vga_print_color("  Free  : ", 0x0A); sh_itoa(free_p * 4, buf);
    vga_print_color(buf, 0x0A); vga_print_color(" KB\n\n", 0x0A);
}

static void cmd_reboot(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_print_color("Rebooting...\n", 0x0C);
    sh_delay(5000000);
    /* Pulse keyboard controller reset line */
    __asm__ volatile("cli");
    /* Wait for keyboard buffer empty */
    volatile uint32_t i;
    for (i = 0; i < 100000; i++) {
        uint8_t s;
        __asm__ volatile("inb $0x64, %0" : "=a"(s));
        if (!(s & 0x02)) break;
    }
    __asm__ volatile("outb %0, $0x64" : : "a"((uint8_t)0xFE));
    /* If that didn't work, triple fault */
    for (;;) __asm__ volatile("hlt");
}

static void cmd_about(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_print_color("\n  Vamos OS v1.0\n", 0x0B);
    vga_print("  A minimal x86 kernel written in C and Assembly.\n");
    vga_print("  Architecture : x86 (i686) protected mode\n");
    vga_print("  Bootloader   : GRUB Multiboot\n");
    vga_print("  Memory       : Manual bitmap allocator (4KB pages)\n");
    vga_print("  Scheduler    : Preemptive round-robin (100Hz)\n");
    vga_print("  Shell        : Built-in kernel-space shell\n\n");
}

static void cmd_banner(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_print_color("\n__     __                              ___  ____  \n", 0x0B);
    vga_print_color("\\ \\   / /__ _ _ __ ___   ___  ___    / _ \\/ ___| \n", 0x0B);
    vga_print_color(" \\ \\ / / _` | '_ ` _ \\ / _ \\/ __|  | | | \\___ \\ \n", 0x0B);
    vga_print_color("  \\ V / (_| | | | | | | (_) \\__ \\  | |_| |___) |\n", 0x0B);
    vga_print_color("   \\_/ \\__,_|_| |_| |_|\\___/|___/   \\___/|____/ \n", 0x0B);
    vga_print_color("           Vamos OS v1.0\n\n", 0x0E);
}

/* ── ls: show processes + threads in one view ───────────────────────────── */
static void cmd_ls(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_print_color("\nProcesses:\n", 0x0B);
    vga_print_color("  PID   STATE     NAME\n", 0x08);
    pcb_t* p = run_queue;
    char buf[12];
    int found = 0;
    while (p) {
        if (p->pid != 0) {
            vga_print("  ");
            sh_itoa(p->pid, buf); vga_print(buf);
            vga_print("     ");
            vga_print_color(state_name(p->state), 0x0A);
            vga_print("  kernel\n");
            found = 1;
        }
        p = p->next;
    }
    if (!found) vga_print("  (none)\n");

    vga_print_color("\nThreads:\n", 0x0B);
    vga_print_color("  TID   STATE     PARENT\n", 0x08);
    p = run_queue;
    found = 0;
    while (p) {
        if (p->pid == 0) {
            tcb_t* t = (tcb_t*)p;
            vga_print("  ");
            sh_itoa(t->tid, buf); vga_print(buf);
            vga_print("     ");
            vga_print_color(state_name(p->state), 0x0A);
            vga_print("  pid=");
            if (t->parent) { sh_itoa(t->parent->pid, buf); vga_print(buf); }
            else vga_print("none");
            vga_putchar('\n');
            found = 1;
        }
        p = p->next;
    }
    if (!found) vga_print("  (none)\n");
    vga_putchar('\n');
}

/* ── spawn: start a new demo process ────────────────────────────────────── */
static uint32_t spawn_counter = 0;

static void spawned_task(void) {
    /* Each spawned process prints its PID a few times then exits */
    pcb_t* me = get_current_process();
    char buf[12];
    uint32_t pid = me ? me->pid : 0;
    for (int i = 0; i < 5; i++) {
        vga_print_color("\n[spawned process pid=", 0x0D);
        sh_itoa(pid, buf); vga_print_color(buf, 0x0D);
        vga_print_color(" tick ", 0x0D);
        sh_itoa((uint32_t)i, buf); vga_print_color(buf, 0x0D);
        vga_print_color("]", 0x0D);
        /* busy wait */
        volatile uint32_t j;
        for (j = 0; j < 5000000; j++) __asm__ volatile("nop");
    }
    process_exit();
}

static void cmd_spawn(int argc, char** argv) {
    (void)argc; (void)argv;
    spawn_counter++;
    pcb_t* p = create_process(spawned_task, 1);
    if (p) {
        char buf[12];
        vga_print_color("Spawned process with PID ", 0x0A);
        sh_itoa(p->pid, buf); vga_print_color(buf, 0x0A);
        vga_putchar('\n');
    } else {
        vga_print_color("Failed to spawn process (out of memory)\n", 0x0C);
    }
}

/* ── thread: start a new kernel thread ──────────────────────────────────── */
static void thread_task(void) {
    pcb_t* me = get_current_process();
    char buf[12];
    uint32_t pid = me ? me->pid : 0;
    for (int i = 0; i < 3; i++) {
        vga_print_color("\n[kernel thread parent=", 0x0E);
        sh_itoa(pid, buf); vga_print_color(buf, 0x0E);
        vga_print_color(" tick ", 0x0E);
        sh_itoa((uint32_t)i, buf); vga_print_color(buf, 0x0E);
        vga_print_color("]", 0x0E);
        volatile uint32_t j;
        for (j = 0; j < 5000000; j++) __asm__ volatile("nop");
    }
    thread_exit();
}

static void cmd_thread(int argc, char** argv) {
    (void)argc; (void)argv;
    pcb_t* parent = get_current_process();
    tcb_t* t = create_thread(parent, thread_task);
    if (t) {
        char buf[12];
        vga_print_color("Started thread with TID ", 0x0A);
        sh_itoa(t->tid, buf); vga_print_color(buf, 0x0A);
        vga_putchar('\n');
    } else {
        vga_print_color("Failed to start thread (out of memory)\n", 0x0C);
    }
}

static void cmd_exit(int argc, char** argv) {
    (void)argc; (void)argv;
    vga_print_color("\nShutting down Vamos OS...\n", 0x0C);
    /* Small visible delay */
    volatile uint32_t i;
    for (i = 0; i < 20000000; i++) __asm__ volatile("nop");
    /* ACPI shutdown via port 0x604 (QEMU) */
    __asm__ volatile("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
    /* Fallback: keyboard controller reset */
    __asm__ volatile("cli");
    for (i = 0; i < 100000; i++) {
        uint8_t s;
        __asm__ volatile("inb $0x64, %0" : "=a"(s));
        if (!(s & 0x02)) break;
    }
    __asm__ volatile("outb %0, $0x64" : : "a"((uint8_t)0xFE));
    /* Final fallback: halt */
    for (;;) __asm__ volatile("cli; hlt");
}

/* ── Command table ──────────────────────────────────────────────────────── */

typedef struct {
    const char* name;
    void (*fn)(int argc, char** argv);
} shell_cmd_t;

static shell_cmd_t command_table[] = {
    { "help",    cmd_help    },
    { "clear",   cmd_clear   },
    { "echo",    cmd_echo    },
    { "ls",      cmd_ls      },
    { "time",    cmd_time    },
    { "ps",      cmd_ps      },
    { "threads", cmd_threads },
    { "meminfo", cmd_meminfo },
    { "spawn",   cmd_spawn   },
    { "thread",  cmd_thread  },
    { "reboot",  cmd_reboot  },
    { "exit",    cmd_exit    },
    { "about",   cmd_about   },
    { "banner",  cmd_banner  },
    { NULL,      NULL        }
};

/* ── Parser & dispatcher ────────────────────────────────────────────────── */

static void shell_dispatch(int argc, char** argv) {
    if (argc == 0) return;
    for (int i = 0; command_table[i].name != NULL; i++) {
        if (sh_strcmp(argv[0], command_table[i].name) == 0) {
            command_table[i].fn(argc, argv);
            return;
        }
    }
    vga_print_color("Unknown command: ", 0x0C);
    vga_print(argv[0]);
    vga_print("  (type 'help' for commands)\n");
}

static void shell_parse(char* line) {
    /* Skip leading spaces */
    while (*line == ' ') line++;
    if (!*line) return;

    char* argv[MAX_ARGS];
    int   argc = 0;
    char* p = line;
    while (*p && argc < MAX_ARGS) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) { *p = '\0'; p++; }
    }
    shell_dispatch(argc, argv);
}

/* ── Shell loop ─────────────────────────────────────────────────────────── */

/* Refresh status bar every N keystrokes */
static uint32_t keystroke_count = 0;

void shell_run(void) {
    char line[MAX_LINE];
    int  pos = 0;

    cmd_banner(0, (void*)0);
    vga_print("Type 'help' to see all commands.\n\n");
    /* Print prompt */
    vga_print_color("user", 0x0A);
    vga_print_color("@vamos-os", 0x0B);
    vga_print_color(" > ", 0x0E);

    for (;;) {
        /* Refresh status bar periodically */
        if (keystroke_count % 50 == 0)
            vga_draw_statusbar();

        char c;
        while ((c = keyboard_getchar()) == 0) {
            __asm__ volatile("sti; hlt; cli");
            __asm__ volatile("sti");
            /* Refresh status bar while idle */
            static uint32_t idle = 0;
            if (++idle % 200 == 0) vga_draw_statusbar();
        }
        keystroke_count++;

        if (c == '\n') {
            vga_putchar('\n');
            line[pos] = '\0';
            if (pos > 0) shell_parse(line);
            pos = 0;
            /* Print prompt */
            vga_print_color("user", 0x0A);
            vga_print_color("@vamos-os", 0x0B);
            vga_print_color(" > ", 0x0E);
        } else if (c == '\b') {
            if (pos > 0) { pos--; vga_putchar('\b'); }
        } else if (pos < MAX_LINE - 1) {
            line[pos++] = c;
            vga_putchar(c);
        }
    }
}
