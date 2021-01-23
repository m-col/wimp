#include <stdio.h>
#include <time.h>
#include <wlr/util/log.h>


static enum wlr_log_importance log_importance;


static const char *verbosity_headers[] = {
    [WLR_SILENT] = "",
    [WLR_ERROR] = "[ERROR]",
    [WLR_INFO] = "[INFO]",
    [WLR_DEBUG] = "[DEBUG]",
};


static void log_stdout(enum wlr_log_importance verbosity, const char *fmt, va_list args) {
    if (verbosity > log_importance) {
	return;
    }

    time_t t = time(NULL);
    struct tm *now = localtime(&t);
    char nows[20];
    strftime(nows, 20, "%F %T", now);

    fprintf(stdout, "%s %s: ", nows, verbosity_headers[verbosity]);
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
}


void init_log(enum wlr_log_importance log_level) {
    log_importance = log_level;
    wlr_log_init(log_importance, log_stdout);
}
