#pragma once

#include "AnoxResourceManager.h"

namespace rkit
{
	template<class T>
	class Span;
}

namespace anox
{
	class AnoxFileResourceLoaderBase : public AnoxCIPathKeyedResourceLoader
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxFileResourceLoaderBase> &resLoader);
	};


	class AnoxFileResourceBase : public AnoxResourceBase
	{
	public:
		virtual rkit::Span<const uint8_t> GetContents() const = 0;
	};
}
