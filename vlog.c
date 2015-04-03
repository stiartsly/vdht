#include "vglobal.h"
#include "vlog.h"

static int g_need_syslog = 0;
static int g_need_stdout = 0;

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

    if (g_need_syslog) {
        va_start(args, fmt);
        vsyslog(VLOG_DEBUG, fmt, args);
        va_end(args);
    }
#ifdef _DEBUG
    if (g_need_stdout) {
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
    if (g_need_syslog) {
        va_start(args, fmt);
        vsyslog(VLOG_DEBUG, fmt, args);
        va_end(args);
    }
#ifdef _DEBUG
    if (g_need_stdout) {
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

    if (g_need_syslog) {
        va_start(args, fmt);
        vsyslog(VLOG_INFO, fmt, args);
        va_end(args);
    }
    if (g_need_stdout) {
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

    if (g_need_syslog) {
        va_start(args,fmt);
        vsyslog(VLOG_INFO, fmt, args);
        va_end(args);
    }
    if (g_need_stdout) {
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

    if (g_need_syslog) {
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
    if (g_need_syslog) {
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

int vlog_open(int syslog, const char* ident)
{
    vassert(ident);

    if (syslog) {
        g_need_syslog = 1;
        openlog(ident, LOG_CONS | LOG_PID, LOG_DAEMON);
    } else {
        g_need_syslog = 0;
        // log to customed log file.
        //todo;
    }
    return 0;
}

int vlog_open_with_cfg(struct vconfig* cfg)
{
    const char* ident = NULL;
    int syslog = 0;
    vassert(cfg);

    syslog = cfg->ext_ops->get_syslog_switch(cfg);
    ident  = cfg->ext_ops->get_syslog_ident(cfg);
    if (syslog) {
         if (g_need_syslog) {
            closelog();
         }
         g_need_syslog = 1;
         openlog(ident, LOG_CONS | LOG_PID, LOG_DAEMON);
    } else {
        //todo;
    }
    return 0;
}

void vlog_close (void)
{
    if(g_need_syslog) {
        closelog();
        g_need_syslog = 0;
    } else {
        //todo;
    }
    return ;
}

void vlog_stdout_enable (void)
{
    g_need_stdout = 1;
    return ;
}

void vlog_stdout_disable(void)
{
    g_need_stdout = 0;
    return ;
}

