#pragma once

#include "rkit/BuildSystem/DependencyGraph.h"

#include "rkit/Vulkan/GraphicsPipeline.h"

namespace rkit { namespace buildsystem { namespace vulkan
{
	uint32_t CreateNodeTypeIDForStage(render::vulkan::GraphicPipelineStage pipelineStage);

	Result CreatePipelineCompiler(UniquePtr<IDependencyNodeCompiler> &outCompiler);
	Result CreateGraphicsPipelineStageCompiler(render::vulkan::GraphicPipelineStage stage, UniquePtr<IDependencyNodeCompiler> &outCompiler);
} } } // rkit::buildsystem::vulkan
