
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define BUILD_OPTS "build exefs | build romfs"

#define DIE_USAGE() die("usage: [ extract-exefs | exheader-info | extract-romfs | romfs-info | ncch-info | tmd-info | smdh-info | test-u128 | tik-info | cia-unpack | " BUILD_OPTS " ]")
#define DIE_BUILD_USAGE() die("usage: [ " BUILD_OPTS " ]")

static const char *opt = "nnc-test";
__attribute__((noreturn)) void die(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	fprintf(stderr, "%s: ", opt);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	va_end(va);
	exit(1);
}

void print_hash(unsigned char *b)
{
	for(int i = 0; i < 0x20; ++i)
		printf("%02X", b[i]);
}


int extract_exefs_main(int argc, char *argv[]); /* exefs.c */
int rewrite_cia_main(int argc, char *argv[]); /* cia.c */
int ncch_info_main(int argc, char *argv[]); /* ncch.c */
int exheader_main(int argc, char *argv[]); /* exheader.c */
int tmd_info_main(int argc, char *argv[]); /* tmd.c */
int xromfs_main(int argc, char *argv[]); /* romfs.c */
int romfs_main(int argc, char *argv[]); /* romfs.c */
int smdh_main(int argc, char *argv[]); /* smdh.c */
int u128_main(int argc, char *argv[]); /* u128.c */
int tik_main(int argc, char *argv[]); /* tik.c */
int cia_main(int argc, char *argv[]); /* cia.c */

int build_exefs_main(int argc, char *argv[]); /* exefs.c */
int bromfs_main(int argc, char *argv[]); /* romfs.c */

static int build_main(int argc, char *argv[])
{
	if(argc < 2) DIE_BUILD_USAGE();
	const char *cmd = argv[1];
	argv[1] = argv[0];
	--argc;
#define CASE(cmdn, func) if(strcmp(cmd, cmdn) == 0) do { opt = "nnc-test: build " cmdn; return func(argc, &argv[1]); } while(0)
	CASE("exefs", build_exefs_main);
	CASE("romfs", bromfs_main);
#undef CASE
	DIE_BUILD_USAGE();
}

int main(int argc, char *argv[])
{
	if(argc < 2) DIE_USAGE();
	const char *cmd = argv[1];
	argv[1] = argv[0];
	--argc;
#define CASE(cmdn, func) if(strcmp(cmd, cmdn) == 0) do { opt = "nnc-test: " cmdn; return func(argc, &argv[1]); } while(0)
	CASE("extract-exefs", extract_exefs_main);
	CASE("exheader-info", exheader_main);
	CASE("extract-romfs", xromfs_main);
	CASE("ncch-info", ncch_info_main);
	CASE("romfs-info", romfs_main);
	CASE("tmd-info", tmd_info_main);
	CASE("smdh-info", smdh_main);
	CASE("test-u128", u128_main);
	CASE("tik-info", tik_main);
	CASE("cia-unpack", cia_main);
	CASE("rewrite-cia", rewrite_cia_main);
	CASE("build", build_main);
#undef CASE
	DIE_USAGE();
}

