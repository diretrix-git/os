#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "pmm.h"
#include "timer.h"
#include "process.h"
#include "scheduler.h"

#define SHELL_BUFFER_SIZE 256

static char input_buffer[SHELL_BUFFER_SIZE];
static int input_index = 0;

static int shell_strcmp(const char *s1, const char *s2);

static void shell_print_welcome(void)
{
    vga_print("\n========================================\n");
    vga_print("       Welcome to MyOS v1.0\n");
    vga_print("    A Hobby Operating System\n");
    vga_print("========================================\n\n");
    vga_print("Type 'help' for available commands.\n\n");
}

static void shell_print_prompt(void)
{
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("myos> ");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void shell_read_line(void)
{
    input_index = 0;

    while (1)
    {
        char c = keyboard_getchar();

        if (c == '\n' || c == '\r')
        {
            vga_putchar('\n');
            input_buffer[input_index] = 0;
            return;
        }
        else if (c == '\b')
        {
            if (input_index > 0)
            {
                input_index--;
                vga_putchar('\b');
                vga_putchar(' ');
                vga_putchar('\b');
            }
        }
        else if (c >= 32 && c < 127)
        {
            if (input_index < SHELL_BUFFER_SIZE - 1)
            {
                input_buffer[input_index++] = c;
                vga_putchar(c);
            }
        }
    }
}

static void cmd_help(void)
{
    vga_print("Available commands:\n");
    vga_print("  help     - Show this help message\n");
    vga_print("  clear    - Clear the screen\n");
    vga_print("  echo     - Print text\n");
    vga_print("  meminfo  - Show memory information\n");
    vga_print("  ps       - Show process list\n");
    vga_print("  timer    - Show timer ticks\n");
    vga_print("  reboot   - Reboot (not implemented)\n");
}

static void cmd_echo(char *args)
{
    vga_print(args);
    vga_putchar('\n');
}

static void cmd_meminfo(void)
{
    uint32_t used = pmm_used_frames();
    uint32_t free = pmm_free_frames();

    vga_print("Memory Information:\n");
    vga_print("  Used frames: ");
    vga_print_int(used);
    vga_print("\n");
    vga_print("  Free frames: ");
    vga_print_int(free);
    vga_print("\n");
    vga_print("  Frame size: 4KB\n");
    vga_print("  Used memory: ");
    vga_print_int(used * 4);
    vga_print(" KB\n");
    vga_print("  Free memory: ");
    vga_print_int(free * 4);
    vga_print(" KB\n");
}

static void cmd_ps(void)
{
    vga_print("Process List:\n");
    vga_print("  PID  Name              State\n");
    vga_print("  ---  ----              -----\n");

    struct process *current = scheduler_get_current();
    if (current)
    {
        vga_print("  ");
        vga_print_int(current->pid);
        vga_print("   ");
        vga_print(current->name);
        vga_print("   RUNNING\n");
    }
    else
    {
        vga_print("  No active processes\n");
    }
}

static void cmd_timer(void)
{
    vga_print("Timer ticks: ");
    vga_print_int(timer_get_ticks());
    vga_print("\n");
}

static void shell_execute(char *input)
{
    /* Skip leading whitespace */
    while (*input == ' ')
        input++;

    if (input[0] == 0)
    {
        return;
    }

    /* Parse command */
    char *cmd = input;
    char *args = 0;

    while (*input && *input != ' ')
    {
        input++;
    }

    if (*input == ' ')
    {
        *input = 0;
        args = input + 1;
    }

    /* Execute command */
    if (shell_strcmp(cmd, "help") == 0)
    {
        cmd_help();
    }
    else if (shell_strcmp(cmd, "clear") == 0)
    {
        vga_clear();
    }
    else if (shell_strcmp(cmd, "echo") == 0)
    {
        cmd_echo(args ? args : "");
    }
    else if (shell_strcmp(cmd, "meminfo") == 0)
    {
        cmd_meminfo();
    }
    else if (shell_strcmp(cmd, "ps") == 0)
    {
        cmd_ps();
    }
    else if (shell_strcmp(cmd, "timer") == 0)
    {
        cmd_timer();
    }
    else
    {
        vga_print("Unknown command: ");
        vga_print(cmd);
        vga_print("\nType 'help' for available commands.\n");
    }
}

static int shell_strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s2 && *s1 == *s2)
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void shell_init(void)
{
    vga_print("\n[SHELL] Initializing\n");
    shell_print_welcome();
}

void shell_run(void)
{
    vga_print("[SHELL] Running main loop\n");
    while (1)
    {
        vga_print("[SHELL] About to print prompt\n");
        shell_print_prompt();
        vga_print("[SHELL] About to read line\n");
        shell_read_line();
        vga_print("[SHELL] About to execute\n");
        shell_execute(input_buffer);
    }
}
