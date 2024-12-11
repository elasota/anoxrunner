#include "anox/AnoxGame.h"

#include "rkit/Core/Drivers.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NewDelete.h"

#include "rkit/Render/RenderDriver.h"

namespace anox
{
	class AnoxGame final : public IAnoxGame
	{
	public:
		rkit::Result Start() override;
		rkit::Result RunFrame() override;
		bool IsExiting() const override;

	private:
		bool m_isExiting = false;
	};
}

namespace anox
{
	rkit::Result AnoxGame::Start()
	{
		// Create render driver
		rkit::IModule* renderModule = rkit::GetDrivers().m_moduleDriver->LoadModule(rkit::IModuleDriver::kDefaultNamespace, "Render_Vulkan", nullptr);
		if (!renderModule)
			return rkit::ResultCode::kModuleLoadFailed;

		rkit::render::IRenderDriver *renderDriver = static_cast<rkit::render::IRenderDriver *>(rkit::GetDrivers().FindDriver(rkit::IModuleDriver::kDefaultNamespace, "Render_Vulkan"));

		if (!renderDriver)
		{
			rkit::log::Error("Missing render driver");
			return rkit::ResultCode::kModuleLoadFailed;
		}

		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxGame::RunFrame()
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	bool AnoxGame::IsExiting() const
	{
		return m_isExiting;
	}

	rkit::Result anox::IAnoxGame::Create(rkit::UniquePtr<IAnoxGame> &outGame)
	{
		return rkit::New<AnoxGame>(outGame);
	}
}
