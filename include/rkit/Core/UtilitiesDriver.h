#pragma once

#include "rkit/Core/StreamProtos.h"
#include "rkit/Core/HashValue.h"

#include "rkit/Utilities/TextParserProtos.h"

#include <stdarg.h>

namespace rkit
{
	namespace utils
	{
		struct IJsonDocument;
		struct ITextParser;
		struct ISha256Calculator;
	}

	template<class T>
	class UniquePtr;

	template<class T>
	class Vector;

	template<class T>
	class SharedPtr;

	struct IMutexProtectedReadWriteStream;
	struct IMutexProtectedReadStream;
	struct IMutexProtectedWriteStream;

	struct IMallocDriver;
	struct IReadStream;
	struct ISeekableReadWriteStream;
	struct ISeekableReadStream;
	struct ISeekableWriteStream;

	struct Result;

	struct IUtilitiesDriver
	{
		typedef Result(*AllocateDynamicStringCallback_t)(void *userdata, size_t numChars, void *&outBuffer);

		virtual ~IUtilitiesDriver() {}

		virtual Result CreateJsonDocument(UniquePtr<utils::IJsonDocument> &outDocument, IMallocDriver *alloc, IReadStream *readStream) const = 0;

		virtual Result CreateMutexProtectedReadWriteStream(SharedPtr<IMutexProtectedReadWriteStream> &outStream, UniquePtr<ISeekableReadWriteStream> &&stream) const = 0;
		virtual Result CreateMutexProtectedReadStream(SharedPtr<IMutexProtectedReadStream> &outStream, UniquePtr<ISeekableReadStream> &&stream) const = 0;
		virtual Result CreateMutexProtectedWriteStream(SharedPtr<IMutexProtectedWriteStream> &outStream, UniquePtr<ISeekableWriteStream> &&stream) const = 0;

		virtual Result CreateDeflateDecompressStream(UniquePtr<IReadStream> &outStream, UniquePtr<IReadStream> &&compressedStream) const = 0;
		virtual Result CreateRangeLimitedReadStream(UniquePtr<IReadStream> &outStream, UniquePtr<ISeekableReadStream> &&stream, FilePos_t startPos, FilePos_t size) const = 0;

		virtual HashValue_t ComputeHash(HashValue_t baseHash, const void *value, size_t size) const = 0;

		virtual Result CreateTextParser(const Span<const char> &contents, utils::TextParserCommentType commentType, utils::TextParserLexerType lexType, UniquePtr<utils::ITextParser> &outParser) const = 0;
		virtual Result ReadEntireFile(ISeekableReadStream &stream, Vector<uint8_t> &outBytes) const = 0;

		virtual bool ValidateFilePath(const Span<const char> &fileName) const = 0;

		virtual void NormalizeFilePath(const Span<char> &chars) const = 0;
		virtual bool FindFilePathExtension(const StringView &str, StringView &outExt) const = 0;

		virtual Result EscapeCStringInPlace(const Span<char> &chars, size_t &outNewLength) const = 0;

		virtual const utils::ISha256Calculator *GetSha256Calculator() const = 0;

		virtual Result VFormatString(char *buffer, size_t bufferSize, void *oversizedUserdata, AllocateDynamicStringCallback_t oversizedCallback, size_t &outLength, const char *fmt, va_list list) const = 0;
	};
}
