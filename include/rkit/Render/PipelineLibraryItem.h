#pragma once

namespace rkit::render
{
	struct IPipelineLibrary;
}

namespace rkit::render
{
	struct RenderPassDesc;

	struct IPipelineLibraryItemResolver
	{
		virtual IInternalRenderPass *ResolveCompiled(const RenderPassDesc &renderPass) const = 0;
	};

	template<class TDesc, class TCompiled>
	class PipelineLibraryItemRef
	{
	public:
		PipelineLibraryItemRef();
		PipelineLibraryItemRef(const IPipelineLibraryItemResolver &itemResolver, const TDesc *desc);

		bool IsValid() const;

		const TDesc *ResolveDesc() const;
		const TCompiled *ResolveCompiled() const;

	private:
		const IPipelineLibraryItemResolver *m_itemResolver;
		const TDesc *m_desc;
	};
}

#include "rkit/Core/RKitAssert.h"

namespace rkit::render
{

	template<class TDesc, class TCompiled>
	inline PipelineLibraryItemRef<TDesc, TCompiled>::PipelineLibraryItemRef()
		: m_itemResolver(nullptr)
		, m_desc(nullptr)
	{
	}

	template<class TDesc, class TCompiled>
	inline PipelineLibraryItemRef<TDesc, TCompiled>::PipelineLibraryItemRef(const IPipelineLibraryItemResolver &itemResolver, const TDesc *desc)
		: m_itemResolver(&itemResolver)
		, m_desc(desc)
	{
	}

	template<class TDesc, class TCompiled>
	inline bool PipelineLibraryItemRef<TDesc, TCompiled>::IsValid() const
	{
		return m_desc != nullptr;
	}

	template<class TDesc, class TCompiled>
	inline const TDesc *PipelineLibraryItemRef<TDesc, TCompiled>::ResolveDesc() const
	{
		return m_desc;
	}

	template<class TDesc, class TCompiled>
	inline const TCompiled *PipelineLibraryItemRef<TDesc, TCompiled>::ResolveCompiled() const
	{
		if (!m_desc)
			return nullptr;

		return m_itemResolver->ResolveCompiled(*m_desc);
	}
}
