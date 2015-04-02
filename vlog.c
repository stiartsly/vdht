#include "vglobal.h"
#include "vlog.h"

static int g_need_syslog = 0;
static int g_need_print  = 0;

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
    if (g_need_print) {
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
    if (g_need_print) {
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
    if (g_need_print) {
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
    if (g_need_print) {
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
    int syslog = 0;
    char ident[32];
    int ret = 0;

    vassert(cfg);

    syslog = cfg->ext_ops->get_syslog_switch(cfg);
    if (syslog) {
         memset(ident, 0, 32);
         ret = cfg->ext_ops->get_syslog_ident(cfg, ident, 32);
         retE((ret < 0));
    }
    vlog_close();
    //vlog_open(syslog, (const char*)ident);
    vlog_open(syslog, "vdhtd");
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

void vlog_enable_console_output(void)
{
    g_need_print = 1;
    return;
}

void vlog_disable_console_output(void)
{
    g_need_print = 0;
    return ;
}

