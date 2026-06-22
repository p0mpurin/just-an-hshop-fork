#!/usr/bin/env perl

use warnings;
use strict;
use utf8;

# ====================== #

my $min_missing_to_stall = 10;
my $build_dir = "build";
my $lang_dir = "lang";
my $print_missing = 0;
my $without_color = 0;
my $dry_run = 0;

# NOTE: The first language in this array
#       will be used as the base language.
my @languages = qw(
	english
	dutch
	german
	spanish
	french
	fr_canada
	romanian
	italian
	portuguese
	korean
	greek
	polish
	hungarian
	japanese
	russian
	spearglish
	ryukyuan
	latvian
	jp_osaka
	moldovan
	schinese
	tchinese
	neapolitan
	macedonian
	welsh
	tagalog
	catalan
	ukrainian
);

# Strings never to put between quotes
my @preserve_keywords = qw(
	PAGE_3HS
	PAGE_THEMES
	PAGE_DONATE
	PAGE_SWITCHING_SD_CARDS
	UI_GLYPH_A
	UI_GLYPH_B
	UI_GLYPH_X
	UI_GLYPH_Y
	UI_GLYPH_L
	UI_GLYPH_R
	UI_GLYPH_CPAD
	UI_GLYPH_DPAD_CLEAR
	UI_GLYPH_DPAD_UP
	UI_GLYPH_DPAD_DOWN
	UI_GLYPH_DPAD_LEFT
	UI_GLYPH_DPAD_RIGHT
	UI_GLYPH_DPAD_HORIZONTAL
	UI_GLYPH_DPAD_VERTICAL
);

my @extra_data_directories = qw(
	error-messages
);

# ====================== #

use Getopt::Long qw(:config no_auto_abbrev no_ignore_case bundling);

GetOptions(
	"lang|l=s", \$lang_dir,
	"build|b=s", \$build_dir,
	"print-missing|m", \$print_missing,
	"no-color|c", \$without_color,
	"dry-run|dry|d", \$dry_run,
) or die;

-d $lang_dir or die "no language directory available.";
-d $build_dir or die "no build directory available." unless $dry_run;

my $source_file = "";
my $header_file = "";
my $main_table = "";

my $langs_w_missing = 0;
my $total_missing = 0;

my $func_ctr = 0;

my @stalled_langs = qw();
my @string_ids = qw();

my $red_fmt = $without_color ? "" : "\033[31;1m";
my $bld_fmt = $without_color ? "" : "\033[1m";
my $clr_fmt = $without_color ? "" : "\033[0m";

my $extra_dir_count = scalar @extra_data_directories;

sub on_string_read {
	my ($strings, $functions_codegen, $functions, $current_id, $string_content) = @_;

	$current_id =~ /^([^ ]+)/i;
	my $real_id = $1;
	return unless $real_id;

	if($current_id  =~ /\+code\((.*)\)/) {
		my $code_bit = $1;
		$functions->{$real_id} = $string_content;
		my @params = split ", ", $code_bit;
		my $genned_code = "";

		for my $i (0..$#params) {
			next if $params[$i] eq '_';
			my $type = $params[$i];
			my $i_2 = $i + 1;
			if($type eq 'string') {
				$genned_code .= "const std::string& p$i_2 = _params_vec[$i];\n";
			} else {
				if($type eq 'number') { $type = 'unsigned long long'; }
				$genned_code .= "auto p$i_2 = unstring<$type>(_params_vec[$i]);\n";
			}
		}
		chomp $genned_code;
		$functions_codegen->{$real_id} = $genned_code;
	}
	else {
		$strings->{$real_id} = $string_content;
	}
}

