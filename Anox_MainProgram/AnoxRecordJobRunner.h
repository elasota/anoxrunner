#pragma once

namespace rkit
{
	namespace render
	{
		struct IBaseCommandAllocator;
		struct IGraphicsCommandAllocator;
		struct IGraphicsComputeCommandAllocator;
		struct IComputeCommandAllocator;
		struct ICopyCommandAllocator;
	}
}


namespace anox
{
	struct IRecordJobRunner
	{
		virtual ~IRecordJobRunner() {}

		virtual rkit::Result RunBase(rkit::render::IBaseCommandAllocator &commandAllocator) = 0;
	};

	template<class T>
	struct ITypedRecordJobRunner : public IRecordJobRunner
	{
		virtual ~ITypedRecordJobRunner() {}

		virtual rkit::Result RunRecord(T &commandAllocator) = 0;

	protected:
		rkit::Result RunBase(rkit::render::IBaseCommandAllocator &commandAllocator) override final;
	};

	typedef ITypedRecordJobRunner<rkit::render::IComputeCommandAllocator> IComputeRecordJobRunner_t;
	typedef ITypedRecordJobRunner<rkit::render::IGraphicsComputeCommandAllocator> IGraphicsComputeRecordJobRunner_t;
	typedef ITypedRecordJobRunner<rkit::render::IGraphicsCommandAllocator> IGraphicsRecordJobRunner_t;
	typedef ITypedRecordJobRunner<rkit::render::ICopyCommandAllocator> ICopyRecordJobRunner_t;
}

#include "rkit/Core/Result.h"
#include "rkit/Core/RKitAssert.h"
#include "rkit/Render/CommandAllocator.h"

namespace anox
{
	template<class T>
	rkit::Result ITypedRecordJobRunner<T>::RunBase(rkit::render::IBaseCommandAllocator &commandAllocator)
	{
		T *retypedCmdAllocator = commandAllocator.DynamicCast<T>();

		return this->RunRecord(*retypedCmdAllocator);
	}
}
