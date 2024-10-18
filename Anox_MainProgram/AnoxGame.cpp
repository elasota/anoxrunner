#include "anox/AnoxGame.h"

#include "rkit/Core/NewDelete.h"

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
