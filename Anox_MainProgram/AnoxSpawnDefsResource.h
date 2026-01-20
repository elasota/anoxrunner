#pragma once

#include "AnoxResourceManager.h"

#include "rkit/Core/Span.h"

namespace anox { namespace data {
	struct UserEntityDef;
	struct EntityClassDef;
} }

namespace anox
{
	class AnoxSpawnDefsResourceLoaderImpl;

	class AnoxSpawnDefsResourceBase : public AnoxResourceBase
	{
	public:
		struct SpawnDef
		{
			const data::EntityClassDef *m_eclass = nullptr;
			void *m_data = nullptr;
		};
	};

	class AnoxSpawnDefsResourceLoaderBase : public AnoxCIPathKeyedResourceLoader<AnoxSpawnDefsResourceBase>
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxSpawnDefsResourceLoaderBase> &outLoader);
	};
}
