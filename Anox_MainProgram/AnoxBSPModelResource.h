#pragma once

#include "AnoxResourceManager.h"

#include "rkit/Math/Vec.h"

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
		struct DrawSurfTriRange
		{
			uint32_t m_firstTri = 0;
			uint32_t m_numTris = 0;
		};

		struct DrawClusterVertRange
		{
			uint32_t m_firstVert = 0;
			uint32_t m_numVerts = 0;
		};
	};
}
