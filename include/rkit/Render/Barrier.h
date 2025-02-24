#pragma once

#include "rkit/Core/Span.h"
#include "rkit/Core/EnumMask.h"
#include "rkit/Core/Optional.h"

#include "PipelineStage.h"
#include "ResourceAccess.h"
#include "RenderDefs.h"
#include "ImagePlane.h"

#include <cstdint>

namespace rkit::render
{
	struct IBufferResource;
	struct IImageResource;
	struct IBaseCommandQueue;

	struct GlobalBarrier
	{
		PipelineStageMask_t m_priorStages;
		ResourceAccessMask_t m_priorAccess;
		PipelineStageMask_t m_subsequentStages;
		ResourceAccessMask_t m_subsequentAccess;
	};

	struct BufferMemoryBarrier
	{
		PipelineStageMask_t m_priorStages;
		ResourceAccessMask_t m_priorAccess;
		PipelineStageMask_t m_subsequentStages;
		ResourceAccessMask_t m_subsequentAccess;

		IBaseCommandQueue *m_sourceQueue = nullptr;
		IBaseCommandQueue *m_destQueue = nullptr;

		uint64_t m_offset = 0;
		uint64_t m_size = 0;
	};

	struct ImageMemoryBarrier
	{
		PipelineStageMask_t m_priorStages;
		ResourceAccessMask_t m_priorAccess;
		PipelineStageMask_t m_subsequentStages;
		ResourceAccessMask_t m_subsequentAccess;

		IImageResource *m_image = nullptr;

		IBaseCommandQueue *m_sourceQueue = nullptr;
		IBaseCommandQueue *m_destQueue = nullptr;

		ImageLayout m_priorLayout = ImageLayout::Undefined;
		ImageLayout m_subsequentLayout = ImageLayout::Undefined;

		Optional<EnumMask<ImagePlane>> m_planes;

		uint32_t m_firstMipLevel = 0;
		uint32_t m_numMipLevels = 1;
		uint32_t m_firstArrayElement = 0;
		uint32_t m_numArrayElements = 1;
	};

	struct BarrierGroup
	{
		Span<GlobalBarrier> m_globalBarriers;
		Span<BufferMemoryBarrier> m_bufferMemoryBarriers;
		Span<ImageMemoryBarrier> m_imageMemoryBarriers;
	};
}
