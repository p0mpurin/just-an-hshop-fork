
#include <nnc/stream.h>
#include <nnc/romfs.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <nnc/utf.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

void die(const char *fmt, ...);


static void print_dir(nnc_romfs_ctx *ctx, nnc_romfs_info *dir, int indent)
{
	nnc_romfs_iterator it = nnc_romfs_mkit(ctx, dir);
	nnc_romfs_info ent;
	while(nnc_romfs_next(&it, &ent))
	{
		printf("%*s%s%s\n", indent, "", nnc_romfs_info_filename(ctx, &ent), ent.type == NNC_ROMFS_DIR ? "/" : "");
		if(ent.type == NNC_ROMFS_DIR)
			print_dir(ctx, &ent, indent + 1);
	}
}

int romfs_main(int argc, char *argv[])
{
	if(argc != 2) die("usage: %s <file>", argv[0]);
	const char *romfs_file = argv[1];

	nnc_file f;
	if(nnc_file_open(&f, romfs_file) != NNC_R_OK)
		die("f->open() failed");

	nnc_romfs_ctx ctx;
	if(nnc_init_romfs(NNC_RSP(&f), &ctx) != NNC_R_OK)
		die("nnc_init_romfs() failed");

	printf(
		"== %s ==\n"
		" == RomFS header ==\n"
		"  Directory Hash Offset      : 0x%" PRIX64 "\n"
		"                 Length      : 0x%X\n"
		"  Directory Metadata Offset  : 0x%" PRIX64 "\n"
		"                     Length  : 0x%X\n"
		"  File Hash Offset           : 0x%" PRIX64 "\n"
		"            Length           : 0x%X\n"
		"  File Metadata Offset       : 0x%" PRIX64 "\n"
		"                Length       : 0x%X\n"
		"  Data Offset                : 0x%" PRIX64 "\n"
		" == RomFS files & directories ==\n"
	, romfs_file, ctx.header.dir_hash.offset
	, ctx.header.dir_hash.length, ctx.header.dir_meta.offset
	, ctx.header.dir_meta.length, ctx.header.file_hash.offset
	, ctx.header.file_hash.length, ctx.header.file_meta.offset
	, ctx.header.file_meta.length, ctx.header.data_offset);

	nnc_romfs_info info;
	if(nnc_get_info(&ctx, &info, "/") != NNC_R_OK)
		die("failed root directory info");
	puts(" /");
	print_dir(&ctx, &info, 2);

	nnc_free_romfs(&ctx);

	NNC_RS_CALL0(f, close);
	return 0;
}

static void extract_dir(nnc_romfs_ctx *ctx, nnc_romfs_info *info, const char *path, int baselen)
{
	if(access(path, F_OK) != 0 && mkdir(path, 0777) != 0)
		die("failed to create directory '%s': %s", path, strerror(errno));
	printf("%s/\n", path + baselen);
	nnc_romfs_iterator it = nnc_romfs_mkit(ctx, info);
	nnc_romfs_info ent;
	char pathbuf[2048];
	int len = strlen(path);
	strcpy(pathbuf, path);
	pathbuf[len++] = '/';
	while(nnc_romfs_next(&it, &ent))
	{
		strcpy(pathbuf + len, nnc_romfs_info_filename(ctx, &ent));
		if(ent.type == NNC_ROMFS_DIR)
			extract_dir(ctx, &ent, pathbuf, baselen);
		else
		{
			puts(pathbuf + baselen);
			FILE *out = fopen(pathbuf, "w");
			/* empty files just need to be touched */
			if(ent.u.f.size)
			{
				/* slurping the file is a bit inefficient for large
				 * files but it's fine for this test */
				nnc_u8 *cbuf = malloc(ent.u.f.size);
				nnc_subview sv;
				if(nnc_romfs_open_subview(ctx, &sv, &ent) != NNC_R_OK)
					goto file_fail;
				nnc_u32 r;
				if(NNC_RS_CALL(sv, read, cbuf, ent.u.f.size, &r) != NNC_R_OK)
					goto file_fail;
				if(r != ent.u.f.size) goto file_fail;
				if(fwrite(cbuf, ent.u.f.size, 1, out) != 1)
					goto file_fail;
out:
				fclose(out);
				free(cbuf);
				continue;
file_fail:
				fprintf(stderr, "fail: ");
				goto out;
			}
			fclose(out);
		}
	}
}

int xromfs_main(int argc, char *argv[])
{
	if(argc != 3) die("usage: %s <file> <output-directory>", argv[0]);
	const char *romfs_file = argv[1];
	const char *output = argv[2];

	nnc_file f;
	if(nnc_file_open(&f, romfs_file) != NNC_R_OK)
		die("nnc_file_open() failed on '%s'", romfs_file);

	nnc_romfs_ctx ctx;
	if(nnc_init_romfs(NNC_RSP(&f), &ctx) != NNC_R_OK)
		die("nnc_init_romfs() failed");

	nnc_romfs_info info;
	if(nnc_get_info(&ctx, &info, "/") != NNC_R_OK)
		die("failed root directory info");
	extract_dir(&ctx, &info, output, strlen(output));

	nnc_free_romfs(&ctx);

	NNC_RS_CALL0(f, close);
	return 0;
}

int bromfs_main(int argc, char *argv[])
{
	if(argc != 3) die("usage: %s <input-directory> <output-file>", argv[0]);
	const char *input_dir = argv[1];
	const char *output = argv[2];

	nnc_wfile wf;
	nnc_vfs vfs;

	nnc_result res;

	if((res = nnc_vfs_init(&vfs)) != NNC_R_OK)
	{
		fprintf(stderr, "failed to init VFS: %s\n", nnc_strerror(res));
		return 1;
	}
	if((res = nnc_vfs_link_directory(&vfs.root_directory, input_dir, nnc_vfs_identity_transform, NULL)) != NNC_R_OK)
	{
		nnc_vfs_free(&vfs);
		fprintf(stderr, "failed to link real directory '%s' to VFS: %s\n", input_dir, nnc_strerror(res));
		return 1;
	}

	if((res = nnc_wfile_open(&wf, output)) != NNC_R_OK)
	{
		nnc_vfs_free(&vfs);
		fprintf(stderr, "failed to open output file '%s': %s\n", output, nnc_strerror(res));
		return 1;
	}
	res = nnc_write_romfs(&vfs, NNC_WSP(&wf));
	wf.funcs->close(NNC_WSP(&wf));
	nnc_vfs_free(&vfs);

	if(res != NNC_R_OK)
	{
		fprintf(stderr, "failed to write romfs: %s\n", nnc_strerror(res));
		return 1;
	}

	return 0;
}

