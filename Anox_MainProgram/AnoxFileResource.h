#pragma once

#include "AnoxResourceManager.h"

namespace rkit
{
	template<class T>
	class Span;
}

namespace anox
{
	class AnoxFileResourceBase;

	class AnoxPathFileResourceLoaderBase : public AnoxCIPathKeyedResourceLoader<AnoxFileResourceBase>
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxPathFileResourceLoaderBase> &resLoader);
	};

	class AnoxContentFileResourceLoaderBase : public AnoxContentIDKeyedResourceLoader<AnoxFileResourceBase>
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxContentFileResourceLoaderBase> &resLoader);
	};

	class AnoxFileResourceBase : public AnoxResourceBase
	{
	public:
		virtual rkit::Span<const uint8_t> GetContents() const = 0;
	};
}
