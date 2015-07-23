#include "vglobal.h"
#include "vlog.h"

static int g_need_syslog  = 0;
static int g_stdout_level = VLOG_ERR;

/*
 * the routine to log debug message.
 */
int vlogD(const char* fmt, ...)
{
    va_list args;
    vassert(fmt);

    if (g_stdout_level < VLOG_DEBUG) {
        return 0;
    }

    if (1) {
        va_start(args, fmt);
        printf("[D]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
    if (g_need_syslog) {
        va_start(args, fmt);
        vsyslog(VLOG_DEBUG, fmt, args);
        va_end(args);
    }

    return 0;
}

/*
 * the routine to log debug message on given condition.
 * @cond: condition.
 */
int vlogDv(int cond, const char* fmt, ...)
{
    va_list(args);
    vassert(fmt);

    if ((!cond) || (g_stdout_level < VLOG_DEBUG)){
        return 0;
    }

    if (1) {
        va_start(args, fmt);
        printf("[D]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
    if (g_need_syslog) {
        va_start(args, fmt);
        vsyslog(VLOG_DEBUG, fmt, args);
        va_end(args);
    }

    return 0;
}

int vlogDp(const char* fmt, ...)
{
    va_list(args);
    vassert(fmt);

    if (g_stdout_level < VLOG_DEBUG) {
        return 0;
    }

    if (1) {
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
    if (g_need_syslog) {
        va_start(args, fmt);
        vsyslog(VLOG_DEBUG, fmt, args);
        va_end(args);
    }

    return 0;
}

/*
 * the routine to log inform message;
 */
int vlogI(const char* fmt, ...)
{
    va_list args;
    vassert(fmt);

    if (g_stdout_level < VLOG_INFO) {
        return 0;
    }

    if (1) {
        va_start(args, fmt);
        printf("[I]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
    if (g_need_syslog) {
        va_start(args, fmt);
        vsyslog(VLOG_INFO, fmt, args);
        va_end(args);
    }

    return 0;
}

/*
 * the routine to log inform message on given condition.
 * @cond: condition.
 */
int vlogIv(int cond, const char* fmt, ...)
{
    va_list args;
    vassert(fmt);

    if (!cond || (g_stdout_level < VLOG_INFO)) {
        return 0;
    }

    if (1) {
        va_start(args, fmt);
        printf("[I]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
    if (g_need_syslog ) {
        va_start(args,fmt);
        vsyslog(VLOG_INFO, fmt, args);
        va_end(args);
    }

    return 0;
}

int vlogIp(const char* fmt, ...)
{
    va_list args;
    vassert(fmt);

    if (g_stdout_level < VLOG_INFO) {
        return 0;
    }

    if (1) {
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
    if (g_need_syslog ) {
        va_start(args,fmt);
        vsyslog(VLOG_INFO, fmt, args);
        va_end(args);
    }

    return 0;
}

/*
 * the routine to log error message
 */
int vlogE(const char* fmt, ...)
{
    va_list(args);
    vassert(fmt);

    if (1) {
        va_start(args, fmt);
        printf("[E]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
    if (g_need_syslog) {
        va_start(args, fmt);
        vsyslog(VLOG_ERR, fmt, args);
        va_end(args);
    }

    return 0;
}

/*
 * the routine to log error message on given condition.
 * @cond: condition.
 */
int vlogEv(int cond, const char* fmt, ...)
{
    va_list(args);
    vassert(fmt);

    if (!cond) {
        return 0;
    }

    if (1) {
        va_start(args, fmt);
        printf("[E]");
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
    if (g_need_syslog) {
        va_start(args, fmt);
        vsyslog(VLOG_ERR, fmt, args);
        va_end(args);
    }

    return 0;
}

/*
 * the routine to open a connection to logger.
 */
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

/*
 * the routine to open a connection to logger.
 */
int vlog_open_with_cfg(struct vconfig* cfg)
{
    const char* ident = NULL;
    int syslog = 0;
    vassert(cfg);

    syslog = cfg->ext_ops->get_syslog_switch(cfg);
    if (syslog > 0) {
        ident  = cfg->ext_ops->get_syslog_ident(cfg);
    }
    if (syslog) {
         if (g_need_syslog) {
            closelog();
         }
         g_need_syslog = 1;
         openlog(ident, LOG_CONS | LOG_PID, LOG_DAEMON);
    } else {
         g_need_syslog = 0;
    }
    return 0;
}

/*
 * the routine to close the connection to logger.
 */
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

/*
 * the routine to enable log stdout.
 */
void vlog_stdout_enable (int level)
{
    if ((level < VLOG_ERR) || (level >= VLOG_BUTT)) {
        g_stdout_level = VLOG_ERR;
    } else {
        g_stdout_level = level;
    }
    return ;
}

/*
 * the routine to disable log stdout.
 */
void vlog_stdout_disable(void)
{
    g_stdout_level = VLOG_ERR;
    return ;
}

