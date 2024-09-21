#pragma once

#include "rkit/Core/UniquePtr.h"

namespace rkit
{
	namespace Utilities
	{
		struct IJsonDocument;
	}

	template<class T>
	class UniquePtr;

	struct IReadStream;
	struct Result;

	struct IUtilitiesDriver
	{
		virtual ~IUtilitiesDriver() {}

		virtual Result CreateJsonDocument(UniquePtr<Utilities::IJsonDocument> &outDocument, IMallocDriver *alloc, IReadStream *readStream) const = 0;
	};
}
