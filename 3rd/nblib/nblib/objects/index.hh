#ifndef nblib_objects_index_hh
#define nblib_objects_index_hh

#include <string>
#include <map>

#include <nblib/nb/array.hh>
#include <nblib/nb.hh>

namespace nb
{
	struct NbIndexMeta
	{
		u32 titles;
		u32 officialTitles;
		u32 legitTitles;
		u64 size;
		u64 downloads;
	};

	template <typename TString>
	struct NbIndexCategoryBase
	{
		NbIndexMeta meta;
		u32 id;
		TString disp;
		TString name;
		TString desc;
	};

	template <typename TString, typename TArray>
	struct NbIndexCategory : NbIndexCategoryBase<TString>
	{
		u8 prio;
		TArray subcategories;
	};

	class IndexCategory;

	class IndexSubcategory : public NbIndexCategoryBase<std::string>
	{
	public:
		static constexpr const char *magic = "IXCB";

		IndexCategory *parent;

		IndexSubcategory() : parent(nullptr) {}

		void set_parent(IndexCategory& p) { this->parent = &p; }

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbIndexCategoryBase<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbIndexCategoryBase<nb::BlobPtr> *chdr = reinterpret_cast<NbIndexCategoryBase<nb::BlobPtr> *>(header);

			memcpy(&this->meta, &chdr->meta, sizeof(chdr->meta));

			char *strdata = reinterpret_cast<char *>(blob);

			nb::ldr(this->id, chdr->id);
			this->name = std::string(&strdata[nb::ldr(chdr->name)]);
			this->disp = std::string(&strdata[nb::ldr(chdr->disp)]);
			this->desc = std::string(&strdata[nb::ldr(chdr->desc)]);

			return nb::StatusCode::SUCCESS;
		}
	};

	using NbRawIndexCategory = NbIndexCategory<nb::BlobPtr, nb::ArrayPtr>;

	class IndexCategory : public NbIndexCategory<std::string, std::map<u8, IndexSubcategory>>
	{
	public:
		static constexpr const char *magic = "IXCT"; // even though this won't be used ever since this obj is in an array

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbRawIndexCategory))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbRawIndexCategory *chdr = reinterpret_cast<NbRawIndexCategory *>(header);

			memcpy(&this->meta, &chdr->meta, sizeof(this->meta));

			char *strdata = reinterpret_cast<char *>(blob);

			nb::ldr(this->id, chdr->id);
			this->name = std::string(&strdata[nb::ldr(chdr->name)]);
			this->disp = std::string(&strdata[nb::ldr(chdr->disp)]);
			this->desc = std::string(&strdata[nb::ldr(chdr->desc)]);
			this->prio = nb::ldr(chdr->prio);

			std::vector<IndexSubcategory> scats;

			nb::StatusCode scats_res = nb::array::parse_inline<nb::IndexSubcategory>(scats, nb::ldr(chdr->subcategories), blob, blob_size);

			if (scats_res != nb::StatusCode::SUCCESS) return scats_res;

			for (IndexSubcategory& scat : scats) {
				this->subcategories.insert(std::make_pair(scat.id, std::move(scat)));
			}

			return nb::StatusCode::SUCCESS;
		};
	};

	template <typename TArray>
	struct NbIndex
	{
		NbIndexMeta meta;
		TArray categories;
		u64 date;
	};

	class Index : public NbIndex<std::map<u8, IndexCategory>>
	{
	public:
		static constexpr const char *magic = "TIDX";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbIndex<nb::ArrayPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbIndex<nb::ArrayPtr> *ihdr = reinterpret_cast<NbIndex<nb::ArrayPtr> *>(header);

			memcpy(&this->meta, &ihdr->meta, sizeof(this->meta));
			nb::ldr(this->date, ihdr->date);

			std::vector<IndexCategory> cat_array;

			nb::StatusCode cat_status = nb::array::parse_inline<IndexCategory>(cat_array, nb::ldr(ihdr->categories), blob, blob_size);

			if (cat_status != nb::StatusCode::SUCCESS) return cat_status;

			for (IndexCategory &cat : cat_array) {
				this->categories.insert(std::make_pair(cat.id, std::move(cat)));
			}

			for (auto& c : this->categories)
			{
				for (auto& s : c.second.subcategories)
				{
					s.second.set_parent(c.second);
				}
			}
			return nb::StatusCode::SUCCESS;
		}
	};


}

#endif
