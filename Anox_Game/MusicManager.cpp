#include "MusicManager.h"

#include "rkit/Core/NewDelete.h"
#include "rkit/Data/ContentID.h"

#include "anox/Data/ResourceTypeCodes.h"
#include "anox/Game/SoundEmitterProperties.h"

#include "SoundHandles.h"
#include "SandboxResourceLoader.h"

namespace anox::game
{
	class MusicManagerImpl;

	class MusicManagerImpl final : public rkit::OpaqueImplementation<MusicManager>
	{
	public:
		rkit::Result Initialize();
		rkit::Result OnFrame();

		rkit::Result SetLevelMusic(const rkit::data::ContentID &contentID);

	private:
		enum class MusicLoadState
		{
			kEmpty,
			kLoading,
			kReady,
		};

		enum class MusicPlayState
		{
			kPaused,
			kPlaying,
		};

		rkit::data::ContentID m_musicContentID;
		MusicLoadState m_musicLoadState = MusicLoadState::kEmpty;
		MusicPlayState m_musicPlayState = MusicPlayState::kPaused;

		SandboxResourceRequestHandle m_musicRequest;
		SandboxResourceHandle m_musicResource;
		SoundEmitterHandle m_musicEmitter;
	};

	rkit::Result MusicManagerImpl::Initialize()
	{
		RKIT_RETURN_OK;
	}

	rkit::Result MusicManagerImpl::OnFrame()
	{
		if (m_musicLoadState == MusicLoadState::kLoading)
		{
			bool isFinished = false;
			RKIT_CHECK(m_musicRequest.TryFinishLoading(isFinished, m_musicResource));

			if (isFinished)
			{
				m_musicLoadState = MusicLoadState::kReady;

				SoundSourceHandle source;
				RKIT_CHECK(SoundSourceHandle::CreateFromFileResource(source, m_musicResource, rkit::audio::AudioContainerFormat::kMPEGLayer3));

				SoundEmitterProperties emitterProperties;

				RKIT_CHECK(SoundEmitterHandle::Create(m_musicEmitter, std::move(source), emitterProperties));

				m_musicEmitter.Play();
			}
		}

		RKIT_RETURN_OK;
	}

	rkit::Result MusicManagerImpl::SetLevelMusic(const rkit::data::ContentID &contentID)
	{
		m_musicLoadState = MusicLoadState::kLoading;
		RKIT_CHECK(SandboxResourceLoader::LoadContentKeyedResource(m_musicRequest, anox::resloaders::kContentIDRawFileResourceTypeCode, contentID));

		RKIT_RETURN_OK;
	}

	rkit::Result MusicManager::Initialize()
	{
		return Impl().Initialize();
	}

	rkit::Result MusicManager::OnFrame()
	{
		return Impl().OnFrame();
	}

	rkit::Result MusicManager::SetLevelMusic(const rkit::data::ContentID &contentID)
	{
		return Impl().SetLevelMusic(contentID);
	}

	rkit::Result MusicManager::Create(rkit::UniquePtr<MusicManager> &outManager)
	{
		return rkit::NewInitialize<MusicManager>(outManager);
	}
}

RKIT_OPAQUE_IMPLEMENT_DESTRUCTOR(anox::game::MusicManagerImpl)
