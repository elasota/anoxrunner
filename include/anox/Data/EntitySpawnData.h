#pragma once

#include "rkit/Core/Endian.h"
#include "rkit/Core/FourCC.h"

namespace anox { namespace data {
	struct EntitySpawnDataFile
	{
		static const uint32_t kFourCC = RKIT_FOURCC('E', 'N', 'T', 'S');
		static const uint32_t kVersion = 1;

		rkit::endian::BigUInt32_t m_fourCC;
		rkit::endian::LittleUInt32_t m_version;
	};
} }

#include "rkit/Core/Vector.h"

namespace anox { namespace data {
	struct EntitySpawnDataChunks
	{
		rkit::Vector<rkit::data::ContentID> m_entityDefContentIDs;
		rkit::Vector<rkit::endian::LittleUInt32_t> m_entityTypes;
		rkit::Vector<rkit::endian::LittleUInt32_t> m_entityStringLengths;
		rkit::Vector<uint8_t> m_entityData;
		rkit::Vector<char> m_entityStringData;

		template<class TVisitor>
		rkit::Result VisitAllChunks(const TVisitor &visitor)
		{
			return EntitySpawnDataChunks::StaticVisitAllChunks<EntitySpawnDataChunks, TVisitor>(*this, visitor);
		}

		template<class TVisitor>
		rkit::Result VisitAllChunks(const TVisitor &visitor) const
		{
			return EntitySpawnDataChunks::StaticVisitAllChunks<const EntitySpawnDataChunks, TVisitor>(*this, visitor);
		}


	private:
		template<class TSelf, class TVisitor>
		static rkit::Result StaticVisitAllChunks(TSelf &self, const TVisitor &visitor)
		{
			RKIT_CHECK(visitor.VisitMember(self.m_entityDefContentIDs));
			RKIT_CHECK(visitor.VisitMember(self.m_entityTypes));
			RKIT_CHECK(visitor.VisitMember(self.m_entityStringLengths));
			RKIT_CHECK(visitor.VisitMember(self.m_entityData));
			RKIT_CHECK(visitor.VisitMember(self.m_entityStringData));

			return rkit::ResultCode::kOK;
		}
	};
} }
