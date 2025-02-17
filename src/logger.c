/*
 * Copyright (c) 2020-2021 Siddharth Chandrasekaran <sidcha.dev@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define _GNU_SOURCE		/* See feature_test_macros(7) */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <utils/logger.h>

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

logger_t default_logger = {
	.file = NULL,
	.flags = LOGGER_FLAG_NONE,
	.log_level = LOG_INFO,
	.name = "GLOBAL",
	.puts = puts,
	.root_path = NULL,
};

static const char *log_level_colors[LOG_MAX_LEVEL] = {
	RED,   RED,   RED,   RED,
	YEL,   MAG,   GRN,   RESET
};

static const char *log_level_names[LOG_MAX_LEVEL] = {
	"EMERG", "ALERT", "CRIT ", "ERROR",
	"WARN ", "NOTIC", "INFO ", "DEBUG"
};

static inline void logger_log_set_color(logger_t *ctx, const char *color)
{
	int ret, len;

	if (ctx->flags & LOGGER_FLAG_NO_COLORS)
		return;

	if (ctx->file && isatty(fileno(ctx->file))) {
		len = strnlen(color, 8);
		ret = write(fileno(ctx->file), color, len);
		assert(ret == len);
		ARG_UNUSED(ret); /* squash warning in Release builds */
	}
}

static const char *get_rel_path(logger_t *ctx, const char *abs_path)
{
	const char *p, *q;

	if (!ctx->root_path || abs_path[0] != '/')
		return abs_path;

	p = ctx->root_path;
	q = abs_path;
	while (*p != '\0' && *q != '\0' && *p == *p) {
		p++;
		q++;
	}
	while (*q != '\0' && *q == '/')
		q++;
	return q;
}

static const char *get_tstamp()
{
	static char time_buf[24];
	struct tm gmt;
	time_t now = time(NULL);

	gmtime_r(&now, &gmt);
	strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", &gmt);
	return time_buf;
}

#define LOG_BUF_LEN 128

__attribute__((format(printf, 5, 6)))
int __logger_log(logger_t *ctx, int log_level, const char *file, unsigned long line,
		 const char *fmt, ...)
{
	int len = 0;
	va_list args;
	char buf[LOG_BUF_LEN + 2]; /* +2 for tailing '\n' and '\0' */

	if (!ctx)
		ctx = &default_logger;

	if (log_level < LOG_EMERG ||
	    log_level >= LOG_MAX_LEVEL ||
	    log_level > ctx->log_level)
		return 0;

	/* print module and log_level prefix */
	len = snprintf(buf, LOG_BUF_LEN, "%s: [%s] [%s] %s:%lu: ", ctx->name, get_tstamp(),
		       log_level_names[log_level], get_rel_path(ctx, file), line);
	if (len > LOG_BUF_LEN)
		goto out;

	/* Print the actual message */
	va_start(args, fmt);
	len += vsnprintf(buf + len, LOG_BUF_LEN - len, fmt, args);
	va_end(args);
	if (len > LOG_BUF_LEN)
		goto out;

out:
	if (len > LOG_BUF_LEN)
		len = LOG_BUF_LEN;

	/* If the log line doesn't already end with a new line, add it */
	if(buf[len - 1] != '\n')
		buf[len++] = '\n';
	buf[len] = '\0';

	logger_log_set_color(ctx, log_level_colors[log_level]);

	if (ctx->file)
		fputs(buf, ctx->file);
	else
		ctx->puts(buf);

	logger_log_set_color(ctx, RESET);

	return len;
}

void logger_get_default(logger_t *ctx)
{
	memcpy(ctx, &default_logger, sizeof(default_logger));
}

void logger_set_default(logger_t *logger)
{
	default_logger = *logger;
}

void logger_set_log_level(logger_t *ctx, int log_level)
{
	ctx->log_level = log_level;
}

void logger_set_put_fn(logger_t *ctx, log_puts_fn_t fn)
{
	ctx->puts = fn;
}

void logger_set_file(logger_t *ctx, FILE *f)
{
	ctx->file = f;
}

void logger_set_name(logger_t *ctx, const char *name)
{
	if (!name)
		return;
	strncpy(ctx->name, name, LOGGER_NAME_MAXLEN);
	ctx->name[LOGGER_NAME_MAXLEN - 1] = '\0';
}

int logger_init(logger_t *ctx, int log_level, const char *name,
		const char *root_path, log_puts_fn_t puts_fn, FILE *file,
		int flags)
{
	if (!puts_fn && !file)
		return -1;

	ctx->log_level = log_level;
	ctx->root_path = root_path;
	ctx->puts = puts_fn;
	ctx->file = file;
	ctx->flags = flags;
	logger_set_name(ctx, name);
	return 0;
}
