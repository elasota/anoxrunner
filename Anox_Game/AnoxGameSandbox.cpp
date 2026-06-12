#include "anox/Sandbox/AnoxGame.sb.generated.inl"

#include "anox/Game/UserEntityDefValues.h"

#include "anox/AnoxModule.h"

#include "AnoxGameSession.h"

#include "rkit/Sandbox/SandboxModuleImpl.h"

#include "rkit/Core/String.h"
#include "rkit/Core/Vector.h"
#include "rkit/Core/LogDriver.h"

namespace anox::game
{
	RKIT_SANDBOX_DEFINE_MODULE_CLASS(anox::game::sandbox::SandboxAPI, AnoxGameSandboxModule)
}

RKIT_SANDBOX_IMPLEMENT_MODULE(Anox, Game, anox::game::AnoxGameSandboxModule)

RKIT_SANDBOX_IMPLEMENT_MODULE_GLOBALS

#if !!RKIT_SANDBOX_USE_PRIVATE_DRIVERS
namespace anox::game::sandbox
{
	class InternalMallocDriver final : public rkit::IMallocDriver
	{
	public:
		struct alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) MemBlockHeader
		{
			size_t m_size = 0;
			uint32_t m_mmid = 0;
		};

		static void *StaticAlloc(size_t size);
		static void *StaticRealloc(void *ptr, size_t size);
		static void StaticFree(void *ptr);

	protected:
		void *InternalAlloc(size_t size) override;
		void *InternalRealloc(void *ptr, size_t size) override;
		void InternalFree(void *ptr) override;
	};

	class InternalLogDriver final : public rkit::ILogDriver
	{
	public:
		void LogMessage(rkit::LogSeverity severity, const rkit::StringSliceView &msg) override;
		void VLogMessage(rkit::LogSeverity severity, const rkit::StringSliceView &msg, const rkit::FormatParameterList<rkit::Utf8Char_t> &args) override;

	private:
		class VectorOutputFormatter final : public rkit::IFormatStringWriter<rkit::Utf8Char_t>
		{
		public:
			explicit VectorOutputFormatter(rkit::Vector<rkit::Utf8Char_t> &v);

			void WriteChars(const rkit::ConstSpan<rkit::Utf8Char_t> &chars) override;
			rkit::CharacterEncoding GetEncoding() const override;

			rkit::PackedResultAndExtCode GetResult() const;

		private:
			rkit::Vector<rkit::Utf8Char_t> &m_v;
			rkit::PackedResultAndExtCode m_result;
		};
	};

	void *InternalMallocDriver::StaticAlloc(size_t size)
	{
		if (size == 0)
			return nullptr;

		if ((std::numeric_limits<size_t>::max() - size) < sizeof(MemBlockHeader))
			return nullptr;

		void *ptr = nullptr;
		uint32_t mmid = 0;
		anox::game::sandbox::SandboxImports::MemAlloc(ptr, mmid, size + sizeof(MemBlockHeader));

		MemBlockHeader *header = static_cast<MemBlockHeader *>(ptr);
		header->m_mmid = mmid;
		header->m_size = size;
		return header + 1;
	}

	void *InternalMallocDriver::StaticRealloc(void *ptr, size_t size)
	{
		if (ptr == nullptr)
			return StaticAlloc(size);

		if (size == 0)
		{
			StaticFree(ptr);
			return nullptr;
		}

		const MemBlockHeader *header = static_cast<const MemBlockHeader *>(ptr) - 1;

		if (header->m_size == size)
			return ptr;

		void *newPtr = StaticAlloc(size);
		if (!newPtr)
			return nullptr;

		memcpy(newPtr, ptr, std::min(header->m_size, size));
		anox::game::sandbox::SandboxImports::MemFree(header->m_mmid);

		return newPtr;
	}

	void InternalMallocDriver::StaticFree(void *ptr)
	{
		if (ptr == nullptr)
			return;

		const MemBlockHeader *header = static_cast<const MemBlockHeader *>(ptr) - 1;
		anox::game::sandbox::SandboxImports::MemFree(header->m_mmid);
	}

	void *InternalMallocDriver::InternalAlloc(size_t size)
	{
		return StaticAlloc(size);
	}

	void *InternalMallocDriver::InternalRealloc(void *ptr, size_t size)
	{
		return StaticRealloc(ptr, size);
	}

	void InternalMallocDriver::InternalFree(void *ptr)
	{
		StaticFree(ptr);
	}

	void InternalLogDriver::LogMessage(rkit::LogSeverity severity, const rkit::StringSliceView &msg)
	{
		SandboxImports::LogUtf8Message(static_cast<uint32_t>(severity), const_cast<rkit::Utf8Char_t *>(msg.GetChars()), msg.Length());
	}

	void InternalLogDriver::VLogMessage(rkit::LogSeverity severity, const rkit::StringSliceView &msg, const rkit::FormatParameterList<rkit::Utf8Char_t> &args)
	{
		rkit::Vector<rkit::Utf8Char_t> v;

		VectorOutputFormatter formatter(v);
		rkit::FormatString(formatter, msg, args);

		if (rkit::utils::ResultIsOK(formatter.GetResult()))
			this->LogMessage(severity, rkit::StringSliceView(v.ToSpan()));
	}


	InternalLogDriver::VectorOutputFormatter::VectorOutputFormatter(rkit::Vector<rkit::Utf8Char_t> &v)
		: m_v(v)
		, m_result(rkit::utils::PackResult(rkit::ResultCode::kOK))
	{
	}

	void InternalLogDriver::VectorOutputFormatter::WriteChars(const rkit::ConstSpan<rkit::Utf8Char_t> &chars)
	{
		if (!rkit::utils::ResultIsOK(m_result))
			return;

		m_result = RKIT_TRY_EVAL(m_v.Append(chars));
	}

	rkit::CharacterEncoding InternalLogDriver::VectorOutputFormatter::GetEncoding() const
	{
		return rkit::CharacterEncoding::kUTF8;
	}

	rkit::PackedResultAndExtCode InternalLogDriver::VectorOutputFormatter::GetResult() const
	{
		return m_result;
	}

}
#endif

