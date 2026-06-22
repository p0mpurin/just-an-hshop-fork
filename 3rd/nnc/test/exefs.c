
#include <nnc/stream.h>
#include <nnc/exefs.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void die(const char *fmt, ...);


int extract_exefs_main(int argc, char *argv[])
{
	if(argc != 3) die("usage: %s <file> <output>", argv[0]);
	const char *exefs_file = argv[1];
	const char *outdir = argv[2];

	mkdir(outdir, 0777);

	nnc_file f;
	if(nnc_file_open(&f, exefs_file) != NNC_R_OK)
		die("f->open() failed");

	nnc_exefs_file_header headers[NNC_EXEFS_MAX_FILES];
	if(nnc_read_exefs_header(NNC_RSP(&f), headers, NULL) != NNC_R_OK)
		die("failed reading exefs file headers");

	char pathbuf[128];
	for(nnc_u8 i = 0; i < NNC_EXEFS_MAX_FILES && nnc_exefs_file_in_use(&headers[i]); ++i)
	{
		nnc_subview sv;
		nnc_exefs_subview(NNC_RSP(&f), &sv, &headers[i]);

		printf("%8s @ %08X [%08X] (", headers[i].name, headers[i].offset, headers[i].size);
		for(nnc_u8 j = 0; j < sizeof(nnc_sha256_hash); ++j)
			printf("%02X", headers[i].hash[j]);
		nnc_sha256_hash digest;
		nnc_crypto_sha256_stream(NNC_RSP(&sv), digest);
		printf(nnc_crypto_hasheq(digest, headers[i].hash) ? "      OK" : "  NOT OK");
		printf(")");

		const char *fname = NULL;
		if(strcmp(headers[i].name, ".code") == 0)
			fname = "code.bin";
		else if(strcmp(headers[i].name, "icon") == 0)
			fname = "icon.icn";
		else if(strcmp(headers[i].name, "banner") == 0)
			fname = "banner.bnr";
		else if(strcmp(headers[i].name, "logo") == 0)
			fname = "logo.darc.lz";
		else
		{
			fflush(stdout);
			fprintf(stderr, ". Unknown file in exefs '%s', bad file?", headers[i].name);
			fflush(stderr);
			fname = headers[i].name;
		}

		nnc_u32 read_size;
		nnc_u8 *buf = malloc(headers[i].size);
		NNC_RS_CALL(sv, seek_abs, 0);
		if(NNC_RS_CALL(sv, read, buf, headers[i].size, &read_size) != NNC_R_OK || read_size != headers[i].size)
			die("\nfailed to extract exefs file %s", headers[i].name);
		sprintf(pathbuf, "%s/%s", outdir, fname);
		printf(" => %s\n", pathbuf);
		FILE *ef = fopen(pathbuf, "w");
		if(fwrite(buf, headers[i].size, 1, ef) != 1)
			die("failed to write exefs file '%s' to '%s'", headers[i].name, pathbuf);
		fclose(ef);
	}

	NNC_RS_CALL0(f, close);
	return 0;
}

int build_exefs_main(int argc, char *argv[])
{
	if(argc != 3)
		die("usage: %s <directory-name> <output-exefs-file>", argv[0]);

	const char *dir = argv[1];
	const char *out = argv[2];

	int ret = 0;
	nnc_result res;

	nnc_wfile outf = { NULL, 0, NULL };
	nnc_vfs vfs;

	if((res = nnc_vfs_init(&vfs)) != NNC_R_OK) goto err;
	if((res = nnc_vfs_link_directory(&vfs.root_directory, dir, nnc_vfs_identity_transform, NULL)) != NNC_R_OK)
		goto err;

	if((res = nnc_wfile_open(&outf, out)) != NNC_R_OK) goto err;
	if((res = nnc_write_exefs(&vfs, NNC_WSP(&outf))) != NNC_R_OK) goto err;

out:
	if(outf.funcs != NULL)
		NNC_WS_CALL0(outf, close);
	nnc_vfs_free(&vfs);
	return ret;
err:
	fprintf(stderr, "Error while building ExeFS: %s\n", nnc_strerror(res));
	ret = 1;
	goto out;
}

