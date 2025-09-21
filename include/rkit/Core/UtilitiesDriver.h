#pragma once

#include "rkit/Core/StringProto.h"
#include "rkit/Core/StreamProtos.h"
#include "rkit/Core/HashValue.h"
#include "rkit/Core/FormatProtos.h"

#include "rkit/Utilities/TextParserProtos.h"

#include <stdarg.h>

namespace rkit
{
	namespace coro
	{
		class Thread;
	}

	namespace utils
	{
		struct IJsonDocument;
		struct ITextParser;
		struct ISha256Calculator;
		struct IThreadPool;
		struct IShadowFile;
		struct IImage;
		struct ImageSpec;
	}

	template<class TChar>
	struct IFormatStringWriter;

	template<class T>
	class UniquePtr;

	template<class T>
	class Vector;

	template<class T>
	class SharedPtr;

	template<class T>
	class Span;

	struct IMutexProtectedReadWriteStream;
	struct IMutexProtectedReadStream;
	struct IMutexProtectedWriteStream;

	struct IMallocDriver;
	struct IReadStream;
	struct ISeekableReadWriteStream;
	struct ISeekableReadStream;
	struct ISeekableWriteStream;
	struct IAsyncReadFile;
	struct IJobQueue;

	struct IUtilitiesDriver
	{
		typedef Result(*AllocateDynamicStringCallback_t)(void *userdata, size_t numChars, void *&outBuffer);

		virtual ~IUtilitiesDriver() {}

		virtual Result CreateJsonDocument(UniquePtr<utils::IJsonDocument> &outDocument, IMallocDriver *alloc, IReadStream *readStream) const = 0;

		virtual Result CreateJobQueue(UniquePtr<IJobQueue> &outJobQueue, IMallocDriver *alloc) const = 0;

		virtual Result CreateMutexProtectedReadWriteStream(SharedPtr<IMutexProtectedReadWriteStream> &outStream, UniquePtr<ISeekableReadWriteStream> &&stream) const = 0;
		virtual Result CreateMutexProtectedReadStream(SharedPtr<IMutexProtectedReadStream> &outStream, UniquePtr<ISeekableReadStream> &&stream) const = 0;
		virtual Result CreateMutexProtectedWriteStream(SharedPtr<IMutexProtectedWriteStream> &outStream, UniquePtr<ISeekableWriteStream> &&stream) const = 0;

		virtual Result OpenShadowFileRead(UniquePtr<utils::IShadowFile> &outShadowFile, ISeekableReadStream &stream) const = 0;
		virtual Result OpenShadowFileReadWrite(UniquePtr<utils::IShadowFile> &outShadowFile, ISeekableReadWriteStream &stream) const = 0;
		virtual Result InitializeShadowFile(UniquePtr<utils::IShadowFile> &outShadowFile, ISeekableReadWriteStream &stream) const = 0;

		virtual Result CreateRestartableDeflateDecompressStream(UniquePtr<ISeekableReadStream> &outStream, UniquePtr<ISeekableReadStream> &&compressedStream, FilePos_t decompressedSize) const = 0;
		virtual Result CreateDeflateDecompressStream(UniquePtr<IReadStream> &outStream, UniquePtr<IReadStream> &&compressedStream) const = 0;
		virtual Result CreateRangeLimitedReadStream(UniquePtr<ISeekableReadStream> &outStream, UniquePtr<ISeekableReadStream> &&stream, FilePos_t startPos, FilePos_t size) const = 0;

		virtual Result CreateThreadPool(UniquePtr<utils::IThreadPool> &outThreadPool, uint32_t numThreads) const = 0;

		virtual HashValue_t ComputeHash(HashValue_t baseHash, const void *value, size_t size) const = 0;

		virtual Result CreateTextParser(const Span<const char> &contents, utils::TextParserCommentType commentType, utils::TextParserLexerType lexType, UniquePtr<utils::ITextParser> &outParser) const = 0;
		virtual Result ReadEntireFile(ISeekableReadStream &stream, Vector<uint8_t> &outBytes) const = 0;

		virtual bool ValidateFilePath(const Span<const char> &fileName, bool permitWildcards) const = 0;

		virtual void NormalizeFilePath(const Span<char> &chars) const = 0;
		virtual bool FindFilePathExtension(const StringSliceView &str, StringSliceView &outExt) const = 0;

		virtual Result EscapeCStringInPlace(const Span<char> &chars, size_t &outNewLength) const = 0;

		virtual const utils::ISha256Calculator *GetSha256Calculator() const = 0;

