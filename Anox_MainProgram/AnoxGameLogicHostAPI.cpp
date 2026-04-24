#include "anox/Sandbox/AnoxGame.host.generated.inl"

#include "AnoxGameResourceManager.h"
#include "AnoxGameSandboxEnv.h"

#include "rkit/Core/Path.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Optional.h"

#include "rkit/Data/ContentID.h"
#include "rkit/Sandbox/Sandbox.h"

namespace anox::game::sandbox
{
	::rkit::Result HostExports::MemAlloc(::rkit::sandbox::Environment &env, ::rkit::sandbox::IThreadContext *thread, ::rkit::sandbox::Address_t &ptr, uint32_t &mmid, ::rkit::sandbox::Address_t size)
	{
		if (size == 0 || size > std::numeric_limits<size_t>::max())
		{
			ptr = 0;
			RKIT_RETURN_OK;
		}

		return static_cast<AnoxGameSandboxEnvironment &>(env).m_sandbox->AllocDynamicMemory(ptr, mmid, size);
	}

	::rkit::Result HostExports::MemFree(::rkit::sandbox::Environment &env, ::rkit::sandbox::IThreadContext *thread, uint32_t mmid)
	{
		return static_cast<AnoxGameSandboxEnvironment &>(env).m_sandbox->ReleaseDynamicMemory(mmid);
	}

	::rkit::Result HostExports::GetContentIDKeyedResource(::rkit::sandbox::Environment &envBase, ::rkit::sandbox::IThreadContext *thread, uint32_t &reqID, uint32_t resourceType, ::rkit::sandbox::Address_t contentIDAddr)
	{
		AnoxGameSandboxEnvironment &env = static_cast<AnoxGameSandboxEnvironment &>(envBase);

		void *cidPtr = nullptr;
		RKIT_CHECK(env.m_sandbox->AccessMemoryRange(cidPtr, contentIDAddr, sizeof(rkit::data::ContentID)));

		rkit::data::ContentID cid = *static_cast<const rkit::data::ContentID *>(cidPtr);

		return env.m_resManager->GetContentIDKeyedResource(reqID, resourceType, cid);
	}

	::rkit::Result HostExports::GetCIPathKeyedResource(::rkit::sandbox::Environment &envBase, ::rkit::sandbox::IThreadContext *thread, uint32_t &reqID, uint32_t resourceType, ::rkit::sandbox::Address_t charsAddr, size_t charsSize)
	{
		AnoxGameSandboxEnvironment &env = static_cast<AnoxGameSandboxEnvironment &>(envBase);

		void *charsPtr = nullptr;
		RKIT_CHECK(env.m_sandbox->AccessMemoryRange(charsPtr, charsAddr, charsSize));

		rkit::Span<const rkit::Utf8Char_t> charsSpan(static_cast<const rkit::Utf8Char_t *>(charsPtr), charsSize);
		if (!rkit::CharacterEncodingValidator<rkit::CharacterEncoding::kUTF8>::ValidateSpan(charsSpan))
			RKIT_THROW(rkit::ResultCode::kInvalidUnicode);

		rkit::CIPath path;
		RKIT_CHECK(path.SetFromUTF8(rkit::StringSliceView(charsSpan)));

		return env.m_resManager->GetCIPathKeyedResource(reqID, resourceType, path);
	}


	::rkit::Result HostExports::FinishLoadingResourceRequest(::rkit::sandbox::Environment &envBase, ::rkit::sandbox::IThreadContext *thread, bool &completed, uint32_t &resID, uint32_t requestID)
	{
		AnoxGameSandboxEnvironment &env = static_cast<AnoxGameSandboxEnvironment &>(envBase);

		rkit::Optional<uint32_t> resultResID;
		RKIT_CHECK(env.m_resManager->TryFinishLoadingResourceRequest(resultResID, requestID));

		if (resultResID.IsSet())
		{
			resID = resultResID.Get();
			completed = true;
		}
		else
		{
			resID = 0;
			completed = false;
		}

		RKIT_RETURN_OK;
	}

	::rkit::Result HostExports::CancelResourceRequest(::rkit::sandbox::Environment &envBase, ::rkit::sandbox::IThreadContext *thread, uint32_t requestID)
	{
		AnoxGameSandboxEnvironment &env = static_cast<AnoxGameSandboxEnvironment &>(envBase);
		return env.m_resManager->DiscardRequest(requestID);
	}

	::rkit::Result HostExports::DiscardResource(::rkit::sandbox::Environment &envBase, ::rkit::sandbox::IThreadContext *thread, uint32_t resID)
	{
		AnoxGameSandboxEnvironment &env = static_cast<AnoxGameSandboxEnvironment &>(envBase);
		return env.m_resManager->DiscardResource(resID);
	}
}

