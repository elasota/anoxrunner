#pragma once

#include "rkit/Core/StreamProtos.h"

#include "CommandQueueType.h"

namespace rkit
{
	struct ISeekableReadStream;
}

namespace rkit::data
{
	struct IRenderDataPackage;
}

namespace rkit::render
{
	struct ICopyCommandQueue;
	struct IComputeCommandQueue;
	struct IGraphicsCommandQueue;
	struct IGraphicsComputeCommandQueue;
	struct ICPUWaitableFence;
	struct IPipelineLibraryLoader;
	struct IPipelineLibraryConfigValidator;
	struct IRenderDeviceCaps;

	struct IRenderDevice
	{
		virtual ~IRenderDevice() {}

		virtual ICopyCommandQueue *GetCopyQueue(size_t index) const = 0;
		virtual IComputeCommandQueue *GetComputeQueue(size_t index) const = 0;
		virtual IGraphicsCommandQueue *GetGraphicsQueue(size_t index) const = 0;
		virtual IGraphicsComputeCommandQueue *GetGraphicsComputeQueue(size_t index) const = 0;

		virtual Result CreateCPUWaitableFence(UniquePtr<ICPUWaitableFence> &outFence) = 0;

		virtual const IRenderDeviceCaps &GetCaps() const = 0;

		virtual Result CreatePipelineLibraryLoader(UniquePtr<IPipelineLibraryLoader> &loader, UniquePtr<IPipelineLibraryConfigValidator> &&validator,
			UniquePtr<data::IRenderDataPackage> &&package, UniquePtr<ISeekableReadStream> &&packageStream, FilePos_t packageBinaryContentStart,
			UniquePtr<utils::IShadowFile> &&cacheShadowFile, UniquePtr<ISeekableReadWriteStream> &&cacheStream) = 0;
	};
}
