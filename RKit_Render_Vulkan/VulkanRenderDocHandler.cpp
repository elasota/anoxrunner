#include "VulkanRenderDoc.h"
#include "rkit/Win32/IncludeWindows.h"

#include "renderdoc_app.h"

namespace rkit::render::vulkan
{
	RENDERDOC_API_1_7_0 *g_rdapi = nullptr;

	void VulkanRenderDocHandler::Load()
	{
	}
}