sub parse_lang_file {
	my ($lang_name) = @_;
	open my $fh, "$lang_dir/$lang_name" or die "failed to open $lang_name file.";
	my $is_reference_lang = @string_ids == 0;
	my %functions_codegen;
	my $do_linebreak = 1;
	my %functions;
	my %strings;
	my $extline;
	my $line;

	my $native_name = "";
	my $string_content = "";
	my $current_id = "";

	print "$lang_name ... ";

	my $dir_num = -1;
	my $next_line_append = 0;

	while(1) {
		while(($line = <$fh>)) {
			chomp $line;
			# Remove CR.......................
			$line =~ s/\r//g;
			if ($line =~ /^\s*#[^#]/) {
				next;
			} elsif($line =~ /^- /) {
				if($current_id eq "native_name") {
					$native_name = $string_content;
				} elsif($current_id) {
					on_string_read \%strings, \%functions_codegen, \%functions, $current_id, $string_content;
				}
				$string_content = "";
				$current_id = $line;
				$current_id =~ s/^- //;
				$do_linebreak = !($current_id =~ /\+nobreak/);
			}
			elsif($line =~ /[^\s]/) {
				$line =~ s/#{2}/#/g;
				if($next_line_append || !$do_linebreak) {
					$string_content and $string_content .= " ";
					$next_line_append = 0;
				}
				else {
					$string_content and $string_content .= "\n";
				}
				if($line =~ /\\$/) {
					$line =~ s/\s*\\$//g;
					$next_line_append = 1;
				}
				$string_content .= $line;
			}
		}

		on_string_read \%strings, \%functions_codegen, \%functions, $current_id, $string_content;

		close $fh;
		for(++$dir_num; $dir_num != $extra_dir_count; ++$dir_num) {
			open $fh, "$lang_dir/$extra_data_directories[$dir_num]/$lang_name" and last;
		}
		last if $dir_num >= $extra_dir_count;
	}

	if($is_reference_lang) {
		print "reference\n";
		foreach my $k (sort keys %strings) {
			push @string_ids, $k;
		}
		foreach my $k (sort keys %functions) {
			push @string_ids, $k;
		}
	} else {
		my $inval_count = 0;
		foreach my $k (keys %strings) {
			if(not grep /^$k$/, @string_ids) {
				if(not $inval_count) {
					print "invalid id(s): ";
				} else {
					print ", ";
				}
				print $k;
				++$inval_count;
			}
		}
		if($inval_count) {
			print "\n";
			return 0;
		}
	}

	if(not $native_name) {
		print "no native name\n";
		return 0;
	}

	my $missing = 0;

	$main_table .= "\t[lang::$lang_name] =\n\t\t{\n";
	foreach my $k (@string_ids) {
		my $string = $strings{$k};
		my $function = $functions{$k};
		if($string) {
			$main_table .= "\t\t\t[str::$k] = { .string = ";

			open my $lr, '<', \$string;
			my $line;
			while(($line = <$lr>)) {
				$line =~ s/\n$/\\n/;
				$line =~ s/"/\\"/g;
				$line = " \"$line\"";
				foreach my $kw (@preserve_keywords) {
					$line =~ s/$kw/" $kw "/g;
				}
				$main_table .= $line;
			}
			$main_table .= ", .info = INFO_NONE },\n";
		}
		elsif($function) {
			my $func_name = "_function_for_table_$func_ctr";
			++$func_ctr;
			$source_file .= <<EOF;
/* string "$k" for language "$lang_name" ("$native_name") */
static const char *$func_name(const std::vector<std::string>& _params_vec)
{
// generated code
$functions_codegen{$k}
// end generated code
$function
}

EOF
			$main_table .= "\t\t\t[str::$k] = { .function = $func_name, .info = INFO_ISFUNC },\n"
		}
		else {
			$main_table .= "\t\t\tSTUB($k),\n";
			print ($missing ? ", ".$bld_fmt.$k.$clr_fmt : $bld_fmt.$k.$clr_fmt) if $print_missing;
			++$missing;
		}
	}
	$main_table .= "\t\t},\n\n";

	if(not $is_reference_lang) {
		if($missing) {
			if($print_missing) {
				my $verb = $missing == 1 ? "is" : "are";
				print " $verb missing";
			}
			else {
				print $red_fmt."$missing".$clr_fmt." missing";
			}
			# If there are more than 10 missing strings we can
			# consider the language stalled, it's kept
			# in there for if anyone picks it up again
			# but it won't be a default anymore
			if ($missing > $min_missing_to_stall) {
				push @stalled_langs, $lang_name;
				print " (stalled)";
			}
			print "\n";

			$total_missing += $missing;
			++$langs_w_missing;
		} else {
			print "complete\n";
		}
	}

	return $native_name;
}

$source_file .= <<EOF;
/* this file was generated by ./lang/make.pl
 * DO NOT EDIT */

#include <ui/base.hh> /* for UI_GLYPH_* */
#include <3ds/types.h>
#include <vector>
#include "./i18n_tab.hh"

#ifndef HS_SITE_LOC
	#define PAGE_3HS "(unset)"
	#define PAGE_THEMES "(unset)"
	#define PAGE_DONATE "(unset)"
	#define PAGE_SWITCHING_SD_CARDS "(unset)"
#else
	#define PAGE_3HS HS_SITE_LOC "/3hs"
	#define PAGE_THEMES HS_SITE_LOC "/wiki/theme-installation"
	#define PAGE_DONATE HS_SITE_LOC "/donate"
	#define PAGE_SWITCHING_SD_CARDS HS_SITE_LOC "/wiki/switching-sd-cards"
