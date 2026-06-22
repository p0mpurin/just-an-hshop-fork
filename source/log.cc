/* This file is part of 3hs
 * Copyright (C) 2021-2025 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "panic.hh"
#include "log.hh"

#if defined(RELEASE) && !defined(FULL_LOG)
	#include "settings.hh"
	#define DEBUG_FEATURES 0
#else
	#define DEBUG_FEATURES 1
#endif

#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <3ds.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define F "/3ds/3hs/3hs.log"
#define GEN_MAX_LEN 200


static FILE *log_file = NULL;
static LightLock file_lock = -1;
enum {
	ST_INIT = 1,
	ST_OOM  = 2,
	ST_FSET = 4,
}; static int log_state = 0;
static char *log_mem = NULL;
static size_t log_mem_len = 0;
static size_t log_mem_used = 0;
static u8 last_elogs = 0;

#define LOG_MEMORY_INITIAL_LEN (1024 - oom_log_msg_len - 1)
#define LOG_ALLOC_SIZE(s) ((s) + oom_log_msg_len + 1)

static char oom_log_msg[] = "-- WARNING: log cut off; out of memory --\n";
#define oom_log_msg_len sizeof(oom_log_msg)


static u8 get_max_elogs()
{
#if DEBUG_FEATURES
	return 0;
#else
	return get_nsettings()->max_elogs;
#endif
}

static void reserve_main_log()
{
	if(access(F, F_OK) != 0)
		return; /* nothing to do ... */

	static char path1[] = F ".XXX";
	static char path2[] = F ".XXX";
	u8 max = get_max_elogs();
	u8 val;

	if(max == 0) /* always override; nothing to do */
		return;
	--max;

#define SET(p,i) sprintf(p + sizeof("/3ds/3hs/3hs.log.") - 1, "%u", (i)+1);
	for(val = 0; val != max; ++val)
	{
		SET(path1, val);
		if(access(path1, F_OK) != 0)
			break;
	}
	SET(path1, val);
	/* upper log number is now in val; shift all up */
	if(val == max) remove(path1); /* we need to discard one */
	for(; val != 0; --val)
	{
		SET(path1, val - 1);
		SET(path2, val);
		rename(path1, path2);
	}
	rename(F, F ".1");
#undef SET
}

static FILE *open_f()
{
	if(!(log_state & ST_FSET))
	{
		reserve_main_log();
		mkdir("/3ds", 0777);
		mkdir("/3ds/3hs", 0777);
		log_state |= ST_FSET;
	}
	log_file = fopen(F, "w+");
	return log_file;
}

static void open_mem()
{
	log_mem = (char *) malloc(LOG_ALLOC_SIZE(LOG_MEMORY_INITIAL_LEN));
	/* this should always succeed; assert success */
	panic_assert(log_mem, "failed to initialize memory log");
	log_mem_len = LOG_MEMORY_INITIAL_LEN;
	log_state &= ~ST_OOM;
}

static void write_string(const char *s)
{
	LightLock_Lock(&file_lock);
#if DEBUG_FEATURES
	fputs(s, stderr);
#endif

	if(get_max_elogs())
	{
		if(!log_file)
			goto out;
		fputs(s, log_file);
#if DEBUG_FEATURES
		fflush(log_file);
#endif
	}
	else if(!(log_state & ST_OOM))
	{
		if(!log_mem) goto out;
		size_t slen = strlen(s);
		size_t nlen = log_mem_used + slen;
		if(nlen >= log_mem_len)
		{
			char *nmem = (char *) realloc(log_mem, LOG_ALLOC_SIZE(nlen * 2));
			if(!nmem)
			{
				/* we always have enough memory to write this, as allocated by LOG_ALLOC_SIZE */
				memcpy(log_mem + log_mem_used, oom_log_msg, oom_log_msg_len);
				log_state |= ST_OOM;
				goto out;
			}
			log_mem_len = nlen * 2;
			log_mem = nmem;
		}
		memcpy(log_mem + log_mem_used, s, slen + 1);
		log_mem_used += slen;
	}

out:
	LightLock_Unlock(&file_lock);
}

#define LOGS_ITER(path, ent, ...) \
	do { \
		DIR *d = opendir("/3ds/3hs"); \
		struct dirent *ent; \
		while((ent = readdir(d))) \
		{ \
			if(strncmp(ent->d_name, "3hs.log", sizeof("3hs.log") - 1) == 0) \
			{ \
				static char path[NAME_MAX+sizeof("/3ds/3hs/")] = "/3ds/3hs/"; \
				strcpy(path + sizeof("/3ds/3hs/") - 1, ent->d_name); \
				__VA_ARGS__ \
			} \
		}  \
		closedir(d); \
	} while(0)

static void clear_all_logs()
{
	LOGS_ITER(path, ent,
		remove(path);
	);
}

static void log_delete_invalid()
{
	u8 max = get_max_elogs();
	LOGS_ITER(path, ent,
		char *start = ent->d_name + sizeof("3hs.log.") - 1;
		char *end;
		errno = 0;
		unsigned long id = strtoul(start, &end, 10);
		if(end == start || errno == ERANGE) continue;
		if(id > max) remove(path);
	);
}

