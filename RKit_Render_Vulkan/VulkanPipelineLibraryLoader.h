#pragma once

#include "rkit/Core/StreamProtos.h"
#include "rkit/Render/PipelineLibraryLoader.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct ISeekableReadStream;
	struct ISeekableWriteStream;
	struct ISeekableReadWriteStream;
}

namespace rkit::data
{
	struct IRenderDataPackage;
}

namespace rkit::utils
{
	struct IShadowFile;
}

namespace rkit::render::vulkan
{
	class VulkanDeviceBase;

	class PipelineLibraryLoaderBase : public IPipelineLibraryLoader
	{
	public:
		static Result Create(VulkanDeviceBase &device, UniquePtr<PipelineLibraryLoaderBase> &loader, UniquePtr<IPipelineLibraryConfigValidator> &&validator,
			UniquePtr<data::IRenderDataPackage> &&package, UniquePtr<ISeekableReadStream> &&packageStream, FilePos_t packageBinaryContentStart);
	};
}
