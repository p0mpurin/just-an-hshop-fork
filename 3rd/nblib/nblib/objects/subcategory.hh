#ifndef nblib_objects_subcategory_hh
#define nblib_objects_subcategory_hh

#include <nblib/nb.hh>
#include <string>

namespace nb
{
	template <typename TString>
	struct NbSubcategory
	{
		u32 id;
		TString disp;
		TString name;
		TString desc;
		bool standalone;
	};

	class Subcategory : public NbSubcategory<std::string>
	{
	public:
		static constexpr const char *magic = "SCAT";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbSubcategory<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbSubcategory<nb::BlobPtr> *shdr = reinterpret_cast<NbSubcategory<nb::BlobPtr> *>(header);

			nb::ldr(this->id, shdr->id);
			nb::ldr(this->standalone, shdr->standalone);

			char *strdata = reinterpret_cast<char *>(blob);

			this->name = std::string(&strdata[nb::ldr(shdr->name)]);
			this->disp = std::string(&strdata[nb::ldr(shdr->disp)]);

			if (this->standalone && nb::ldr(shdr->desc))
				this->desc = std::string(&strdata[nb::ldr(shdr->desc)]);

			return nb::StatusCode::SUCCESS;
		}
	};
};
#endif
