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

#ifndef inc_panic_hh
#define inc_panic_hh

#include <string>

#include <string.h>
#include <3ds.h>

#include "error.hh"

#ifndef RELEASE
	#define SOURCE_LOCATION const char *func = __builtin_FUNCTION(), const char *file = __builtin_FILE(), size_t line = __builtin_LINE()
	#define WITH_DEBUG(...) __VA_ARGS__
#else
	#define SOURCE_LOCATION const char *func = __builtin_FUNCTION(), const char *file = NULL, size_t line = __builtin_LINE()
	#define WITH_DEBUG(...)
#endif
#define PASS_SOURCE_LOCATION func, file, line

class SourceLocation {
public:
	static SourceLocation Caller(SOURCE_LOCATION)
	{
		return SourceLocation { PASS_SOURCE_LOCATION };
	}

	static SourceLocation Null()
	{
		return SourceLocation { nullptr, nullptr, 0 };
	}

	const char *function_name() const { return this->function; }
	void log(const char *msg = "called") const;
	std::string to_string() const;

private:
	explicit SourceLocation(SOURCE_LOCATION)
		: function(func), filename(file), lineno(line) { }

	const char *function, *filename;
	size_t lineno;
};

#define panic_if_err_3ds(result) do { Result res = (result); if(R_FAILED(res)) panic(res); } while(0)
#define panic_assert(cond, msg) if(!(cond)) panic("Assertion failed\n" #cond "\n" msg)
#define panic_if(cond, msg) if((cond)) panic("Assertion failed\n" #cond "\n" msg)

void handle_error(const error_container& err, const std::string *label = nullptr);

[[noreturn]] void panic(const std::string& msg, const SourceLocation& = SourceLocation::Caller());
[[noreturn]] void panic(Result res, const SourceLocation& = SourceLocation::Caller());
[[noreturn]] void panic(const SourceLocation& = SourceLocation::Caller());

void panic_enable_gfx();

Result init_services(bool& isLuma);
void exit_services();

#endif

