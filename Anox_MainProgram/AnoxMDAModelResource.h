#pragma once

#include "AnoxResourceManager.h"

namespace anox
{
	class AnoxMDAModelResourceBase;

	class AnoxMDAModelResourceLoaderBase : public AnoxContentIDKeyedResourceLoader<AnoxMDAModelResourceBase>
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxMDAModelResourceLoaderBase> &resLoader);
	};

	class AnoxMDAModelResourceBase : public AnoxResourceBase
	{
	public:
	};
}
