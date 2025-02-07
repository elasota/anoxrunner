#pragma once

namespace rkit
{
	struct Result;

	template<class T>
	class UniquePtr;
}

namespace rkit::render
{
	struct ICopyCommandList;
	struct IGraphicsCommandList;
	struct IComputeCommandList;
	struct IGraphicsComputeCommandList;

	struct IBaseCommandAllocator;
	struct IInternalCommandAllocator;
	struct IInternalCommandList;

	struct ICopyCommandAllocator;
	struct IGraphicsCommandAllocator;
	struct IComputeCommandAllocator;
	struct IGraphicsComputeCommandAllocator;

	struct CommandListHandle;

	struct IBaseCommandAllocator
	{
		virtual ~IBaseCommandAllocator() {}

		virtual ICopyCommandAllocator *ToCopyCommandAllocator() = 0;
		virtual IGraphicsCommandAllocator *ToGraphicsCommandAllocator() = 0;
		virtual IComputeCommandAllocator *ToComputeCommandAllocator() = 0;
		virtual IGraphicsComputeCommandAllocator *ToGraphicsComputeCommandAllocator() = 0;

		virtual Result ResetCommandAllocator(bool clearResources) = 0;
	};

	struct ICopyCommandAllocator : public IBaseCommandAllocator
	{
		virtual ~ICopyCommandAllocator() {}

		virtual Result OpenCopyCommandList(ICopyCommandList *&outCommandList) = 0;
	};

	struct IGraphicsCommandAllocator : public virtual ICopyCommandAllocator
	{
		virtual ~IGraphicsCommandAllocator() {}

		virtual Result OpenGraphicsCommandList(IGraphicsCommandList *&outCommandList) = 0;
	};

	struct IComputeCommandAllocator : public virtual ICopyCommandAllocator
	{
		virtual ~IComputeCommandAllocator() {}

		virtual Result OpenComputeCommandList(IComputeCommandList *&outCommandList) = 0;
	};

	struct IGraphicsComputeCommandAllocator : public virtual IGraphicsCommandAllocator, public virtual IComputeCommandAllocator
	{
		virtual ~IGraphicsComputeCommandAllocator() {}

		virtual Result OpenGraphicsComputeCommandList(IGraphicsComputeCommandList *&outCommandList) = 0;
	};
}
