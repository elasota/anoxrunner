#pragma once

#include "rkit/Core/CoreDefs.h"

namespace rkit
{
	namespace render
	{
		struct IBaseCommandQueue;
		struct IGraphicsCommandQueue;
		struct IGraphicsComputeCommandQueue;
		struct IComputeCommandQueue;
		struct ICopyCommandQueue;
	}
}


namespace anox
{
	struct ISubmitJobRunner
	{
		virtual ~ISubmitJobRunner() {}

		virtual rkit::Result RunBase(rkit::render::IBaseCommandQueue &commandQueue) = 0;
	};

	template<class T>
	struct ITypedSubmitJobRunner : public ISubmitJobRunner
	{
		virtual ~ITypedSubmitJobRunner() {}

		virtual rkit::Result RunSubmit(T &commandQueue) = 0;

	protected:
		rkit::Result RunBase(rkit::render::IBaseCommandQueue &commandQueue) override final;
	};

	typedef ITypedSubmitJobRunner<rkit::render::IComputeCommandQueue> IComputeSubmitJobRunner_t;
	typedef ITypedSubmitJobRunner<rkit::render::IGraphicsComputeCommandQueue> IGraphicsComputeSubmitJobRunner_t;
	typedef ITypedSubmitJobRunner<rkit::render::IGraphicsCommandQueue> IGraphicsSubmitJobRunner_t;
	typedef ITypedSubmitJobRunner<rkit::render::ICopyCommandQueue> ICopySubmitJobRunner_t;
}

#include "rkit/Core/Result.h"
#include "rkit/Core/RKitAssert.h"
#include "rkit/Render/CommandQueue.h"

namespace anox
{
	template<class T>
	rkit::Result ITypedSubmitJobRunner<T>::RunBase(rkit::render::IBaseCommandQueue &commandQueue)
	{
		T *retypedCmdQueue = commandQueue.DynamicCast<T>();

		return this->RunSubmit(*retypedCmdQueue);
	}
}