void log_init()
{
	/* already initialized */
	if(log_state & ST_INIT)
		return;
	last_elogs = get_max_elogs();

#if DEBUG_FEATURES
	#warning compiling with debug features on
	consoleDebugInit(debugDevice_SVC);
#endif

	LightLock_Init(&file_lock);
	if(last_elogs)
		open_f();
	else open_mem();

	log_state |= ST_INIT;
}

char *log_retrieve_file(int extra_alloc)
{
	u32 size = ftell(log_file);
	if(size == 0) return strdup("-- empty log file --\n");
	fseek(log_file, 0, SEEK_SET);
	char *data = (char *) malloc(size + extra_alloc + 1);
	int rsize = fread(data, 1, size, log_file);
	fseek(log_file, size, SEEK_SET);
	if(rsize <= 0)
	{
		free(data);
		return NULL;
	}
	data[rsize] = '\0';
	return data;
}

char *log_retrieve()
{
	if((!get_max_elogs() && !log_mem) || (get_max_elogs() && !log_file))
		return strdup("-- failed to open log this session --\n");
	if(get_max_elogs())
		return log_retrieve_file(0);
	else if(log_mem_used == 0)
		return strdup("-- empty log file --\n");
	else return strdup(log_mem);
}

void log_on_settings_changed()
{
	u8 elogs = get_max_elogs();
	/* small optimization since this function is called
	 * every time the settings menu is exited */
	if(elogs == last_elogs)
		return;

	/* we need to enable memory logging; fetch the old log from
	 * the file and put it in our buffer */
	if(!elogs && last_elogs)
	{
		log_mem = log_retrieve_file(oom_log_msg_len);
		if(log_mem) log_mem_used = log_mem_len = strlen(log_mem);
		fclose(log_file);
		log_file = NULL;
	}
	/* if we have a log file and max_elogs is not zero that means that
	 * file log was re-enabled; we need to rewrite our file */
	else if(elogs && !last_elogs)
	{
		open_f();
		if(log_file)
		{
			/* we need to write the entire memory log here */
			fwrite(log_mem, 1, log_mem_used, log_file);
		}
		free(log_mem);
		log_mem = NULL;
	}
	log_delete_invalid();
	last_elogs = elogs;
}

void log_del()
{
	if(log_file) fclose(log_file);
	if(log_mem) free(log_mem);
	clear_all_logs();
	if(get_max_elogs())
		open_f();
	else open_mem();
}

void log_exit()
{
	if(log_file) fclose(log_file);
	if(log_mem) free(log_mem);
	if(file_lock != -1)
		LightLock_Unlock(&file_lock);
}

void log_flush()
{
	if(get_max_elogs() && log_file)
		fflush(log_file);
	/* ...nothing to do for memory logs */
}

extern "C" void _logf(const char *fnname, const char *filen,
	size_t line, LogLevel lvl, const char *fmt, ...)
{
	/* early log, we can't log yet */
	if(file_lock == -1)
		return;
//	if(lvl > MAX_LVL)
//		return; /* check is done in header */

	const char *lvl_s;
	switch(lvl)
	{
	case LogLevel::fatal:
		lvl_s = "FATAL";
		break;
	case LogLevel::error:
		lvl_s = "ERROR";
		break;
	case LogLevel::warning:
		lvl_s = "WARNING";
		break;
	case LogLevel::info:
		lvl_s = "INFO";
		break;
	case LogLevel::debug:
		lvl_s = "DEBUG";
		break;
	case LogLevel::verbose:
		lvl_s = "VERBOSE";
		break;
	default:
		panic("EINVAL");
		return;
	}

	time_t now = time(NULL);
	struct tm *tm = gmtime(&now);

#define TIME_ARGS tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec
#ifdef RELEASE
	#define FMT "%i-%i-%i %02i:%02i:%02i %s [%s@%i] "
	#define ARGS TIME_ARGS, lvl_s, fnname, line
	(void) filen; /* always NULL on release anyways */
#else
	#define FMT "%i-%i-%i %02i:%02i:%02i %s [%s:%s@%i] "
	#define ARGS TIME_ARGS, lvl_s, filen, fnname, line
#endif

	va_list va;
	va_start(va, fmt);
	int ulen = vsnprintf(NULL, 0, fmt, va);
	va_end(va);
	char *s = (char *) malloc(ulen + GEN_MAX_LEN + 2);
	if(!s) return; /* shouldn't happen */
	int start = snprintf(s, GEN_MAX_LEN, FMT, ARGS);
	start = MIN(start, GEN_MAX_LEN);
	va_start(va, fmt);
	vsprintf(s + start, fmt, va);
	va_end(va);
	s[start + ulen + 0] = '\n';
	s[start + ulen + 1] = '\0';

	write_string(s);
	free(s);

#undef TIME_ARGS
#undef ARGS
#undef FMT
}
