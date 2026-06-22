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

#ifndef inc_log_hh
#define inc_log_hh

/* this header is a bit messed up due to c+cpp support */

#include <stddef.h>

#ifdef __cplusplus
	#define LOG_CLASS_KEYWORD class
	#define LOG_PREFIX
	#define LOGLEVEL(name) LogLevel::name
	#define LOG_EXTERN_C extern "C"
#else
	#define LOG_CLASS_KEYWORD
	#define LOG_PREFIX LLVL_
	#define LOGLEVEL(name) LLVL_##name
	#define LOG_EXTERN_C
	typedef int LogLevel;
#endif
#define LOGLEVEL_PREFIXED___(a,b) a##b
#define LOGLEVEL_PREFIXED__(a,b) LOGLEVEL_PREFIXED___(a,b)
#define LOGLEVEL_PREFIXED(name) LOGLEVEL_PREFIXED__(LOG_PREFIX, name)

enum LOG_CLASS_KEYWORD LogLevel
{
	LOGLEVEL_PREFIXED(fatal)    = 0,
	LOGLEVEL_PREFIXED(error)    = 1,
	LOGLEVEL_PREFIXED(warning)  = 2,
	LOGLEVEL_PREFIXED(info)     = 3,
	LOGLEVEL_PREFIXED(debug)    = 4,
	LOGLEVEL_PREFIXED(verbose)  = 5,
};

LOG_EXTERN_C __attribute__((format(printf, 5, 6)))
void _logf(const char *fnname, const char *filen,
	size_t line, LogLevel lvl, const char *fmt, ...);

#ifdef __cplusplus
void log_on_settings_changed();
char *log_retrieve();
void log_flush();
void log_init();
void log_exit();
void log_del();
#endif

#ifdef RELEASE
	#ifdef FULL_LOG
		#define l__log(l, ...) _logf(__func__, NULL, __LINE__, l, __VA_ARGS__)
	#else
		/* inlined check so compiler can optimize it out entirely */
		#define l__log(l, ...) if(l <= LOGLEVEL(info)) \
			_logf(__func__, NULL, __LINE__, l, __VA_ARGS__)
	#endif
#else
	#define l__log(l, ...) _logf(__func__, __FILE__, __LINE__, l, __VA_ARGS__)
	#define FULL_LOG 1
#endif

#define elog(...) l__log(LOGLEVEL(error), __VA_ARGS__)
#define flog(...) l__log(LOGLEVEL(fatal), __VA_ARGS__)
#define wlog(...) l__log(LOGLEVEL(warning), __VA_ARGS__)
#define ilog(...) l__log(LOGLEVEL(info), __VA_ARGS__)
#define dlog(...) l__log(LOGLEVEL(debug), __VA_ARGS__)
#define vlog(...) l__log(LOGLEVEL(verbose), __VA_ARGS__)

#endif