		virtual Result SetProgramName(const StringView &str) = 0;
		virtual StringView GetProgramName() const = 0;

		virtual void SetProgramVersion(uint32_t major, uint32_t minor, uint32_t patch) = 0;
		virtual void GetProgramVersion(uint32_t &outMajor, uint32_t &outMinor, uint32_t &outPatch) const = 0;

		virtual void GetRKitVersion(uint32_t &outMajor, uint32_t &outMinor, uint32_t &outPatch) const = 0;

		virtual Span<const uint8_t> GetLinearToSRGBTable() const = 0;
		virtual Span<const uint16_t> GetSRGBToLinearTable() const = 0;
		virtual int GetSRGBToLinearPrecisionBits() const = 0;

		virtual bool ContainsWildcards(const StringSliceView &str) const = 0;
		virtual bool MatchesWildcard(const StringSliceView &candidate, const StringSliceView &wildcard) const = 0;

		virtual bool DefaultIsPathComponentValid(const BaseStringSliceView<char, CharacterEncoding::kUTF8> &span, bool isFirst, bool allowWildcards) const = 0;

		virtual Result ConvertUTF16ToUTF8(size_t &outSize, const Span<uint8_t> &dest, const Span<const uint16_t> &src) const = 0;
		virtual Result ConvertUTF16WCharToUTF8(size_t &outSize, const Span<uint8_t> &dest, const Span<const wchar_t> &src) const = 0;

		virtual Result ConvertUTF8ToUTF16(size_t &outSize, const Span<uint16_t> &dest, const Span<const uint8_t> &src) const = 0;
		virtual Result ConvertUTF8ToUTF16WChar(size_t &outSize, const Span<wchar_t> &dest, const Span<const uint8_t> &src) const = 0;

		virtual bool IsPathComponentValidOnWindows(const BaseStringSliceView<wchar_t, CharacterEncoding::kUTF16> &span, bool isAbsolute, bool isFirst, bool allowWildcards) const = 0;

		virtual Result CreateCoroThread(UniquePtr<coro::Thread> &thread, size_t stackSize) const = 0;

		virtual Result CreateBlockingReader(UniquePtr<ISeekableReadStream> &outReadStream, UniquePtr<IAsyncReadFile> &&asyncFile, FilePos_t fileSize) const = 0;

		virtual bool ParseDouble(const StringView &str, double &d) const = 0;

		virtual bool ParseInt32(const StringSliceView &str, uint8_t radix, int32_t &i) const = 0;
		virtual bool ParseInt64(const StringSliceView &str, uint8_t radix, int64_t &i) const = 0;
		virtual bool ParseUInt32(const StringSliceView &str, uint8_t radix, uint32_t &i) const = 0;
		virtual bool ParseUInt64(const StringSliceView &str, uint8_t radix, uint64_t &i) const = 0;

		virtual Result CreateImage(const utils::ImageSpec &spec, UniquePtr<utils::IImage> &image) const = 0;
		virtual Result CloneImage(UniquePtr<utils::IImage> &outImage, const utils::IImage &image) const = 0;
		virtual Result BlitImageSigned(utils::IImage &destImage, const utils::IImage &srcImage, ptrdiff_t srcX, ptrdiff_t srcY, ptrdiff_t destX, ptrdiff_t destY, size_t width, size_t height) const = 0;
		virtual Result BlitImage(utils::IImage &destImage, const utils::IImage &srcImage, size_t srcX, size_t srcY, size_t destX, size_t destY, size_t width, size_t height) const = 0;

		virtual void FormatSignedInt(IFormatStringWriter<char> &writer, intmax_t value) const = 0;
		virtual void FormatUnsignedInt(IFormatStringWriter<char> &writer, uintmax_t value) const = 0;
		virtual void FormatFloat(IFormatStringWriter<char> &writer, float f) const = 0;
		virtual void FormatDouble(IFormatStringWriter<char> &writer, double f) const = 0;
		virtual void FormatCString(IFormatStringWriter<char> &writer, const char *str) const = 0;
		virtual void WFormatCString(IFormatStringWriter<wchar_t> &writer, const wchar_t *str) const = 0;

		virtual void FormatString(IFormatStringWriter<char> &writer, const StringSliceView &fmt, const FormatParameterList<char> &paramList) const = 0;
		virtual void FormatString(IFormatStringWriter<wchar_t> &writer, const WStringSliceView &fmt, const FormatParameterList<wchar_t> &paramList) const = 0;
	};
}
