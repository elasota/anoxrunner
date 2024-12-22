#pragma once

namespace rkit::render
{
	struct IBaseCommandList;
	struct ICopyCommandList;
	struct IComputeCommandList;
	struct IGraphicsCommandList;
	struct IGraphicsComputeCommandList;

	struct IBaseCommandList
	{
		virtual IComputeCommandList *ToComputeCommandList() { return nullptr; }
		virtual IGraphicsCommandList *ToGraphicsCommandList() { return nullptr; }
		virtual IGraphicsComputeCommandList *ToGraphicsComputeCommandList() { return nullptr; }
		virtual ICopyCommandList *ToCopyCommandList() { return nullptr; }
	};

	struct ICopyCommandList : public virtual IBaseCommandList
	{
		ICopyCommandList *ToCopyCommandList() override { return this; }
	};

	struct IComputeCommandList : public virtual ICopyCommandList
	{
		IComputeCommandList *ToComputeCommandList() override { return this; }
	};

	struct IGraphicsCommandList : public virtual ICopyCommandList
	{
		IGraphicsCommandList *ToGraphicsCommandList() override { return this; }
	};

	struct IGraphicsComputeCommandList : public IComputeCommandList, public IGraphicsCommandList
	{
		IComputeCommandList *ToComputeCommandList() override { return this; }
		IGraphicsCommandList *ToGraphicsCommandList() override { return this; }
		IGraphicsComputeCommandList *ToGraphicsComputeCommandList() override { return this; }
		ICopyCommandList *ToCopyCommandList() override { return this; }
	};
}
