#pragma once

#include "rkit/BuildSystem/DependencyGraph.h"

#include "rkit/Vulkan/GraphicsPipeline.h"

namespace rkit { namespace buildsystem { namespace vulkan
{
	struct GlslCApi;

	uint32_t CreateNodeTypeIDForStage(render::vulkan::GraphicPipelineStage pipelineStage);

	Result CreatePipelineCompiler(UniquePtr<IDependencyNodeCompiler> &outCompiler);
	Result CreateGraphicsPipelineStageCompiler(const GlslCApi *glslc, render::vulkan::GraphicPipelineStage stage, UniquePtr<IDependencyNodeCompiler> &outCompiler);
} } } // rkit::buildsystem::vulkan
