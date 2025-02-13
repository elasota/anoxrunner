#pragma once

#include <cstddef>

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
}

namespace rkit::render::priv
{
	template<class T>
	struct CommandListDynamicCastHelper
	{
		static T *DynamicCast(IBaseCommandList &commandList);
	};

	template<>
	struct CommandListDynamicCastHelper<ICopyCommandList>
	{
		static ICopyCommandList *DynamicCast(IBaseCommandList &commandList);
	};

	template<>
	struct CommandListDynamicCastHelper<IComputeCommandList>
	{
		static IComputeCommandList *DynamicCast(IBaseCommandList &commandList);
	};

	template<>
	struct CommandListDynamicCastHelper<IGraphicsCommandList>
	{
		static IGraphicsCommandList *DynamicCast(IBaseCommandList &commandList);
	};

	template<>
	struct CommandListDynamicCastHelper<IGraphicsComputeCommandList>
	{
		static IGraphicsComputeCommandList *DynamicCast(IBaseCommandList &commandList);
	};
}

namespace rkit::render
{
	struct IBaseCommandList;
	struct IInternalCommandAllocator;

	struct IInternalCommandList
	{
	protected:
		virtual ~IInternalCommandList() {}
	};

	struct IBaseCommandList
	{
		friend struct priv::CommandListDynamicCastHelper<IComputeCommandList>;
		friend struct priv::CommandListDynamicCastHelper<IGraphicsCommandList>;
		friend struct priv::CommandListDynamicCastHelper<IGraphicsComputeCommandList>;
		friend struct priv::CommandListDynamicCastHelper<ICopyCommandList>;

		template<class T>
		T *DynamicCast();

	protected:
		virtual IComputeCommandList *ToComputeCommandList() = 0;
		virtual IGraphicsCommandList *ToGraphicsCommandList() = 0;
		virtual IGraphicsComputeCommandList *ToGraphicsComputeCommandList() = 0;
		virtual ICopyCommandList *ToCopyCommandList() = 0;

		virtual IInternalCommandList *ToInternalCommandList() = 0;

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

namespace rkit::render
{
	template<class T>
	T *IBaseCommandList::DynamicCast()
	{
		return priv::CommandListDynamicCastHelper<T>::DynamicCast(*this);
	}
}

namespace rkit::render::priv
{
	template<class T>
	inline T *CommandListDynamicCastHelper<T>::DynamicCast(IBaseCommandList &commandList)
	{
		return nullptr;
	}

	inline ICopyCommandList *CommandListDynamicCastHelper<ICopyCommandList>::DynamicCast(IBaseCommandList &commandList)
	{
		return commandList.ToCopyCommandList();
	}

	inline IGraphicsCommandList *CommandListDynamicCastHelper<IGraphicsCommandList>::DynamicCast(IBaseCommandList &commandList)
	{
		return commandList.ToGraphicsCommandList();
	}

	inline IComputeCommandList *CommandListDynamicCastHelper<IComputeCommandList>::DynamicCast(IBaseCommandList &commandList)
	{
		return commandList.ToComputeCommandList();
	}

	inline IGraphicsComputeCommandList *CommandListDynamicCastHelper<IGraphicsComputeCommandList>::DynamicCast(IBaseCommandList &commandList)
	{
		return commandList.ToGraphicsComputeCommandList();
	}
}
