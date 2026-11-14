#pragma once

#include "rkit/Core/NoCopy.h"

namespace rkit
{
	template<class T>
	struct ISpan;
}

namespace rkit::render
{
	struct IBufferResource;

	struct ConstantBufferMultiViewBufferSpec
	{
		IBufferResource *m_resource = nullptr;
		uint32_t m_size = 0;
	};

	struct ConstantBufferMultiViewSpec
	{
		const ISpan<const ConstantBufferMultiViewBufferSpec &> *m_buffers = nullptr;
	};

	struct IConstantBufferMultiView : public NoCopy
	{
		virtual ~IConstantBufferMultiView() {}
	};
} // rkit::render
