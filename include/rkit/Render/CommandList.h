#pragma once

#include <cstddef>

namespace rkit
{
	struct Result;
}

namespace rkit::render
{
	struct BaseCommandListRef;
	struct ICopyCommandList;
	struct IComputeCommandList;
	struct IGraphicsCommandList;
	struct IGraphicsComputeCommandList;

	struct IBaseCommandList;
	struct IInternalCommandAllocator;

	struct IInternalCommandList
	{
	protected:
		virtual ~IInternalCommandList() {}
	};

	struct IBaseCommandList
	{
		virtual IComputeCommandList *ToComputeCommandList() = 0;
		virtual IGraphicsCommandList *ToGraphicsCommandList() = 0;
		virtual IGraphicsComputeCommandList *ToGraphicsComputeCommandList() = 0;
		virtual ICopyCommandList *ToCopyCommandList() = 0;

		virtual IInternalCommandList *ToInternalCommandList() = 0;

	protected:
		virtual ~IBaseCommandList() {}
	};

	struct ICopyCommandList : public virtual IBaseCommandList
	{
	};

	struct IComputeCommandList : public virtual ICopyCommandList
	{
	};

	struct IGraphicsCommandList : public virtual ICopyCommandList
	{
	};

	struct IGraphicsComputeCommandList : public IComputeCommandList, public IGraphicsCommandList
	{
	};
}
