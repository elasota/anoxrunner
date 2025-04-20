#pragma once

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct IMallocDriver;
	struct IReadStream;

	namespace utils
	{
		struct IJsonDocument;

		extern Result CreateJsonDocument(UniquePtr<IJsonDocument> &outDocument, IMallocDriver *alloc, IReadStream *readStream);
	}
}
