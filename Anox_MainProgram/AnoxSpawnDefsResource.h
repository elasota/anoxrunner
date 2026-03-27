#pragma once

#include "AnoxResourceManager.h"

#include "anox/Game/SpawnDef.h"

#include "rkit/Core/Span.h"

namespace anox { namespace data {
	struct UserEntityDef;
	struct EntityClassDef;
	struct EntitySpawnDataChunks;
} }

namespace anox
{
	class AnoxEntityDefResourceBase;
	class AnoxSpawnDefsResourceLoaderImpl;

	class AnoxSpawnDefsResourceBase : public AnoxResourceBase
	{
	public:
		virtual rkit::Span<const game::SpawnDef> GetSpawnDefs() const = 0;
		virtual const data::EntitySpawnDataChunks &GetChunks() const = 0;
		virtual rkit::Span<const uint8_t> GetDataBuffer() const = 0;
		virtual rkit::CallbackSpan<AnoxEntityDefResourceBase *, const AnoxSpawnDefsResourceBase *> GetUserEntityDefs() const = 0;
	};

	class AnoxSpawnDefsResourceLoaderBase : public AnoxCIPathKeyedResourceLoader<AnoxSpawnDefsResourceBase>
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxSpawnDefsResourceLoaderBase> &outLoader);
	};
}
