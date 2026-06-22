/** \example determine_legitimacy.c
 *  \brief   This example shows how to determine the legitimacy
 *           of a CIA file.
 */

#include <nnc/sigcert.h>
#include <nnc/ticket.h>
#include <nnc/ncch.h>
#include <nnc/tmd.h>
#include <nnc/cia.h>


int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		fprintf(stderr,
			"usage: %s [cia-file...]\n"
			"legit = both TMD and ticket are cryptographically signed and all hashes line up.\n"
			"piratelegit = TMD is cryptographically signed and all hashes line up.\n"
			"standard = all hashes line up.\n"
			"\n"
			"“lol i dunno what to make of those GM9 terms\n"
			" fake legit legit\n"
			" legit legit fake fake legit”\n"
		, argv[0]);
		return 1;
	}

	nnc_keyset kset = NNC_KEYSET_INIT;
	nnc_seeddb seeddb;
	nnc_result res;
	int ret = 0;

	if((res = nnc_scan_seeddb(&seeddb)) != NNC_R_OK)
		fprintf(stderr, "Failed to load seeddb: %s. Titles with new crypto will not work.\n", nnc_strerror(res));
	nnc_keyset_default(&kset, NNC_KEYSET_RETAIL);

	for(int i = 1; i < argc; ++i)
	{
		const char *fname = argv[i];
		nnc_rstream *rs;
		nnc_file file;
		if((res = nnc_file_open(&file, fname)) != NNC_R_OK)
		{
			fprintf(stderr, "%s: %s.\n", fname, nnc_strerror(res));
			ret = 1;
			continue;
		}
		rs = NNC_RSP(&file);

		nnc_cinfo_record info_records[NNC_CINFO_MAX_SIZE];
		nnc_cia_content_reader reader = { .chunks = NULL };
		nnc_certchain chain = { .len = 0 };
		nnc_u16 cindex = UINT16_MAX;
		nnc_sha_hash digest;
		nnc_tmd_header tmd;
		nnc_cia_header cia;
		nnc_ticket tik;
		nnc_subview sv;

		bool tmd_legit = false, tik_legit = false;

		if((res = nnc_read_cia_header(rs, &cia)) != NNC_R_OK) goto err;
		nnc_cia_open_certchain(&cia, rs, &sv);
		if((res = nnc_read_certchain(NNC_RSP(&sv), &chain, false)) != NNC_R_OK) goto err;
		nnc_cia_open_tmd(&cia, rs, &sv);
		if((res = nnc_read_tmd_header(NNC_RSP(&sv), &tmd)) != NNC_R_OK) goto err;
		if((res = nnc_tmd_signature_hash(NNC_RSP(&sv), &tmd, digest)) != NNC_R_OK) goto err;
		if(nnc_verify_signature(&chain, &tmd.sig, digest) == NNC_R_OK) tmd_legit = true;
		if(nnc_verify_read_tmd_info_records(NNC_RSP(&sv), &tmd, info_records) != NNC_R_OK || !nnc_verify_tmd_chunk_records(NNC_RSP(&sv), &tmd, info_records))
		{
			res = NNC_R_CORRUPT;
			goto err;
		}
		nnc_cia_open_ticket(&cia, rs, &sv);
		if((res = nnc_read_ticket(NNC_RSP(&sv), &tik)) != NNC_R_OK) goto err;
		if((res = nnc_ticket_signature_hash(NNC_RSP(&sv), &tik, digest)) != NNC_R_OK) goto err;
		if(nnc_verify_signature(&chain, &tik.sig, digest) == NNC_R_OK) tik_legit = true;
		if((res = nnc_cia_make_reader(&cia, rs, &kset, &reader)) != NNC_R_OK) goto err;
		/* now we verify everything else */
		NNC_FOREACH_CINDEX(cindex, cia.content_index)
		{
			nnc_cia_content_stream content;
			nnc_chunk_record *chunk;
			if((res = nnc_cia_open_content(&reader, cindex, &content, &chunk)) != NNC_R_OK)
				goto err;
			nnc_sha256_hash digest;
			if((res = nnc_crypto_sha256_stream(NNC_RSP(&content), digest)) != NNC_R_OK)
			{
				NNC_RS_CALL0(content, close);
				goto err;
			}
			NNC_RS_CALL0(content, close);
			if(!nnc_crypto_hasheq(digest, chunk->hash))
			{
				res = NNC_R_CORRUPT;
				goto err;
			}
		}

		if(tmd_legit && tik_legit)
			puts("legit");
		else if(tmd_legit)
			puts("piratelegit");
		else
			puts("standard");

out:
		nnc_cia_free_reader(&reader);
		nnc_free_certchain(&chain);
		NNC_RS_CALL0(file, close);
		continue;
err:
		if(cindex == UINT16_MAX)
			fprintf(stderr, "%s: %s.\n", fname, nnc_strerror(res));
		else
			fprintf(stderr, "%s: NCCH %u: %s.\n", fname, cindex, nnc_strerror(res));
		ret = 1;
		goto out;
	}

	nnc_free_seeddb(&seeddb);
	return ret;
}