namespace anox::game::sandbox
{
	::rkit::Result SandboxExports::Initialize(void *&outGameSessionObject, void *&outGameSessionMem)
	{
#if !!RKIT_SANDBOX_USE_PRIVATE_DRIVERS
		{
			void *memAllocDriverPtr = InternalMallocDriver::StaticAlloc(sizeof(InternalMallocDriver));
			if (!memAllocDriverPtr)
				RKIT_THROW(rkit::ResultCode::kOutOfMemory);

			InternalMallocDriver *mallocDriver = new (memAllocDriverPtr) InternalMallocDriver();

			rkit::SimpleObjectAllocation<rkit::IMallocDriver> mallocDriverPtr;
			mallocDriverPtr.m_alloc = mallocDriver;
			mallocDriverPtr.m_mem = memAllocDriverPtr;
			mallocDriverPtr.m_obj = mallocDriver;

			rkit::GetMutableDrivers().m_mallocDriver = mallocDriverPtr;
		}

		{
			rkit::UniquePtr<InternalLogDriver> logDriver;
			RKIT_CHECK(rkit::New<InternalLogDriver>(logDriver));
			rkit::GetMutableDrivers().m_logDriver = logDriver.Detach();
		}
#endif

		rkit::UniquePtr<Session> session;
		RKIT_CHECK(Session::Create(session, rkit::GetDrivers().m_mallocDriver.Get()));

		rkit::SimpleObjectAllocation<Session> sessionAllocation = session.Detach();

		outGameSessionObject = sessionAllocation.m_obj;
		outGameSessionMem = sessionAllocation.m_mem;

		RKIT_RETURN_OK;
	}

	rkit::Result SandboxExports::Shutdown(void *gameSessionObject, void *gameSessionMem)
	{
		if (gameSessionObject)
		{
			rkit::SimpleObjectAllocation<Session> sessionAllocation;
			sessionAllocation.m_obj = static_cast<Session *>(gameSessionObject);
			sessionAllocation.m_mem = gameSessionMem;
			sessionAllocation.m_alloc = rkit::GetDrivers().m_mallocDriver.Get();

			rkit::Delete(sessionAllocation);
		}

#if !!RKIT_SANDBOX_USE_PRIVATE_DRIVERS
		rkit::SafeDelete(rkit::GetMutableDrivers().m_logDriver);
		rkit::SafeDelete(rkit::GetMutableDrivers().m_mallocDriver);
#endif

		RKIT_RETURN_OK;
	}

