#ifndef LOG_H
#define LOG_H

#define LOG_FILE_PATH "/tmp/link_log"

int format_fd(const char *format, ...);

#define D(fmt, args...) \
	format_fd("%s() - %d: "fmt"\n", __func__, __LINE__, ##args)

#endif
