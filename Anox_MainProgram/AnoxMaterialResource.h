#pragma once

#include "AnoxResourceManager.h"

#include "anox/Data/MaterialResourceType.h"

namespace anox
{
	class AnoxMaterialResourceBase : public AnoxResourceBase
	{
	public:
	};

	class AnoxMaterialResourceLoaderBase : public AnoxContentIDKeyedResourceLoader<AnoxMaterialResourceBase>
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxMaterialResourceLoaderBase> &outLoader, data::MaterialResourceType resType);
	};
}
