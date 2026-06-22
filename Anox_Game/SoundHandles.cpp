#include "SoundHandles.h"
#include "SandboxResourceLoader.h"

#include "anox/Sandbox/AnoxGame.sb.generated.h"

namespace anox::game
{
	class SandboxResourceHandle;

	SoundEmitterHandle::~SoundEmitterHandle()
	{
		if (m_emitterID != 0)
			anox::game::sandbox::SandboxImports::SoundEmitter_Destroy(m_emitterID);
	}

	rkit::Result SoundEmitterHandle::Play() const
	{
		return anox::game::sandbox::SandboxImports::SoundEmitter_Play(m_emitterID);
	}

	rkit::Result SoundEmitterHandle::Create(SoundEmitterHandle &outHandle, SoundSourceHandle &&src, const SoundEmitterProperties &emitterProperties)
	{
		outHandle = SoundEmitterHandle();

		uint32_t emitterID = 0;
		RKIT_CHECK(anox::game::sandbox::SandboxImports::SoundEmitter_Create(emitterID, src.GetSourceID(), const_cast<SoundEmitterProperties *>(&emitterProperties)));
		src.UnsafeRelease();

		SoundEmitterHandle tempHandle;
		tempHandle.m_emitterID = emitterID;
		outHandle = std::move(tempHandle);

		RKIT_RETURN_OK;
	}

	SoundSourceHandle::~SoundSourceHandle()
	{
		if (m_srcID != 0)
			anox::game::sandbox::SandboxImports::SoundSource_Destroy(m_srcID);
	}

	rkit::Result SoundSourceHandle::CreateFromFileResource(SoundSourceHandle &outHandle, const SandboxResourceHandle &resHandle, rkit::audio::AudioContainerFormat containerFormat)
	{
		uint32_t srcID = 0;
		RKIT_CHECK(anox::game::sandbox::SandboxImports::SoundSource_CreateFromFileResource(srcID, resHandle.GetResourceID(), static_cast<uint32_t>(containerFormat)));

		SoundSourceHandle tempHandle;
		tempHandle.m_srcID = srcID;
		outHandle = std::move(tempHandle);

		RKIT_RETURN_OK;
	}
}