#endif

#define STUB(id) [str::id] = lang_strtab[lang::english][str::id]
#define RAW(lid, sid) &lang_strtab[lid][sid]

template <typename T> T unstring(const std::string& s);
template<> unsigned unstring(const std::string& s) { return strtoul(s.c_str(), NULL, 10); }
template<> unsigned long unstring(const std::string& s) { return strtoul(s.c_str(), NULL, 10); }
template<> unsigned long long unstring(const std::string& s) { return strtoull(s.c_str(), NULL, 10); }

typedef const char *(*i18n_string_get_func)(const std::vector<std::string>&);
typedef struct I18NStringStore {
	union {
		const char *string;
		i18n_string_get_func function;
	};
	u8 info;
} I18NStringStore;

#define INFO_NONE   (0)
#define INFO_ISFUNC (1 << 0)

EOF

$header_file .= <<EOF;
/* this file was generated by ./lang/make.pl
 * DO NOT EDIT */

#ifndef inc_i18n_tab_hh
#define inc_i18n_tab_hh

namespace lang
{
	enum _enum
	{

EOF

my @names_ids;
my @lang_names;
my @names;

for my $i (0..$#languages) {
	my $native_name = parse_lang_file "$languages[$i]";
	if($native_name) {
		push @names, $native_name;
		my $up = uc $languages[$i];
		push @names_ids, "LANGNAME_$up";
		push @lang_names, "$languages[$i]";
		$header_file .= <<EOF;
		$languages[$i] = $i,
EOF
	}
}

$header_file .= <<EOF;
		// === //
		_i_max,
	};

	using type = unsigned short int;
}

namespace str
{
	enum _enum
	{
EOF

for my $i (0..$#string_ids) {
	$header_file .= <<EOF
		$string_ids[$i] = $i,
EOF
}

$header_file .= <<EOF;
		// === //
		_i_max,
	};

	using type = unsigned short int;
}

EOF

for my $i (0..$#names) {
	$header_file .= <<EOF;
#define $names_ids[$i] "$names[$i]"
EOF
}

$header_file .= <<EOF;

namespace i18n
{
	const char *langname(lang::type);
}

EOF

chomp $main_table;
$source_file .= <<EOF;
// [str::xxx] is a GCC extension
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
static I18NStringStore lang_strtab[lang::_i_max][str::_i_max] =
{
$main_table
};
#pragma GCC diagnostic pop

const char *i18n::langname(lang::type id)
{
	switch(id)
	{
EOF

foreach my $lang (@lang_names) {
	my $def = uc $lang;
	$def = "LANGNAME_$def";
	$source_file .= "\tcase lang::$lang: return $def;\n";
}

$source_file .= <<EOF;
	}
	return "invalid";
}
EOF

$header_file .= <<EOF;
#define I18N_ALL_NAMES \\
EOF

foreach my $name (@names_ids) {
	$header_file .= "\t$name, \\\n";
}

$header_file .= <<EOF;

#define I18N_ALL_IDS \\
EOF

foreach my $id (@lang_names) {
	$header_file .= "\tlang::$id, \\\n";
}

$header_file .= <<EOF;

// A(name_string, name_id)
#define I18N_ALL_LANG_ITER(A) \\
EOF

for my $i (0..$#lang_names) {
	my $id = $lang_names[$i];
	my $name = $names_ids[$i];
	$header_file .= "\tA($name, lang::$id) \\\n";
}

$header_file .= "\n";
for my $i (0..$#lang_names) {
	my $id = $lang_names[$i];
	my $is_stalled = grep /^$id$/, @stalled_langs;
	$header_file .= '#define IS_STALLED_'.$id.' '.$is_stalled."\n";
}

$header_file .= <<EOF;

#endif
EOF

unless ($dry_run) {
	my $old_header = "";
	my $rheader;
	open $rheader, "<", "$build_dir/i18n_tab.hh" and do {
		local $/;
		$old_header = <$rheader>;
	};

	# Try to not write the header to save us a recompile if we just changed a single string
	$old_header eq $header_file or do {
		my $header;
		open $header, ">", "$build_dir/i18n_tab.hh" or die "failed to write i18n_tab.hh";
		print $header $header_file;
		close $header;
	};

	open my $source, ">", "$build_dir/i18n_tab.cc" or die "failed to write i18n_tab.cc";
	print $source $source_file;
	close $source;
}

if($langs_w_missing) {
	print "missing $total_missing strings in $langs_w_missing languages\n";
} else {
	print "translations are complete\n"
}

