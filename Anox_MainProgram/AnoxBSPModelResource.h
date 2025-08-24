#pragma once

#include "AnoxResourceManager.h"

namespace rkit
{
	template<class T>
	class Span;
}

namespace anox
{
	class AnoxBSPModelResourceBase;

	class AnoxBSPModelResourceLoaderBase : public AnoxCIPathKeyedResourceLoader<AnoxBSPModelResourceBase>
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxBSPModelResourceLoaderBase> &resLoader);
	};

	class AnoxBSPModelResourceBase : public AnoxResourceBase
	{
	public:
	};
}
