#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/UtilitiesDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/NewDelete.h"

#include "DeflateDecompressStream.h"
#include "Json.h"
#include "MutexProtectedStream.h"
#include "RangeLimitedReadStream.h"

namespace rkit
{
	namespace Utilities
	{
		struct IJsonDocument;
	}

	class UtilitiesDriver final : public IUtilitiesDriver
	{
	public:
		Result InitDriver();
		void ShutdownDriver();

		Result CreateJsonDocument(UniquePtr<Utilities::IJsonDocument> &outDocument, IMallocDriver *alloc, IReadStream *readStream) const override;

		Result CreateMutexProtectedReadWriteStream(SharedPtr<IMutexProtectedReadWriteStream> &outStream, UniquePtr<ISeekableReadWriteStream> &&stream) const override;
		Result CreateMutexProtectedReadStream(SharedPtr<IMutexProtectedReadStream> &outStream, UniquePtr<ISeekableReadStream> &&stream) const override;
		Result CreateMutexProtectedWriteStream(SharedPtr<IMutexProtectedWriteStream> &outStream, UniquePtr<ISeekableWriteStream> &&stream) const override;

		Result CreateDeflateDecompressStream(UniquePtr<IReadStream> &outStream, UniquePtr<IReadStream> &&compressedStream) const override;
		Result CreateRangeLimitedReadStream(UniquePtr<IReadStream> &outStream, UniquePtr<ISeekableReadStream> &&stream, FilePos_t startPos, FilePos_t size) const override;
	};

	typedef DriverModuleStub<UtilitiesDriver, IUtilitiesDriver, &Drivers::m_utilitiesDriver> UtilitiesModule;

	Result UtilitiesDriver::InitDriver()
	{
		return ResultCode::kOK;
	}

	void UtilitiesDriver::ShutdownDriver()
	{
	}

	Result UtilitiesDriver::CreateJsonDocument(UniquePtr<Utilities::IJsonDocument> &outDocument, IMallocDriver *alloc, IReadStream *readStream) const
	{
		return Utilities::CreateJsonDocument(outDocument, alloc, readStream);
	}

	Result UtilitiesDriver::CreateMutexProtectedReadWriteStream(SharedPtr<IMutexProtectedReadWriteStream> &outStream, UniquePtr<ISeekableReadWriteStream> &&stream) const
	{
		ISeekableStream *seek = stream.Get();
		IReadStream *read = stream.Get();
		IWriteStream *write = stream.Get();

		UniquePtr<MutexProtectedStreamWrapper> mpsWrapper;
		RKIT_CHECK(New<MutexProtectedStreamWrapper>(mpsWrapper, std::move(stream), seek, read, write));

		SharedPtr<MutexProtectedStreamWrapper> sharedWrapper;
		RKIT_CHECK(MakeShared(sharedWrapper, std::move(mpsWrapper)));

		sharedWrapper->SetTracker(sharedWrapper.GetTracker());

		outStream = SharedPtr<IMutexProtectedReadWriteStream>(std::move(sharedWrapper));

		return ResultCode::kOK;
	}

	Result UtilitiesDriver::CreateMutexProtectedReadStream(SharedPtr<IMutexProtectedReadStream> &outStream, UniquePtr<ISeekableReadStream> &&stream) const
	{
		ISeekableStream *seek = stream.Get();
		IReadStream *read = stream.Get();
		IWriteStream *write = nullptr;

		UniquePtr<MutexProtectedStreamWrapper> mpsWrapper;
		RKIT_CHECK(New<MutexProtectedStreamWrapper>(mpsWrapper, std::move(stream), seek, read, write));

		SharedPtr<MutexProtectedStreamWrapper> sharedWrapper;
		RKIT_CHECK(MakeShared(sharedWrapper, std::move(mpsWrapper)));

		sharedWrapper->SetTracker(sharedWrapper.GetTracker());

		outStream = SharedPtr<IMutexProtectedReadStream>(std::move(sharedWrapper));

		return ResultCode::kOK;
	}

	Result UtilitiesDriver::CreateMutexProtectedWriteStream(SharedPtr<IMutexProtectedWriteStream> &outStream, UniquePtr<ISeekableWriteStream> &&stream) const
	{
		ISeekableStream *seek = stream.Get();
		IReadStream *read = nullptr;
		IWriteStream *write = stream.Get();

		UniquePtr<MutexProtectedStreamWrapper> mpsWrapper;
		RKIT_CHECK(New<MutexProtectedStreamWrapper>(mpsWrapper, std::move(stream), seek, read, write));

		SharedPtr<MutexProtectedStreamWrapper> sharedWrapper;
		RKIT_CHECK(MakeShared(sharedWrapper, std::move(mpsWrapper)));

		sharedWrapper->SetTracker(sharedWrapper.GetTracker());

		outStream = SharedPtr<IMutexProtectedWriteStream>(std::move(sharedWrapper));

		return ResultCode::kOK;
	}

	Result UtilitiesDriver::CreateDeflateDecompressStream(UniquePtr<IReadStream> &outStream, UniquePtr<IReadStream> &&compressedStream) const
	{
		IMallocDriver *alloc = GetDrivers().m_mallocDriver;

		UniquePtr<IReadStream> streamMoved(std::move(compressedStream));

		UniquePtr<DeflateDecompressStream> createdStream;
		RKIT_CHECK(NewWithAlloc<DeflateDecompressStream>(createdStream, alloc, std::move(streamMoved), alloc));

		outStream = UniquePtr<IReadStream>(std::move(createdStream));

		return ResultCode::kOK;
	}

	Result UtilitiesDriver::CreateRangeLimitedReadStream(UniquePtr<IReadStream> &outStream, UniquePtr<ISeekableReadStream> &&streamSrc, FilePos_t startPos, FilePos_t size) const
	{
		IMallocDriver *alloc = GetDrivers().m_mallocDriver;

		UniquePtr<ISeekableReadStream> stream(std::move(streamSrc));

		return NewWithAlloc<RangeLimitedReadStream>(outStream, alloc, std::move(stream), startPos, size);
	}

}


RKIT_IMPLEMENT_MODULE("RKit", "Utilities", ::rkit::UtilitiesModule)
