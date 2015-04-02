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

    if (g_syslog_already_opened) {
        va_start(args, fmt);
        vsyslog(VLOG_DEBUG, fmt, args);
        va_end(args);
    }
#ifdef _DEBUG
    if (g_console_output_enabled) {
        va_start(args, fmt);
        printf("[D]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
#endif
    return 0;
}

int vlogDv(int cond, const char* fmt, ...)
{
    va_list(args);
    vassert(fmt);

    if (!cond) {
        return 0;
    }

    if (g_syslog_already_opened) {
        va_start(args, fmt);
        vsyslog(VLOG_DEBUG, fmt, args);
        va_end(args);
    }
#ifdef _DEBUG
    if (g_console_output_enabled) {
        va_start(args, fmt);
        printf("[D]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
#endif
    return 0;
}

int vlogI(const char* fmt, ...)
{
    va_list args;
    vassert(fmt);

    if (g_syslog_already_opened) {
        va_start(args, fmt);
        vsyslog(VLOG_INFO, fmt, args);
        va_end(args);
    }
    if (g_console_output_enabled) {
        va_start(args, fmt);
        printf("[I]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
    return 0;
}

int vlogIv(int cond, const char* fmt, ...)
{
    va_list(args);
    vassert(fmt);

    if (!cond) {
        return 0;
    }

    if (g_syslog_already_opened) {
        va_start(args,fmt);
        vsyslog(VLOG_INFO, fmt, args);
        va_end(args);
    }
    if (g_console_output_enabled) {
        va_start(args, fmt);
        printf("[I]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
    return 0;
}

int vlogE(const char* fmt, ...)
{
    va_list(args);
    vassert(fmt);

    if (g_syslog_already_opened) {
        va_start(args, fmt);
        vsyslog(VLOG_ERR, fmt, args);
        va_end(args);
    }
    if (1) {
        va_start(args, fmt);
        printf("[E]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
    return 0;
}

int vlogEv(int cond, const char* fmt, ...)
{
    va_list(args);
    vassert(fmt);

    if (!cond) {
        return 0;
    }
    if (g_syslog_already_opened) {
        va_start(args, fmt);
        vsyslog(VLOG_ERR, fmt, args);
        va_end(args);
    }
    if (1) {
        va_start(args, fmt);
        printf("[E]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
    return 0;
}

int vlog_open(const char* ident)
{
    vassert(ident);

    openlog(ident, LOG_CONS | LOG_PID, LOG_DAEMON);
    g_syslog_already_opened = 1;
    return 0;
}

void vlog_close (void)
{
    g_syslog_already_opened = 0;
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

