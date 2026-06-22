
#ifndef inc_nnc_nncpp_tmd_hh
#define inc_nnc_nncpp_tmd_hh

#include <nncpp/stream.hh>
#include <nncpp/crypto.hh>
#include <nncpp/base.hh>
#include <nnc/tmd.h>

/* TODO: Signature of TMD */
/* TODO: Various "verify" methods */

namespace nnc
{
	class cinfo_record final
	{
	private:
		nnc_cinfo_record record;

	public:
		NNCPP__DEFINE_GETTER_SETTER(offset, this->record.offset, u16)
		NNCPP__DEFINE_GETTER_SETTER(count, this->record.count, u16)
		NNCPP__DEFINE_ARRAY_GETTER_SETTER(hash, this->record.hash, sha256)

		bool is_last() { return NNC_CINFO_IS_LAST(this->record); }
	};
	NNCPP__MUST_WRAP(cinfo_record, nnc_cinfo_record)

	class chunk_record final
	{
	private:
		friend class cia_reader;
		nnc_chunk_record record;

	public:
		enum class flag
		{
			encrypted = NNC_CHUNKF_ENCRYPTED,
			disc      = NNC_CHUNKF_DISC,
			cfm       = NNC_CHUNKF_CFM,
			optional  = NNC_CHUNKF_OPTIONAL,
			shared    = NNC_CHUNKF_SHARED,
		};

		NNCPP__DEFINE_GETTER_SETTER(id, this->record.id, u32)
		NNCPP__DEFINE_GETTER_SETTER(index, this->record.index, u16)
		NNCPP__DEFINE_GETTER_SETTER(flags, this->record.flags, bitfield<u16, flag>)
		NNCPP__DEFINE_GETTER_SETTER(size, this->record.size, u64)
		NNCPP__DEFINE_ARRAY_GETTER_SETTER(hash, this->record.hash, sha256)
	};
	NNCPP__MUST_WRAP(chunk_record, nnc_chunk_record)
	NNCPP__DEFINE_ENUM_BITWISE_OPERATORS(chunk_record::flag)

	class tmd_header final
	{
	public:
		tmd_header() { }

#if NNCPP_ALLOW_IGNORE_ERRORS
		tmd_header(read_stream_like& rs)
		{
			this->read(rs);
		}
#endif

		result read(read_stream_like& rs)
		{
			result ret = (result) nnc_read_tmd_header(rs.as_rstream(), &this->tmd_hdr);
			if(ret == nnc::result::ok)
				this->read_stream = &rs;
			return ret;
		}

		// NNCPP__DEFINE_GETTER_SETTER(signature, this->tmd_hdr.sig, signature&)
		NNCPP__DEFINE_GETTER_SETTER(version, this->tmd_hdr.version, u8)
		NNCPP__DEFINE_GETTER_SETTER(ca_crl_version, this->tmd_hdr.ca_crl_ver, u8)
		NNCPP__DEFINE_GETTER_SETTER(signer_crl_version, this->tmd_hdr.signer_crl_ver, u8)
		NNCPP__DEFINE_GETTER_SETTER(system_version, this->tmd_hdr.sys_ver, u8)
		NNCPP__DEFINE_GETTER_SETTER(title_id, this->tmd_hdr.title_id, nnc::title_id)
		NNCPP__DEFINE_GETTER_SETTER(title_type, this->tmd_hdr.title_type, u32)
		NNCPP__DEFINE_GETTER_SETTER(save_size, this->tmd_hdr.save_size, u32)
		NNCPP__DEFINE_GETTER_SETTER(private_save_size, this->tmd_hdr.priv_save_size, u32)
		NNCPP__DEFINE_GETTER_SETTER(srl_flag, this->tmd_hdr.srl_flag, u8)
		NNCPP__DEFINE_GETTER_SETTER(access_rights, this->tmd_hdr.access_rights, u32)
		NNCPP__DEFINE_GETTER_SETTER(title_version, this->tmd_hdr.title_ver, u16)
		NNCPP__DEFINE_GETTER_SETTER(group_id, this->tmd_hdr.group_id, u16)
		NNCPP__DEFINE_GETTER_SETTER(content_count, this->tmd_hdr.content_count, u16)
		NNCPP__DEFINE_GETTER_SETTER(boot_content, this->tmd_hdr.boot_content, u16)
		NNCPP__DEFINE_ARRAY_GETTER_SETTER(hash, this->tmd_hdr.hash, sha256);

		result read_info_records(dynamic_array<cinfo_record>& out_records)
		{
			if(!this->read_stream) return nnc::result::not_open;
			if(!out_records.allocate(NNC_CINFO_MAX_SIZE))
				return nnc::result::nomem;
			return (result) nnc_read_tmd_info_records(this->read_stream->as_rstream(), &this->tmd_hdr, (nnc_cinfo_record *) out_records.data());
		}

		result read_chunk_records(dynamic_array<chunk_record>& out_records)
		{
			if(!this->read_stream) return nnc::result::not_open;
			if(!out_records.allocate(this->content_count()))
				return nnc::result::nomem;
			return (result) nnc_read_tmd_chunk_records(this->read_stream->as_rstream(), &this->tmd_hdr, (nnc_chunk_record *) out_records.data());
		}

	private:
		read_stream_like *read_stream = nullptr;
		nnc_tmd_header tmd_hdr;
	};
}

#endif

