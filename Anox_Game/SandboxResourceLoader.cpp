#include "SandboxResourceLoader.h"

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/StringView.h"

#include "anox/Data/ResourceTypeCodes.h"
#include "anox/Sandbox/AnoxGame.sb.generated.h"

namespace anox::game
{
	class SandboxResourceBlockerContext
	{
	public:
		explicit SandboxResourceBlockerContext(SandboxResourceRequestHandle &reqHandle, SandboxResourceHandle &resHandle);

		rkit::CoroThreadBlocker CreateBlocker();

	private:
		SandboxResourceRequestHandle &m_reqHandle;
		SandboxResourceHandle &m_resHandle;

		static bool StaticCheckUnblock(void *context);
		static rkit::Result StaticConsume(void *context);
		static void StaticRelease(void *context);

		rkit::PackedResultAndExtCode m_packedResult = {};
	};

	SandboxResourceBlockerContext::SandboxResourceBlockerContext(SandboxResourceRequestHandle &reqHandle, SandboxResourceHandle &resHandle)
		: m_reqHandle(reqHandle)
		, m_resHandle(resHandle)
	{
	}

	rkit::CoroThreadBlocker SandboxResourceBlockerContext::CreateBlocker()
	{
		return rkit::CoroThreadBlocker::Create(this, StaticCheckUnblock, StaticConsume, StaticRelease, false);
	}

	bool SandboxResourceBlockerContext::StaticCheckUnblock(void *context)
	{
		SandboxResourceBlockerContext *ctx = static_cast<SandboxResourceBlockerContext *>(context);

		if (!ctx->m_reqHandle.IsValid())
			return true;

		bool isFinished = false;
		rkit::PackedResultAndExtCode packedResult = RKIT_TRY_EVAL(ctx->m_reqHandle.TryFinishLoading(isFinished, ctx->m_resHandle));

		if (!rkit::utils::ResultIsOK(packedResult))
		{
			ctx->m_packedResult = packedResult;
			return true;
		}

		return isFinished;
	}

	rkit::Result SandboxResourceBlockerContext::StaticConsume(void *context)
	{
		SandboxResourceBlockerContext *ctx = static_cast<SandboxResourceBlockerContext *>(context);

		return rkit::ThrowIfError(ctx->m_packedResult);
	}

	void SandboxResourceBlockerContext::StaticRelease(void *context)
	{
		SandboxResourceBlockerContext *ctx = static_cast<SandboxResourceBlockerContext *>(context);
		ctx->m_reqHandle = SandboxResourceRequestHandle();
	}

	void SandboxResourceDataBlob::Dispose(uint32_t mmid, void *mem)
	{
		sandbox::SandboxImports::MemFree(mmid);
		(void)mem;
	}

	rkit::Result SandboxResourceLoader::LoadCIPathKeyedResource(SandboxResourceRequestHandle &outRequest, uint32_t resourceType, const rkit::StringSliceView &path)
	{
		uint32_t reqID = 0;
		RKIT_CHECK(sandbox::SandboxImports::GetCIPathKeyedResource(reqID, resourceType, const_cast<rkit::Utf8Char_t *>(path.GetChars()), path.Length()));

		outRequest = SandboxResourceRequestHandle(reqID);

		RKIT_RETURN_OK;
	}

	rkit::Result SandboxResourceLoader::LoadContentKeyedResource(SandboxResourceRequestHandle &outRequest, uint32_t resourceType, const rkit::data::ContentID &cid)
	{
		uint32_t reqID = 0;
		RKIT_CHECK(sandbox::SandboxImports::GetContentIDKeyedResource(reqID, resourceType, const_cast<rkit::data::ContentID *>(&cid)));

		outRequest = SandboxResourceRequestHandle(reqID);

		RKIT_RETURN_OK;
	}

	rkit::Result SandboxResourceLoader::GetFileResourceContents(SandboxResourceDataBlob &outBlob, const SandboxResourceHandle &res)
	{
		void *ptr = nullptr;
		uint32_t mmid = 0;
		size_t size = 0;
		RKIT_CHECK(sandbox::SandboxImports::GetFileResourceContents(ptr, size, mmid, res.GetResourceID()));

		outBlob = SandboxResourceDataBlob(mmid, ptr, size);

		RKIT_RETURN_OK;
	}

	rkit::ResultCoroutine SandboxResourceLoader::BlockingLoadCIPathKeyedFileResource(rkit::ICoroThread &thread, SandboxResourceDataBlob &outBlob, const rkit::StringSliceView &path)
	{
		SandboxResourceRequestHandle req;
		CORO_CHECK(LoadCIPathKeyedResource(req, resloaders::kCIPathRawFileResourceTypeCode, path));

		SandboxResourceHandle res;
		CORO_CHECK(co_await req.WaitForLoaded(thread, res));

		co_return SandboxResourceLoader::GetFileResourceContents(outBlob, res);
	}

	void SandboxResourceHandle::Dispose(uint32_t resID)
	{
		sandbox::SandboxImports::DecRefResource(resID);
	}

	void SandboxResourceRequestHandle::Dispose(uint32_t resID)
	{
		sandbox::SandboxImports::CancelResourceRequest(resID);
	}

	rkit::Result SandboxResourceRequestHandle::TryFinishLoading(bool &outIsFinished, SandboxResourceHandle &outReqHandle)
	{
		bool finished = false;
		uint32_t resID = 0;
		RKIT_CHECK(sandbox::SandboxImports::FinishLoadingResourceRequest(finished, resID, m_requestID));

		if (finished)
		{
			m_requestID = 0;
			outReqHandle = SandboxResourceHandle(resID);
		}

		outIsFinished = finished;

		RKIT_RETURN_OK;
	}

	rkit::ResultCoroutine SandboxResourceRequestHandle::WaitForLoaded(rkit::ICoroThread &thread, SandboxResourceHandle &outResHandle)
	{
		SandboxResourceBlockerContext blockerContext(*this, outResHandle);
		rkit::CoroThreadBlocker blocker = blockerContext.CreateBlocker();
		CORO_CHECK(co_await thread.AwaitBlocker(blocker));
		CORO_RETURN_OK;
	}
}
