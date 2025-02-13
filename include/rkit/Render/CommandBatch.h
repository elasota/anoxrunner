#pragma once

namespace rkit
{
	struct Result;
}

namespace rkit::render
{
	struct IBaseCommandBatch
	{
		virtual Result Submit() = 0;
		virtual Result WaitForCompletion() = 0;
	};

	struct ICopyCommandBatch : public IBaseCommandBatch
	{
	};

	struct IComputeCommandBatch : public virtual ICopyCommandBatch
	{
	};

	struct IGraphicsCommandBatch : public virtual ICopyCommandBatch
	{
	};

	struct IGraphicsComputeCommandBatch : public IGraphicsCommandBatch, public IComputeCommandBatch
	{
	};
}
