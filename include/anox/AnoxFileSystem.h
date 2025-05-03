#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/PathProto.h"
#include "rkit/Core/StreamProtos.h"

namespace rkit
{
	struct ISeekableReadStream;
	struct IAsyncReadFile;

	template<class T>
	class UniquePtr;

	template<class T>
	class Future;

	template<class T>
	class RCPtr;

	class Job;
	struct IJobQueue;
}

namespace anox
{
	struct IGameDataFileSystem
	{
		virtual ~IGameDataFileSystem() {}

		virtual rkit::Result OpenNamedFileBlocking(rkit::RCPtr<rkit::Job> &openJob, const rkit::Future<rkit::UniquePtr<rkit::ISeekableReadStream>> &outStream, const rkit::CIPathView &path) = 0;
		virtual rkit::Result OpenNamedFileAsync(rkit::RCPtr<rkit::Job> &openJob, const rkit::Future<rkit::AsyncFileOpenReadResult> &outStream, const rkit::CIPathView &path) = 0;
	};
}
