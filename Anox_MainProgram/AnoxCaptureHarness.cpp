#include "AnoxCaptureHarness.h"

#include "anox/AnoxGame.h"

#include "rkit/Core/Future.h"
#include "rkit/Core/Job.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/NewDelete.h"

#include "rkit/Utilities/ThreadPool.h"

#include "AnoxResourceManager.h"

namespace anox
{
	class AnoxRealTimeCaptureHarness final : public ICaptureHarness
	{
	public:
		AnoxRealTimeCaptureHarness(IAnoxGame &game, AnoxResourceManagerBase &resManager);

		rkit::Result Initialize();

		rkit::Result GetConfigurationState(const IConfigurationState **outConfigStatePtr) override;
		rkit::Result GetSessionState(const IConfigurationState **outConfigStatePtr) override;

		rkit::Result GetContentIDKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::data::ContentID &cid) override;
		rkit::Result GetCIPathKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::CIPathView &path) override;
		rkit::Result GetStringKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::StringView &str) override;

	private:
		IAnoxGame &m_game;
		AnoxResourceManagerBase &m_resManager;
	};

	AnoxRealTimeCaptureHarness::AnoxRealTimeCaptureHarness(IAnoxGame &game, AnoxResourceManagerBase &resManager)
		: m_game(game)
		, m_resManager(resManager)
	{
	}

	rkit::Result AnoxRealTimeCaptureHarness::Initialize()
	{
		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxRealTimeCaptureHarness::GetConfigurationState(const IConfigurationState **outConfigStatePtr)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxRealTimeCaptureHarness::GetSessionState(const IConfigurationState **outConfigStatePtr)
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxRealTimeCaptureHarness::GetContentIDKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::data::ContentID &cid)
	{
		return m_resManager.GetContentIDKeyedResource(nullptr, loadFuture, resourceType, cid);
	}

	rkit::Result AnoxRealTimeCaptureHarness::GetCIPathKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::CIPathView &path)
	{
		return m_resManager.GetCIPathKeyedResource(nullptr, loadFuture, resourceType, path);
	}

	rkit::Result AnoxRealTimeCaptureHarness::GetStringKeyedResource(rkit::Future<AnoxResourceRetrieveResult> &loadFuture, uint32_t resourceType, const rkit::StringView &str)
	{
		return m_resManager.GetStringKeyedResource(nullptr, loadFuture, resourceType, str);
	}

	rkit::Result ICaptureHarness::CreateRealTime(rkit::UniquePtr<ICaptureHarness> &outHarness, IAnoxGame &game, AnoxResourceManagerBase &resManager)
	{
		rkit::UniquePtr<AnoxRealTimeCaptureHarness> harness;
		RKIT_CHECK(rkit::New<AnoxRealTimeCaptureHarness>(harness, game, resManager));

		RKIT_CHECK(harness->Initialize());

		outHarness = std::move(harness);

		return rkit::ResultCode::kOK;
	}
}
