/** \example extract_cdn_contents.c
 *  \brief   This example shows how to extract CDN contents + TMD from a CIA file.
 */

#include <nnc/stream.h>
#include <nnc/crypto.h>
#include <nnc/ticket.h>
#include <nnc/ncch.h>
#include <nnc/tmd.h>
#include <nnc/cia.h>

#include <sys/stat.h>
#include <string.h>


int main(int argc, char *argv[])
{
	if(argc < 3)
	{
		fprintf(stderr,
			"usage: %s [cia-file] [output-directory] [-d]\n"
			"Extract CDN contents to a directory from a CIA.\n"
			"Note that these contents are encrypted with the titlekey found in the CIA,\n"
			"that means that illegit tickets may contain the wrong titlekey used for encryption.\n"
			"To save without encryption make the last parameter \"-d\".\n", argv[0]);
		return 1;
	}

	int encrypted = 1;
	if(argc > 3 && strcmp(argv[3], "-d") == 0)
		encrypted = 0;

	const char *cia_name = argv[1];
	const char *output_dir = argv[2];

	mkdir(output_dir, 0777);
	int ret = 1;

	nnc_keyset kset = NNC_KEYSET_INIT;
	nnc_keyset_default(&kset, NNC_KEYSET_RETAIL);

	nnc_file cia_s;
	nnc_result res;
	if((res = nnc_file_open(&cia_s, cia_name)) != NNC_R_OK)
	{
		fprintf(stderr, "%s: %s.\n", cia_name, nnc_strerror(res));
		return 1;
	}

	nnc_cia_content_reader reader = { .chunks = NULL };
	nnc_chunk_record *chunk;
	nnc_cia_header cia;
	nnc_u8 tkey[0x10];
	char fname[1024];
	nnc_wfile outtmd;
	nnc_u8 iv[0x10];
	nnc_subview sv;
	nnc_ticket tik;
	nnc_u32 i;

	if((res = nnc_read_cia_header(NNC_RSP(&cia_s), &cia)) != NNC_R_OK) goto out;
	nnc_cia_open_ticket(&cia, NNC_RSP(&cia_s), &sv);
	if((res = nnc_read_ticket(NNC_RSP(&sv), &tik))) goto out;
	if((res = nnc_decrypt_tkey(&tik, &kset, tkey))) goto out;
	if((res = nnc_cia_make_reader(&cia, NNC_RSP(&cia_s), &kset, &reader)) != NNC_R_OK) goto out;
	NNC_FOREACH_CINDEX(i, cia.content_index)
	{
		nnc_cia_content_stream cs;
		if((res = nnc_cia_open_content(&reader, i, &cs, &chunk)) != NNC_R_OK)
		{
			fprintf(stderr, "%s: NCCH%i: %s.\n", cia_name, i, nnc_strerror(res));
			continue;
		}
		printf("%08X...", chunk->id);

		union _uw {
			struct _a {
				nnc_aes_cbc r0;
				nnc_wfile r1;
			} enc;
			struct _b {
				nnc_wfile r0;
			} dec;
		} writer;

		if(encrypted)
		{
			nnc_cia_get_iv(iv, chunk->index);
			sprintf(fname, "%s/%08X", output_dir, chunk->id);
			if((res = nnc_wfile_open(&writer.enc.r1, fname)) != NNC_R_OK)
				goto out1;
			if((res = nnc_aes_cbc_open_w(&writer.enc.r0, NNC_WSP(&writer.enc.r1), tkey, iv)) != NNC_R_OK)
			{
				NNC_WS_CALL0(writer.enc.r1, close);
				goto out1;
			}
		}
		else
		{
			sprintf(fname, "%s/%08X", output_dir, chunk->id);
			if((res = nnc_wfile_open(&writer.dec.r0, fname)) != NNC_R_OK)
				goto out1;
		}

		res = nnc_copy(NNC_RSP(&cs), NNC_WSP(&writer), NULL);

		if(encrypted) { NNC_WS_CALL0(writer.dec.r0, close); NNC_WS_CALL0(writer.enc.r1, close); }
		else          { NNC_WS_CALL0(writer.enc.r0, close); }
out1:
		fflush(stdout);
		if(res != NNC_R_OK)
			fprintf(stderr, "failed; %s\n", nnc_strerror(res));
		else printf("OK\n");
		NNC_RS_CALL0(cs, close);
	}
	printf("tmd...");
	nnc_cia_open_tmd(&cia, NNC_RSP(&cia_s), &sv);
	sprintf(fname, "%s/tmd.%u", output_dir, tik.title_version);
	if((res = nnc_wfile_open(&outtmd, fname)) != NNC_R_OK) goto out;
	res = nnc_copy(NNC_RSP(&sv), NNC_WSP(&outtmd), NULL);
	NNC_WS_CALL0(outtmd, close);
	if(res == NNC_R_OK)
	{
		ret = 0;
		puts("OK");
	}

out:
	fflush(stdout);
	if(res != NNC_R_OK)
		fprintf(stderr, "%s: %s.\n", cia_name, nnc_strerror(res));
	nnc_cia_free_reader(&reader);
	NNC_RS_CALL0(cia_s, close);
	return ret;
}
