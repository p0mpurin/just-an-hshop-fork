#ifndef nblib_objects_top_title_hh
#define nblib_objects_top_title_hh

#include <nblib/nb/raw_array.hh>
#include <nblib/nb.hh>
#include <string>

namespace nb
{
	template <typename TString, typename TRawArray>
	struct NbTopTitle
	{
		u64 dlCount;
		u32 id;
		u32 preferred_alt_idx;
		TString name;
		TString alt;
		TRawArray alt_names;
	};


	using NbRawTopTitle = NbTopTitle<nb::BlobPtr, nb::RawArrayPtr>;

	class TopTitle : public NbTopTitle<std::string, std::vector<std::string>>
	{
	public:
		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbRawTopTitle))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbRawTopTitle *thdr = reinterpret_cast<NbRawTopTitle *>(header);

			nb::ldr(this->dlCount, thdr->dlCount);
			nb::ldr(this->id, thdr->id);
			nb::ldr(this->preferred_alt_idx, thdr->preferred_alt_idx);

			if (nb::ldr(thdr->name)) this->name = std::string(reinterpret_cast<char *>(&blob[nb::ldr(thdr->name)]));
			if (nb::ldr(thdr->alt)) this->alt = std::string(reinterpret_cast<char *>(&blob[nb::ldr(thdr->alt)]));
			if (nb::ldr(thdr->alt_names))
				return nb::raw_array::parse_inline<std::string>(this->alt_names, nb::raw_helpers::utf8_str, nb::ldr(thdr->alt_names), blob, blob_size);

			return nb::StatusCode::SUCCESS;
		}
	};
};
#endif
