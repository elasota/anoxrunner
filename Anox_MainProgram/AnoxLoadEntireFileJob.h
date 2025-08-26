#pragma once

#include "rkit/Core/PathProto.h"
#include "rkit/Core/Result.h"

namespace rkit
{
	struct IJobQueue;
	class Job;

	template<class T>
	class RCPtr;

	template<class T>
	class Vector;
}

namespace anox
{
	class AnoxGameFileSystemBase;

	// Posts IO tasks to open and load a file into a ref-counted vector, outputs
	// the completion job
	rkit::Result CreateLoadEntireFileJob(rkit::RCPtr<rkit::Job> &outJob, const rkit::RCPtr<rkit::Vector<uint8_t>> &blob,
		AnoxGameFileSystemBase &fileSystem, const rkit::CIPathView &path);
}
