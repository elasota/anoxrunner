#include "anox/AnoxGame.h"
#include "anox/AnoxGraphicsSubsystem.h"

#include "rkit/Core/Drivers.h"
#include "rkit/Core/DriverModuleInitParams.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleDriver.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Vector.h"

#include "rkit/Render/DisplayManager.h"

#include "rkit/Render/RenderDevice.h"
#include "rkit/Render/RenderDriver.h"

namespace anox
{
	class AnoxGame final : public IAnoxGame
	{
	public:
		~AnoxGame();

		rkit::Result Start() override;
		rkit::Result RunFrame() override;
		bool IsExiting() const override;

	private:
		bool m_isExiting = false;

		rkit::UniquePtr<IGraphicsSubsystem> m_graphicsSubsystem;
	};
}

namespace anox
{
	AnoxGame::~AnoxGame()
	{
	}

	rkit::Result AnoxGame::Start()
	{
		RKIT_CHECK(IGraphicsSubsystem::Create(m_graphicsSubsystem, anox::RenderBackend::kVulkan));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxGame::RunFrame()
	{
		RKIT_CHECK(m_graphicsSubsystem->TransitionDisplayState());

		return rkit::ResultCode::kOK;
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
