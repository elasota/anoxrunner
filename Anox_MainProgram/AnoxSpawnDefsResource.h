#pragma once

#include "AnoxResourceManager.h"

#include "rkit/Core/Span.h"

namespace anox { namespace data {
	struct UserEntityDef;
	struct EntityClassDef;
	struct EntitySpawnDataChunks;
} }

namespace anox
{
	class AnoxSpawnDefsResourceLoaderImpl;

	class AnoxSpawnDefsResourceBase : public AnoxResourceBase
	{
	public:
		struct SpawnDef
		{
			uint32_t m_eclassIndex = 0;
			uint32_t m_dataOffset = 0;
		};

		virtual rkit::Span<const SpawnDef> GetSpawnDefs() const = 0;
		virtual const data::EntitySpawnDataChunks &GetChunks() const = 0;
		virtual rkit::Span<const uint8_t> GetDataBuffer() const = 0;
	};

	class AnoxSpawnDefsResourceLoaderBase : public AnoxCIPathKeyedResourceLoader<AnoxSpawnDefsResourceBase>
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxSpawnDefsResourceLoaderBase> &outLoader);
	};
}
