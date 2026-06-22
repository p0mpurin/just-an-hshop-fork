
#include <nnc/stream.h>
#include <nnc/ticket.h>
#include <nnc/crypto.h>
#include <inttypes.h>

void nnc_dumpmem(const nnc_u8 *b, nnc_u32 size);
void die(const char *fmt, ...);

const char *license_type(nnc_u8 lic)
{
	switch(lic)
	{
	case NNC_TICKET_LIC_PERMANENT: return "permanent";
	case NNC_TICKET_LIC_DEMO: return "demo";
	case NNC_TICKET_LIC_TRIAL: return "trial";
	case NNC_TICKET_LIC_RENTAL: return "rental";
	case NNC_TICKET_LIC_SUBSCRIPTION: return "subscription";
	case NNC_TICKET_LIC_SERVICE: return "service";
	}
	return "unknown";
}

int tik_main(int argc, char *argv[])
{
	if(argc != 2) die("usage: %s tik-info <tik-file>", argv[0]);
	const char *tik_file = argv[1];

	nnc_file f;
	if(nnc_file_open(&f, tik_file) != NNC_R_OK)
		die("failed to open '%s'", tik_file);

	nnc_ticket tik;
	if(nnc_read_ticket(NNC_RSP(&f), &tik) != NNC_R_OK)
		die("failed to read ticket from '%s'", tik_file);
	nnc_keyset kset;
	nnc_keyset_default(&kset, NNC_KEYSET_RETAIL);

	printf(
		"== %s ==\n"
		"  Signature Type         : %s\n"
		"  Signature Issuer       : %s\n"
		"  ECC Public Key         :\n"
	, tik_file, nnc_sigstr(tik.sig.type), tik.sig.issuer);
	nnc_dumpmem(tik.ecc_pubkey, sizeof(tik.ecc_pubkey));
	printf(
		"  Version                : %i\n"
		"  CA CRL Version         : %i\n"
		"  Signer CRL Version     : %i\n"
		"  Title Key (Encrypted)  : "
	, tik.version, tik.cacrlversion, tik.signercrlversion);
	nnc_u8 title_key_dec[0x10];
	for(int i = 0; i < 0x10; ++i) printf("%02X", tik.title_key[i]);
	puts("");
	printf("  Title Key (Decrypted)  : ");
	if(nnc_decrypt_tkey(&tik, &kset, title_key_dec) == NNC_R_OK)
		for(int i = 0; i < 0x10; ++i) printf("%02X", title_key_dec[i]);
	else printf("(failed to decrypt)");
	puts("");
	nnc_u8 major, minor, patch;
	nnc_parse_version(tik.title_version, &major, &minor, &patch);
	printf(
		"  Ticket ID              : %016" PRIX64 "\n"
		"  Console ID             : %08" PRIX32 "\n"
		"  Title ID               : %016" PRIX64 "\n"
		"  Title Version          : %i.%i.%i (v%i)\n"
		"  License Type           : %s\n"
		"  Common KeyY            : %i\n"
		"  eShop Account ID       : %08" PRIX32 "\n"
		"  Audit                  : %i\n"
		"  Limits                 :\n"
	, tik.ticket_id, tik.console_id, tik.title_id
	, major, minor, patch, tik.title_version
	, license_type(tik.license_type), tik.common_keyy, tik.eshop_account_id, tik.audit);
	nnc_dumpmem(tik.limits, 0x40);

	printf("  Certificate Validation : ");
	nnc_certchain chain;
	nnc_scan_certchains(&chain);
	nnc_sha_hash digest;
	nnc_ticket_signature_hash(NNC_RSP(&f), &tik, digest);
	puts(nnc_strerror(nnc_verify_signature(&chain, &tik.sig, digest)));
	nnc_free_certchain(&chain);


	NNC_RS_CALL0(f, close);
	return 0;
}

