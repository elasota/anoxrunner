#pragma once

#include "AnoxResourceManager.h"

namespace anox
{
	class AnoxTextureResourceBase : public AnoxResourceBase
	{
	public:
	};

	class AnoxTextureResourceLoaderBase : public AnoxContentIDKeyedResourceLoader<AnoxTextureResourceBase>
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxTextureResourceLoaderBase> &outLoader);
	};
}