	rkit::Result SandboxExports::MTAsync_SpawnInitialEntities(void *gameSession,
		void *entityTypes, size_t numEntityTypes,
		void *spawnData, size_t numSpawnData,
		void *stringLengths, size_t numStrings,
		void *stringData, size_t numStringData,
		void *udefValues, size_t numUDefs,
		void *udefStringData, size_t udefStringDataSize
		)
	{
		Session *session = static_cast<Session *>(gameSession);

		const rkit::ConstSpan<rkit::endian::LittleUInt32_t> entityTypesSpan(static_cast<const rkit::endian::LittleUInt32_t *>(entityTypes), numEntityTypes);
		const rkit::ConstSpan<uint8_t> spawnDataSpan(static_cast<const uint8_t *>(spawnData), numSpawnData);
		const rkit::ConstSpan<rkit::endian::LittleUInt32_t> stringLengthsSpan(static_cast<const rkit::endian::LittleUInt32_t *>(stringLengths), numStrings);
		const rkit::ConstSpan<uint8_t> stringDataSpan(static_cast<const uint8_t *>(stringData), numStringData);
		const rkit::ConstSpan<game::UserEntityDefValues> udefValuesSpan(static_cast<const game::UserEntityDefValues *>(udefValues), numUDefs);
		const rkit::ConstSpan<uint8_t> udefDescsDataSpan(static_cast<const uint8_t *>(udefStringData), udefStringDataSize);

		rkit::Vector<rkit::ByteStringView> spawnDataStrings;
		{
			uint32_t stringDataPos = 0;
			for (const rkit::endian::LittleUInt32_t &stringLength : stringLengthsSpan)
			{
				RKIT_THROW(rkit::ResultCode::kNotYetImplemented);
			}
		}

		rkit::Vector<rkit::ByteString> udefDescs;
		{
			uint32_t udefDescPos = 0;
			for (const game::UserEntityDefValues &udef : udefValuesSpan)
			{
				if (udef.m_descLength > 0)
				{
					rkit::ByteStringConstructionBuffer cbuf;
					RKIT_CHECK(cbuf.Allocate(udef.m_descLength));

					rkit::CopySpanNonOverlapping(cbuf.GetSpan(), udefDescsDataSpan.SubSpan(udefDescPos, udef.m_descLength));

					RKIT_CHECK(udefDescs.Append(rkit::ByteString(std::move(cbuf))));

					udefDescPos += udef.m_descLength;
				}
			}
		}

		RKIT_CHECK(session->AsyncSpawnInitialEntities(session->GetWorld(), entityTypesSpan, spawnDataSpan, std::move(spawnDataStrings), udefValuesSpan, std::move(udefDescs)));
		RKIT_RETURN_OK;
	}

	rkit::Result SandboxExports::MTAsync_RunFrame(void *gameSession)
	{
		Session *session = static_cast<Session *>(gameSession);
		return session->AsyncRunFrame(session->GetWorld());
	}

	rkit::Result SandboxExports::MTAsync_LoadMapScriptPackage(void *gameSession, void *scriptCatalogData, size_t scriptCatalogSize)
	{
		Session *session = static_cast<Session *>(gameSession);
		return session->AsyncLoadMapScriptPackage(
			rkit::ConstSpan<uint8_t>(static_cast<const uint8_t *>(scriptCatalogData), scriptCatalogSize).ReinterpretCast<const rkit::data::ContentID>());
	}

	rkit::Result SandboxExports::MTAsync_PostSpawnInitialEntities(void *gameSession)
	{
		Session *session = static_cast<Session *>(gameSession);
		return session->AsyncPostSpawnInitialEntities(session->GetWorld());
	}

	rkit::Result SandboxExports::MTAsync_StartGlobalSession(void *gameSession)
	{
		Session *session = static_cast<Session *>(gameSession);
		return session->AsyncStartGlobalSession();
	}

	::rkit::Result SandboxExports::MTAsync_EnterGameSession(void *gameSession)
	{
		Session *session = static_cast<Session *>(gameSession);
		return session->AsyncEnterGameSession(session->GetWorld());
	}

	rkit::Result SandboxExports::WaitForMainThread(bool &isFinished, void *sessionPtr)
	{
		isFinished = false;
		return static_cast<Session *>(sessionPtr)->WaitForMainThreadCoro(isFinished);
	}
}
