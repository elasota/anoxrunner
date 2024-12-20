#pragma once

namespace rkit
{
	struct Result;

	template<class T>
	class Span;
}

namespace rkit::render
{
	struct ICopyCommandList;
	struct IComputeCommandList;
	struct IGraphicsCommandList;
	struct IGraphicsComputeCommandList;

	struct IBaseCommandQueue
	{
	};

	struct ICopyCommandQueue : public IBaseCommandQueue
	{
		virtual Result ExecuteCopy(const Span<ICopyCommandList*> &cmdLists) = 0;
		Result ExecuteCopy(ICopyCommandList &cmdList);
	};

	struct IComputeCommandQueue : public virtual ICopyCommandQueue
	{
		virtual Result ExecuteCompute(const Span<IComputeCommandList *> &cmdLists) = 0;
		Result ExecuteCompute(IComputeCommandList &cmdList);
	};

	struct IGraphicsCommandQueue : public virtual ICopyCommandQueue
	{
		virtual Result ExecuteCompute(const Span<IGraphicsCommandList *> &cmdLists) = 0;
		Result ExecuteGraphics(IGraphicsCommandList &cmdList);
	};

	struct IGraphicsComputeCommandQueue : public IComputeCommandQueue, public IGraphicsCommandQueue
	{
		virtual Result ExecuteGraphicsCompute(const Span<IGraphicsComputeCommandList *> &cmdLists) = 0;
		Result ExecuteGraphicsCompute(IGraphicsComputeCommandList &cmdList);
	};
}

#include "rkit/Core/Result.h"

