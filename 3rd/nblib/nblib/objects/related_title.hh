#ifndef nblib_objects_related_title_hh
#define nblib_objects_related_title_hh

#include <nblib/nb.hh>
#include <nblib/nb/single_object.hh>

namespace nb
{
    enum class RelationType : u8
    {
        DLC          = 0,
        Update       = 1,
        DLCActivator = 2,
        Demo         = 3,
        Base         = 4,
        Other        = 255,
    };

    struct NbRelatedTitle
    {
        nb::BlobPtr title;
        RelationType relation;
    };

    template<typename TTitle>
    class RelatedTitle : public TTitle
    {
    public:
        static constexpr const char *magic = "RLTL";
        RelationType relation;

        RelatedTitle() {}

        RelatedTitle(const TTitle& other, RelationType relType)
        {
            static_cast<TTitle &>(*this) = other;
            this->relation = relType;
        }

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbRelatedTitle))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbRelatedTitle *hdr = reinterpret_cast<NbRelatedTitle *>(header);

            nb::StatusCode tstatus = nb::single_object::parse(*static_cast<TTitle *>(this), &blob[nb::ldr(hdr->title)], blob_size - nb::ldr(hdr->title));
            if (tstatus != nb::StatusCode::SUCCESS)
                return tstatus;

            nb::ldr(this->relation, hdr->relation);

            return nb::StatusCode::SUCCESS;
		}
    };
}

#endif