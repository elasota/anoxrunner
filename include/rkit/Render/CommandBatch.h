#pragma once

#include "rkit/Core/CoreDefs.h"

#include "PipelineStage.h"

namespace rkit
{
	template<class T>
	class EnumMask;
}

namespace rkit::render
{
	struct ICopyCommandEncoder;
	struct IComputeCommandEncoder;
	struct IGraphicsCommandEncoder;

	struct ISwapChainSyncPoint;
	struct IRenderPassInstance;

	struct IBaseCommandBatch
	{
		virtual Result Submit() = 0;
		virtual Result WaitForCompletion() = 0;
		virtual Result CloseBatch() = 0;
	};

	struct ICopyCommandBatch : public IBaseCommandBatch
	{
		virtual Result OpenCopyCommandEncoder(ICopyCommandEncoder *&outCopyCommandEncoder) = 0;
	};

	struct IComputeCommandBatch : public virtual ICopyCommandBatch
	{
		virtual Result OpenComputeCommandEncoder(IComputeCommandEncoder *&outCopyCommandEncoder) = 0;
	};

	struct IGraphicsCommandBatch : public virtual ICopyCommandBatch
	{
		virtual Result OpenGraphicsCommandEncoder(IGraphicsCommandEncoder *&outCopyCommandEncoder, IRenderPassInstance &rpi) = 0;
	};

	struct IGraphicsComputeCommandBatch : public IGraphicsCommandBatch, public IComputeCommandBatch
	{
	};
}
