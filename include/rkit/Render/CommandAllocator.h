#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/DynamicCastable.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	namespace render
	{
		struct ICopyCommandList;
		struct IGraphicsCommandList;
		struct IComputeCommandList;
		struct IGraphicsComputeCommandList;

		struct ICopyCommandBatch;
		struct IGraphicsCommandBatch;
		struct IComputeCommandBatch;
		struct IGraphicsComputeCommandBatch;

		struct IBaseCommandAllocator;
		struct IInternalCommandAllocator;
		struct IInternalCommandList;

		struct ICopyCommandAllocator;
		struct IGraphicsCommandAllocator;
		struct IComputeCommandAllocator;
		struct IGraphicsComputeCommandAllocator;

		struct CommandListHandle;
	}
}

namespace rkit { namespace render
{
	struct IInternalCommandAllocator
	{
		virtual ~IInternalCommandAllocator() {}
	};

	struct IBaseCommandAllocator : public IDynamicCastable<ICopyCommandAllocator, IGraphicsCommandAllocator, IComputeCommandAllocator, IGraphicsComputeCommandAllocator>
	{
		virtual ~IBaseCommandAllocator() {}

		virtual IInternalCommandAllocator *ToInternalCommandAllocator() = 0;

		virtual Result ResetCommandAllocator(bool clearResources) = 0;
	};

	struct ICopyCommandAllocator : public IBaseCommandAllocator
	{
		virtual ~ICopyCommandAllocator() {}

		virtual Result OpenCopyCommandBatch(ICopyCommandBatch *&outCommandBatch, bool cpuWaitable) = 0;
	};

	struct IGraphicsCommandAllocator : public virtual ICopyCommandAllocator
	{
		virtual ~IGraphicsCommandAllocator() {}

		virtual Result OpenGraphicsCommandBatch(IGraphicsCommandBatch *&outCommandBatch, bool cpuWaitable) = 0;
	};

	struct IComputeCommandAllocator : public virtual ICopyCommandAllocator
	{
		virtual ~IComputeCommandAllocator() {}

		virtual Result OpenComputeCommandBatch(IComputeCommandBatch *&outCommandBatch, bool cpuWaitable) = 0;
	};

	struct IGraphicsComputeCommandAllocator : public virtual IGraphicsCommandAllocator, public virtual IComputeCommandAllocator
	{
		virtual ~IGraphicsComputeCommandAllocator() {}

		virtual Result OpenGraphicsComputeCommandBatch(IGraphicsComputeCommandBatch *&outCommandBatch, bool cpuWaitable) = 0;
	};
} } // rkit::render
