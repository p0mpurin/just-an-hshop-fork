#ifndef nblib_objects_category_hh
#define nblib_objects_category_hh

#include <string>

#include <nblib/nb.hh>

namespace nb
{
	template <typename TString>
	struct NbCategory
	{
		u32 id;
		TString disp;
		TString name;
		TString desc;
		TString subcatDesc;
		u8 prio;
	};

	class Category : public NbCategory<std::string>
	{
	public:
		static constexpr const char *magic = "CATE";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbCategory<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbCategory<nb::BlobPtr> *chdr = reinterpret_cast<NbCategory<nb::BlobPtr> *>(header);

			nb::ldr(this->id, chdr->id);

			char *strdata = reinterpret_cast<char *>(blob);

			this->name = std::string(&strdata[nb::ldr(chdr->name)]);
			this->disp = std::string(&strdata[nb::ldr(chdr->disp)]);
			this->desc = std::string(&strdata[nb::ldr(chdr->desc)]);
			if (nb::ldr(chdr->subcatDesc)) this->subcatDesc = std::string(&strdata[nb::ldr(chdr->subcatDesc)]);

			return nb::StatusCode::SUCCESS;
		}
	};
};
#endif
