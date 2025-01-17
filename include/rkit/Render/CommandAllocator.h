#pragma once

namespace rkit
{
	struct Result;

	template<class T>
	class UniquePtr;
}

namespace rkit::render
{
	struct IBaseCommandList;
	struct ICopyCommandList;
	struct IGraphicsCommandList;
	struct IComputeCommandList;
	struct IGraphicsComputeCommandList;

	struct IBaseCommandAllocator
	{
		virtual ~IBaseCommandAllocator() {}

		virtual Result ResetCommandAllocator() = 0;
	};

	struct ICopyCommandAllocator : public IBaseCommandAllocator
	{
		virtual ~ICopyCommandAllocator() {}

		virtual Result CreateCopyCommandList(UniquePtr<ICopyCommandList> &outCommandList, bool isBundle) = 0;
	};

	struct IGraphicsCommandAllocator : public virtual ICopyCommandAllocator
	{
		virtual ~IGraphicsCommandAllocator() {}

		virtual Result CreateGraphicsCommandList(UniquePtr<IGraphicsCommandList> &outCommandList, bool isBundle) = 0;
	};

	struct IComputeCommandAllocator : public virtual ICopyCommandAllocator
	{
		virtual ~IComputeCommandAllocator() {}

		virtual Result CreateComputeCommandList(UniquePtr<IComputeCommandList> &outCommandList, bool isBundle) = 0;
	};

	struct IGraphicsComputeCommandAllocator : public virtual IGraphicsCommandAllocator, public virtual IComputeCommandAllocator
	{
		virtual ~IGraphicsComputeCommandAllocator() {}

		virtual Result CreateGraphicsComputeCommandList(UniquePtr<IGraphicsComputeCommandList> &outCommandList, bool isBundle) = 0;
	};
}
