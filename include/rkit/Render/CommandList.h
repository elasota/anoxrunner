#pragma once

namespace rkit
{
	struct Result;
}

namespace rkit::render
{
	struct IBaseCommandList;
	struct ICopyCommandList;
	struct IComputeCommandList;
	struct IGraphicsCommandList;
	struct IGraphicsComputeCommandList;

	struct IInternalCommandList
	{
	};

	struct IBaseCommandList
	{
		virtual IComputeCommandList *ToComputeCommandList() = 0;
		virtual IGraphicsCommandList *ToGraphicsCommandList() = 0;
		virtual IGraphicsComputeCommandList *ToGraphicsComputeCommandList() = 0;
		virtual ICopyCommandList *ToCopyCommandList() = 0;

		virtual IInternalCommandList *ToInternalCommandList() = 0;

		virtual bool IsBundle() const = 0;

		virtual Result ResetCommandList() = 0;
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
