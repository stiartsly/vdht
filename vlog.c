#include "vglobal.h"
#include "vlog.h"

static int g_syslog_already_opened  = 0;
static int g_console_output_enabled = 0;

enum {
    VLOG_ERR    = LOG_ERR,
    VLOG_INFO   = LOG_INFO,
    VLOG_DEBUG  = LOG_DEBUG,
    VLOG_BUTT
};

int vlogD(const char* fmt, ...)
{
    va_list args;
    vassert(fmt);

    va_start(args, fmt);
    if (g_syslog_already_opened) {
        vsyslog(VLOG_DEBUG, fmt, args);
    }
#ifdef _DEBUG
    if (g_console_output_enabled) {
        printf("[D]");
        vprintf(fmt, args);
        printf("\n");
    }
#endif
    va_end(args);

    return 0;
}

int vlogDv(int cond, const char* fmt, ...)
{
    va_list(args);
    vassert(fmt);

    if (!cond) {
        return 0;
    }

    va_start(args, fmt);
    if (g_syslog_already_opened) {
        vsyslog(VLOG_DEBUG, fmt, args);
    }
#ifdef _DEBUG
    if (g_console_output_enabled) {
        printf("[D]");
        vprintf(fmt, args);
        printf("\n");
    }
#endif
    va_end(args);
    return 0;
}

int vlogI(const char* fmt, ...)
{
    va_list args;
    vassert(fmt);

    va_start(args, fmt);
    if (g_syslog_already_opened) {
        vsyslog(VLOG_INFO, fmt, args);
    }
    if (g_console_output_enabled) {
        printf("[I]");
        vprintf(fmt, args);
        printf("\n");
    }
    va_end(args);
    return 0;
}

int vlogIv(int cond, const char* fmt, ...)
{
    va_list(args);
    vassert(fmt);

    if (!cond) {
        return 0;
    }

    va_start(args, fmt);
    if (g_syslog_already_opened) {
        vsyslog(VLOG_INFO, fmt, args);
    }
    if (g_console_output_enabled) {
        printf("[I]");
        vprintf(fmt, args);
        printf("\n");
    }
    va_end(args);
    return 0;
}

int vlogE(const char* fmt, ...)
{
    va_list(args);
    vassert(fmt);

    va_start(args, fmt);
    if (g_syslog_already_opened) {
        vsyslog(VLOG_ERR, fmt, args);
    }
    if (1) {
        printf("[E]");
        vprintf(fmt, args);
        printf("\n");
    }
    va_end(args);
    return 0;
}

int vlogEv(int cond, const char* fmt, ...)
{
    va_list(args);
    vassert(fmt);

    if (!cond) {
        return 0;
    }
    va_start(args, fmt);
    if (g_syslog_already_opened) {
        vsyslog(VLOG_ERR, fmt, args);
    }
    if (1) {
        printf("[E]");
        vprintf(fmt, args);
        printf("\n");
    }
    va_end(args);
    return 0;
}

int vlog_open(const char* ident)
{
    vassert(ident);

    openlog(ident, LOG_CONS | LOG_PID, LOG_DAEMON);
    return 0;
}

void vlog_close (void)
{
    closelog();
    return ;
}

void vlog_enable_console_output(void)
{
    g_console_output_enabled = 1;
    return;
}

void vlog_disable_console_output(void)
{
    g_console_output_enabled = 0;
    return ;
}

