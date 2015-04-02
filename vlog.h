#ifndef __VLOG_H__
#define __VLOG_H__

int vlogD(const char*, ...);
int vlogI(const char*, ...);
int vlogE(const char*, ...);

int vlogDv(int, const char*, ...);
int vlogIv(int, const char*, ...);
int vlogEv(int, const char*, ...);

int  vlog_open (int, const char*);
void vlog_close(void);
void vlog_enable_console_output(void);
void vlog_disable_console_output(void);

#endif

