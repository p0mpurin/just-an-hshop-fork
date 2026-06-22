#ifndef nblib_objects_dl_token_hh
#define nblib_objects_dl_token_hh

#include <string>

#include <nblib/nb.hh>

namespace nb
{
	template <typename TString>
	struct NbDLToken
	{
		u64 expiry;
		u32 id;
		TString token;
	};

	class DLToken : public NbDLToken<std::string>
	{
	public:
		static constexpr const char *magic = "TOKN";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbDLToken<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbDLToken<nb::BlobPtr> *thdr = reinterpret_cast<NbDLToken<nb::BlobPtr> *>(header);

			nb::ldr(this->id, thdr->id);
			nb::ldr(this->expiry, thdr->expiry);

			char *strdata = reinterpret_cast<char *>(blob);

			this->token = std::string(&strdata[nb::ldr(thdr->token)]);

			return nb::StatusCode::SUCCESS;
		}
	};
};

#endif
