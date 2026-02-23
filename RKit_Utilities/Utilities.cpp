#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/Format.h"
#include "rkit/Core/UtilitiesDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Platform.h"
#include "rkit/Core/String.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/Unicode.h"
#include "rkit/Core/Vector.h"

#include "rkit/Core/CoreLib.h"

#include "rkit/Utilities/ThreadPool.h"

#include "Coro2Thread.h"
#include "DeflateDecompressStream.h"
#include "JobQueue.h"
#include "Json.h"
#include "Image.h"
#include "MutexProtectedStream.h"
#include "RangeLimitedReadStream.h"
#include "Sha2Calculator.h"
#include "TextParser.h"
#include "ThreadPool.h"

#include "ryu/ryu.h"

#ifdef RKIT_PLATFORM_ARCH_HAVE_SSE2
#include <emmintrin.h>
#endif

#ifdef RKIT_PLATFORM_ARCH_HAVE_SSE41
#include <smmintrin.h>
#endif

namespace rkit
{
	namespace utils
	{
		struct IJsonDocument;
	}

	class UtilitiesDriver final : public IUtilitiesDriver
	{
	public:
		UtilitiesDriver();

		Result InitDriver(const DriverInitParameters *);
		void ShutdownDriver();

		Result CreateJsonDocument(UniquePtr<utils::IJsonDocument> &outDocument, IMallocDriver *alloc, IReadStream *readStream) const override;
		Result CreateJobQueue(UniquePtr<IJobQueue> &outJobQueue, IMallocDriver *alloc) const override;

		Result CreateMutexProtectedReadWriteStream(SharedPtr<IMutexProtectedReadWriteStream> &outStream, UniquePtr<ISeekableReadWriteStream> &&stream) const override;
		Result CreateMutexProtectedReadStream(SharedPtr<IMutexProtectedReadStream> &outStream, UniquePtr<ISeekableReadStream> &&stream) const override;
		Result CreateMutexProtectedWriteStream(SharedPtr<IMutexProtectedWriteStream> &outStream, UniquePtr<ISeekableWriteStream> &&stream) const override;

		Result CreateRestartableDeflateDecompressStream(UniquePtr<ISeekableReadStream> &outStream, UniquePtr<ISeekableReadStream> &&compressedStream, FilePos_t decompressedSize) const override;
		Result CreateDeflateDecompressStream(UniquePtr<IReadStream> &outStream, UniquePtr<IReadStream> &&compressedStream) const override;
		Result CreateRangeLimitedReadStream(UniquePtr<ISeekableReadStream> &outStream, UniquePtr<ISeekableReadStream> &&stream, FilePos_t startPos, FilePos_t size) const override;

		Result CreateThreadPool(UniquePtr<utils::IThreadPool> &outThreadPool, uint32_t numThreads) const override;

		HashValue_t ComputeHash(HashValue_t baseHash, const void *data, size_t size) const override;

		Result CreateTextParser(const Span<const uint8_t> &contents, utils::TextParserCommentType commentType, utils::TextParserLexerType lexType, UniquePtr<utils::ITextParser> &outParser) const override;
		Result ReadEntireFile(ISeekableReadStream &stream, Vector<uint8_t> &outBytes) const override;

		bool ValidateFilePath(const Span<const Utf8Char_t> &fileName, bool permitWildcards) const override;

		void NormalizeFilePath(const Span<Utf8Char_t> &chars) const override;
		bool FindFilePathExtension(const StringSliceView &str, StringSliceView &outExt) const override;

		Result EscapeCStringInPlace(const Span<Utf8Char_t> &chars, size_t &outNewLength) const override;

		const utils::ISha256Calculator *GetSha256Calculator() const override;

		Result SetProgramName(const StringView &str) override;
		StringView GetProgramName() const override;

		void SetProgramVersion(uint32_t major, uint32_t minor, uint32_t patch) override;
		void GetProgramVersion(uint32_t &outMajor, uint32_t &outMinor, uint32_t &outPatch) const override;

		void GetRKitVersion(uint32_t &outMajor, uint32_t &outMinor, uint32_t &outPatch) const override;

		ConstSpan<uint8_t> GetLinearToSRGBTable() const override;
		ConstSpan<uint16_t> GetSRGBToLinearTable() const override;
		int GetSRGBToLinearPrecisionBits() const override;

		bool ContainsWildcards(const StringSliceView &str) const override;
		bool MatchesWildcard(const StringSliceView &candidate, const StringSliceView &wildcard) const override;

		bool DefaultIsPathComponentValid(const StringSliceView &span, bool isFirst, bool allowWildcards) const override;

		Result ConvertUTF16ToUTF8(size_t &outSize, const Span<uint8_t> &dest, const Span<const uint16_t> &src) const override;
		Result ConvertUTF16WCharToUTF8(size_t &outSize, const Span<uint8_t> &dest, const Span<const wchar_t> &src) const override;

		Result ConvertUTF8ToUTF16(size_t &outSize, const Span<uint16_t> &dest, const Span<const uint8_t> &src) const override;
		Result ConvertUTF8ToUTF16WChar(size_t &outSize, const Span<wchar_t> &dest, const Span<const uint8_t> &src) const override;

		bool IsPathComponentValidOnWindows(const BaseStringSliceView<OSPathChar_t, CharacterEncoding::kOSPath> &span, bool isAbsolute, bool isFirst, bool allowWildcards) const override;

		Result CreateCoro2Thread(UniquePtr<ICoroThread> &thread, size_t stackSize) const override;

		bool ParseDouble(const ByteStringSliceView &str, double &d) const override;
		bool ParseFloat(const ByteStringSliceView &str, float &f) const override;

		bool ParseInt32(const ByteStringSliceView &str, uint8_t radix, int32_t &i) const override;
		bool ParseInt64(const ByteStringSliceView &str, uint8_t radix, int64_t &i) const override;
		bool ParseUInt32(const ByteStringSliceView &str, uint8_t radix, uint32_t &i) const override;
		bool ParseUInt64(const ByteStringSliceView &str, uint8_t radix, uint64_t &i) const override;

		Result CreateImage(const utils::ImageSpec &spec, UniquePtr<utils::IImage> &image) const override;
		Result CloneImage(UniquePtr<utils::IImage> &outImage, const utils::IImage &image) const override;
		Result BlitImageSigned(utils::IImage &destImage, const utils::IImage &srcImage, ptrdiff_t srcX, ptrdiff_t srcY, ptrdiff_t destX, ptrdiff_t destY, size_t width, size_t height) const override;
		Result BlitImage(utils::IImage &destImage, const utils::IImage &srcImage, size_t srcX, size_t srcY, size_t destX, size_t destY, size_t width, size_t height) const override;

		void FormatSignedInt(IFormatStringWriter<Utf8Char_t> &writer, intmax_t value) const override;
		void FormatUnsignedInt(IFormatStringWriter<Utf8Char_t> &writer, uintmax_t value) const override;
		void FormatFloat(IFormatStringWriter<Utf8Char_t> &writer, float f) const override;
		void FormatDouble(IFormatStringWriter<Utf8Char_t> &writer, double f) const override;
		void FormatUtf8String(IFormatStringWriter<Utf8Char_t> &writer, const Utf8Char_t *str) const override;
		void FormatCString(IFormatStringWriter<char> &writer, const char *str) const override;

		void FormatString(IFormatStringWriter<Utf8Char_t> &writer, const StringSliceView &fmt, const FormatParameterList<Utf8Char_t> &paramList) const override;

		void SanitizeClampFloats(const Span<float> &outFloats, const Span<const endian::LittleFloat32_t> &inFloats, int maxMagnitude) const override;
		void SanitizeClampUInt16s(const Span<uint16_t> &outFloats, const Span<const endian::LittleUInt16_t> &inFloats, uint16_t maxValue) const override;

	private:
		static bool ValidateFilePathSlice(const Span<const Utf8Char_t> &name, bool permitWildcards);

		static bool MatchesWildcardWithMinLiteralCount(const StringSliceView &candidate, const StringSliceView &wildcard, size_t minLiteralCount);

		template<class TWChar>
		static Result TypedConvertUTF16ToUTF8(size_t &outSize, const Span<uint8_t> &dest, const Span<const TWChar> &src);

		template<class TWChar>
		static Result TypedConvertUTF8ToUTF16(size_t &outSize, const Span<TWChar> &dest, const Span<const uint8_t> &src);

		static bool TryParseDigit(uint8_t c, uint8_t &outDigit);

		template<class TInteger>
		static bool ParsePositiveInt(const ByteStringSliceView &str, size_t startDigit, uint8_t radix, TInteger &i);

		template<class TInteger>
		static bool ParseNegativeInt(const ByteStringSliceView &str, size_t startDigit, uint8_t radix, TInteger &i);

		template<class TInteger>
		static bool ParseSignedInt(const ByteStringSliceView &str, uint8_t radix, TInteger &i);

		template<class TChar, CharacterEncoding TEncoding>
		static void FormatStringImpl(IFormatStringWriter<TChar> &writer, const BaseStringSliceView<TChar, TEncoding> &fmtRef, const FormatParameterList<TChar> &paramListRef);

		utils::Sha256Calculator m_sha256Calculator;

		String m_programName;

		uint32_t m_programVersion[3];

		static const int ms_linearToSRGBPrecisionBits = 14;
		static const uint16_t ms_srgbToLinearTable[256];
		static const uint8_t ms_linearToSRGBTable[1 << ms_linearToSRGBPrecisionBits];
	};

	typedef DriverModuleStub<UtilitiesDriver, IUtilitiesDriver, &Drivers::m_utilitiesDriver> UtilitiesModule;

	UtilitiesDriver::UtilitiesDriver()
		: m_programVersion{ 1, 0, 0 }
	{
	}

	Result UtilitiesDriver::InitDriver(const DriverInitParameters *)
	{
		RKIT_RETURN_OK;
	}

	void UtilitiesDriver::ShutdownDriver()
	{
	}

	Result UtilitiesDriver::CreateJsonDocument(UniquePtr<utils::IJsonDocument> &outDocument, IMallocDriver *alloc, IReadStream *readStream) const
	{
		return utils::CreateJsonDocument(outDocument, alloc, readStream);
	}

	Result UtilitiesDriver::CreateJobQueue(UniquePtr<IJobQueue> &outJobQueue, IMallocDriver *alloc) const
	{
		return utils::CreateJobQueue(outJobQueue, alloc);
	}

	Result UtilitiesDriver::CreateMutexProtectedReadWriteStream(SharedPtr<IMutexProtectedReadWriteStream> &outStream, UniquePtr<ISeekableReadWriteStream> &&stream) const
	{
		ISeekableStream *seek = stream.Get();
		IReadStream *read = stream.Get();
		IWriteStream *write = stream.Get();
		ISeekableWriteStream *seekableWrite = stream.Get();

		UniquePtr<IMutex> mutex;
		RKIT_CHECK(GetDrivers().m_systemDriver->CreateMutex(mutex));

		UniquePtr<MutexProtectedStreamWrapper> mpsWrapper;
		RKIT_CHECK(New<MutexProtectedStreamWrapper>(mpsWrapper, std::move(stream), std::move(mutex), seek, read, write, seekableWrite));

		SharedPtr<MutexProtectedStreamWrapper> sharedWrapper;
		RKIT_CHECK(MakeShared(sharedWrapper, std::move(mpsWrapper)));

		sharedWrapper->SetTracker(sharedWrapper.GetTracker());

		outStream = SharedPtr<IMutexProtectedReadWriteStream>(std::move(sharedWrapper));

		RKIT_RETURN_OK;
	}

	Result UtilitiesDriver::CreateMutexProtectedReadStream(SharedPtr<IMutexProtectedReadStream> &outStream, UniquePtr<ISeekableReadStream> &&stream) const
	{
		ISeekableStream *seek = stream.Get();
		IReadStream *read = stream.Get();
		IWriteStream *write = nullptr;
		ISeekableWriteStream *seekableWrite = nullptr;

		UniquePtr<IMutex> mutex;
		RKIT_CHECK(GetDrivers().m_systemDriver->CreateMutex(mutex));

		UniquePtr<MutexProtectedStreamWrapper> mpsWrapper;
		RKIT_CHECK(New<MutexProtectedStreamWrapper>(mpsWrapper, std::move(stream), std::move(mutex), seek, read, write, seekableWrite));

		SharedPtr<MutexProtectedStreamWrapper> sharedWrapper;
		RKIT_CHECK(MakeShared(sharedWrapper, std::move(mpsWrapper)));

		sharedWrapper->SetTracker(sharedWrapper.GetTracker());

		outStream = SharedPtr<IMutexProtectedReadStream>(std::move(sharedWrapper));

		RKIT_RETURN_OK;
	}

	Result UtilitiesDriver::CreateMutexProtectedWriteStream(SharedPtr<IMutexProtectedWriteStream> &outStream, UniquePtr<ISeekableWriteStream> &&stream) const
	{
		ISeekableStream *seek = stream.Get();
		IReadStream *read = nullptr;
		IWriteStream *write = stream.Get();
		ISeekableWriteStream *seekableWrite = stream.Get();

		UniquePtr<IMutex> mutex;
		RKIT_CHECK(GetDrivers().m_systemDriver->CreateMutex(mutex));

		UniquePtr<MutexProtectedStreamWrapper> mpsWrapper;
		RKIT_CHECK(New<MutexProtectedStreamWrapper>(mpsWrapper, std::move(stream), std::move(mutex), seek, read, write, seekableWrite));

		SharedPtr<MutexProtectedStreamWrapper> sharedWrapper;
		RKIT_CHECK(MakeShared(sharedWrapper, std::move(mpsWrapper)));

		sharedWrapper->SetTracker(sharedWrapper.GetTracker());

		outStream = SharedPtr<IMutexProtectedWriteStream>(std::move(sharedWrapper));

		RKIT_RETURN_OK;
	}

	Result UtilitiesDriver::CreateRestartableDeflateDecompressStream(UniquePtr<ISeekableReadStream> &outStream, UniquePtr<ISeekableReadStream> &&compressedStream, FilePos_t decompressedSize) const
	{
		IMallocDriver *alloc = GetDrivers().m_mallocDriver;

		ISeekableStream *seekable = compressedStream.Get();
		UniquePtr<IReadStream> streamMoved(std::move(compressedStream));

		UniquePtr<DeflateDecompressStream> createdStream;
		RKIT_CHECK(NewWithAlloc<DeflateDecompressStream>(createdStream, alloc, std::move(streamMoved), seekable, decompressedSize, alloc));

		outStream = std::move(createdStream);

		RKIT_RETURN_OK;
	}

	Result UtilitiesDriver::CreateDeflateDecompressStream(UniquePtr<IReadStream> &outStream, UniquePtr<IReadStream> &&compressedStream) const
	{
		IMallocDriver *alloc = GetDrivers().m_mallocDriver;

		UniquePtr<IReadStream> streamMoved(std::move(compressedStream));

		UniquePtr<DeflateDecompressStream> createdStream;
		RKIT_CHECK(NewWithAlloc<DeflateDecompressStream>(createdStream, alloc, std::move(streamMoved), nullptr, rkit::Optional<FilePos_t>(), alloc));

		outStream = UniquePtr<IReadStream>(std::move(createdStream));

		RKIT_RETURN_OK;
	}

	Result UtilitiesDriver::CreateRangeLimitedReadStream(UniquePtr<ISeekableReadStream> &outStream, UniquePtr<ISeekableReadStream> &&streamSrc, FilePos_t startPos, FilePos_t size) const
	{
		IMallocDriver *alloc = GetDrivers().m_mallocDriver;

		UniquePtr<ISeekableReadStream> stream(std::move(streamSrc));

		return NewWithAlloc<RangeLimitedReadStream>(outStream, alloc, std::move(stream), startPos, size);
	}

	Result UtilitiesDriver::CreateThreadPool(UniquePtr<utils::IThreadPool> &outThreadPool, uint32_t numThreads) const
	{
		UniquePtr<utils::ThreadPoolBase> threadPool;
		RKIT_CHECK(utils::ThreadPoolBase::Create(threadPool, *this, numThreads));

		outThreadPool = std::move(threadPool);

		RKIT_RETURN_OK;
	}

	HashValue_t UtilitiesDriver::ComputeHash(HashValue_t baseHash, const void *value, size_t size) const
	{
		const uint8_t *bytes = static_cast<const uint8_t *>(value);

		// TODO: Improve this
		HashValue_t hash = baseHash;
		for (size_t i = 0; i < size; i++)
			hash = hash * 223u + bytes[i] * 4447u;

		return hash;
	}

	Result UtilitiesDriver::CreateTextParser(const Span<const uint8_t> &contents, utils::TextParserCommentType commentType, utils::TextParserLexerType lexType, UniquePtr<utils::ITextParser> &outParser) const
	{
		UniquePtr<utils::TextParserBase> parser;
		RKIT_CHECK(utils::TextParserBase::Create(contents, commentType, lexType, parser));

		outParser = std::move(parser);

		RKIT_RETURN_OK;
	}

	Result UtilitiesDriver::ReadEntireFile(ISeekableReadStream &stream, Vector<uint8_t> &outBytes) const
	{
		FilePos_t fileSizeRemaining = stream.GetSize() - stream.Tell();

		if (fileSizeRemaining == 0)
			outBytes.Reset();
		else
		{
			if (fileSizeRemaining > std::numeric_limits<size_t>::max())
				RKIT_THROW(ResultCode::kOutOfMemory);

			RKIT_CHECK(outBytes.Resize(static_cast<size_t>(fileSizeRemaining)));

			RKIT_CHECK(stream.ReadAll(outBytes.GetBuffer(), static_cast<size_t>(fileSizeRemaining)));
		}

		RKIT_RETURN_OK;
	}

	bool UtilitiesDriver::ValidateFilePathSlice(const rkit::Span<const Utf8Char_t> &sliceName, bool permitWildcards)
	{
		if (sliceName.Count() == 0)
			return false;

		char firstChar = sliceName[0];
		char lastChar = sliceName[sliceName.Count() - 1];

		if (firstChar == ' ' || lastChar == ' ' || lastChar == '.')
			return false;

		const char *bannedNames[] =
		{
			"con",
			"prn",
			"aux",
			"nul",
			"com1",
			"com2",
			"com3",
			"com4",
			"com5",
			"com6",
			"com7",
			"com8",
			"com9",
			"lpt1",
			"lpt2",
			"lpt3",
			"lpt4",
			"lpt5",
			"lpt6",
			"lpt7",
			"lpt8",
			"lpt9",
		};

		for (const char *bannedName : bannedNames)
		{
			size_t bannedNameLength = strlen(bannedName);
			if (sliceName.Count() == bannedNameLength && !memcmp(bannedName, sliceName.Ptr(), bannedNameLength))
				return false;
		}

		for (size_t i = 0; i < sliceName.Count(); i++)
		{
			char c = sliceName[i];
			if (c == '.' && i > 0 && sliceName[i - 1] == '.')
				return false;

			bool isValidChar = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');

			if (!isValidChar)
			{
				const char *extChars = "_-. +~#()";

				for (size_t j = 0; extChars[j] != 0; j++)
				{
					if (permitWildcards && (c == '*' || c == '?'))
					{
						isValidChar = true;
						break;
					}

					if (extChars[j] == c)
					{
						isValidChar = true;
						break;
					}
				}
			}

			if (!isValidChar)
				return false;
		}

		return true;
	}

	bool UtilitiesDriver::ValidateFilePath(const Span<const Utf8Char_t> &name, bool permitWildcards) const
	{
		size_t sliceStart = 0;
		for (size_t i = 0; i < name.Count(); i++)
		{
			if (name[i] == '/')
			{
				if (!ValidateFilePathSlice(name.SubSpan(sliceStart, i - sliceStart), permitWildcards))
					return false;

				sliceStart = i + 1;
			}
		}

		return ValidateFilePathSlice(name.SubSpan(sliceStart, name.Count() - sliceStart), permitWildcards);
	}

	void UtilitiesDriver::NormalizeFilePath(const Span<Utf8Char_t> &chars) const
	{
		for (Utf8Char_t &ch : chars)
		{
			if (ch == '\\')
				ch = '/';
			else if (ch >= 'A' && ch <= 'Z')
				ch = ch - 'A' + 'a';
		}
	}

	bool UtilitiesDriver::FindFilePathExtension(const StringSliceView &str, StringSliceView &outExt) const
	{
		for (size_t ri = 0; ri < str.Length(); ri++)
		{
			size_t i = str.Length() - 1 - ri;

			if (str[i] == '.')
			{
				outExt = StringSliceView(str.GetChars() + i + 1, ri);
				return true;
			}
		}

		return false;
	}

	Result UtilitiesDriver::EscapeCStringInPlace(const Span<Utf8Char_t> &charsRef, size_t &outNewLength) const
	{
		Span<Utf8Char_t> span = charsRef;

		if (span.Count() < 2)
			RKIT_THROW(ResultCode::kInvalidCString);

		size_t outPos = 0;
		size_t inPos = 1;
		size_t endInPos = charsRef.Count() - 1;
		while (inPos < endInPos)
		{
			char c = charsRef[inPos++];
			if (c == '\"')
				break;
			else if (c == '\\')
			{
				if (inPos == endInPos)
					RKIT_THROW(ResultCode::kInvalidCString);

				char escC = charsRef[inPos++];
				if (escC == 't')
					c = '\t';
				else if (escC == 'r')
					c = '\r';
				else if (escC == 'n')
					c = '\n';
				else if (escC == '\"')
					c = '\"';
				else if (escC == '\'')
					c = '\'';
				else
					RKIT_THROW(ResultCode::kInvalidCString);

				span[outPos++] = c;
			}
			else
				span[outPos++] = c;
		}

		outNewLength = outPos;

		RKIT_RETURN_OK;
	}

	const utils::ISha256Calculator *UtilitiesDriver::GetSha256Calculator() const
	{
		return &m_sha256Calculator;
	}

	Result UtilitiesDriver::SetProgramName(const StringView &str)
	{
		return m_programName.Set(str);
	}

	StringView UtilitiesDriver::GetProgramName() const
	{
		return m_programName;
	}

	void UtilitiesDriver::SetProgramVersion(uint32_t major, uint32_t minor, uint32_t patch)
	{
		m_programVersion[0] = major;
		m_programVersion[1] = minor;
		m_programVersion[2] = patch;
	}

	void UtilitiesDriver::GetProgramVersion(uint32_t &outMajor, uint32_t &outMinor, uint32_t &outPatch) const
	{
		outMajor = m_programVersion[0];
		outMinor = m_programVersion[1];
		outPatch = m_programVersion[2];
	}

	void UtilitiesDriver::GetRKitVersion(uint32_t &outMajor, uint32_t &outMinor, uint32_t &outPatch) const
	{
		outMajor = 1;
		outMinor = 0;
		outPatch = 0;
	}

	ConstSpan<uint8_t> UtilitiesDriver::GetLinearToSRGBTable() const
	{
		return ConstSpan<uint8_t>(ms_linearToSRGBTable);
	}

	ConstSpan<uint16_t> UtilitiesDriver::GetSRGBToLinearTable() const
	{
		return ConstSpan<uint16_t>(ms_srgbToLinearTable);
	}

	int UtilitiesDriver::GetSRGBToLinearPrecisionBits() const
	{
		return ms_linearToSRGBPrecisionBits;
	}

	const uint16_t UtilitiesDriver::ms_srgbToLinearTable[256] =
	{
		0, 20, 40, 60, 80, 99, 119, 139,
		159, 179, 199, 219, 241, 264, 288, 313,
		340, 367, 396, 427, 458, 491, 526, 562,
		599, 637, 677, 718, 761, 805, 851, 898,
		947, 997, 1048, 1101, 1156, 1212, 1270, 1330,
		1391, 1453, 1517, 1583, 1651, 1720, 1790, 1863,
		1937, 2013, 2090, 2170, 2250, 2333, 2418, 2504,
		2592, 2681, 2773, 2866, 2961, 3058, 3157, 3258,
		3360, 3464, 3570, 3678, 3788, 3900, 4014, 4129,
		4247, 4366, 4488, 4611, 4736, 4864, 4993, 5124,
		5257, 5392, 5530, 5669, 5810, 5953, 6099, 6246,
		6395, 6547, 6700, 6856, 7014, 7174, 7335, 7500,
		7666, 7834, 8004, 8177, 8352, 8528, 8708, 8889,
		9072, 9258, 9445, 9635, 9828, 10022, 10219, 10417,
		10619, 10822, 11028, 11235, 11446, 11658, 11873, 12090,
		12309, 12530, 12754, 12980, 13209, 13440, 13673, 13909,
		14146, 14387, 14629, 14874, 15122, 15371, 15623, 15878,
		16135, 16394, 16656, 16920, 17187, 17456, 17727, 18001,
		18277, 18556, 18837, 19121, 19407, 19696, 19987, 20281,
		20577, 20876, 21177, 21481, 21787, 22096, 22407, 22721,
		23038, 23357, 23678, 24002, 24329, 24658, 24990, 25325,
		25662, 26001, 26344, 26688, 27036, 27386, 27739, 28094,
		28452, 28813, 29176, 29542, 29911, 30282, 30656, 31033,
		31412, 31794, 32179, 32567, 32957, 33350, 33745, 34143,
		34544, 34948, 35355, 35764, 36176, 36591, 37008, 37429,
		37852, 38278, 38706, 39138, 39572, 40009, 40449, 40891,
		41337, 41785, 42236, 42690, 43147, 43606, 44069, 44534,
		45002, 45473, 45947, 46423, 46903, 47385, 47871, 48359,
		48850, 49344, 49841, 50341, 50844, 51349, 51858, 52369,
		52884, 53401, 53921, 54445, 54971, 55500, 56032, 56567,
		57105, 57646, 58190, 58737, 59287, 59840, 60396, 60955,
		61517, 62082, 62650, 63221, 63795, 64372, 64952, 65535,
	};

	const uint8_t UtilitiesDriver::ms_linearToSRGBTable[16384] =
	{
		0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3,
		3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6,
		6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9,
		10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12,
		13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15,
		15, 15, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 17, 17,
		18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20,
		20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 21,
		22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23,
		23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 25,
		25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 27,
		27, 27, 27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28,
		28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 30,
		30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 31, 31, 31, 31, 31,
		31, 31, 31, 31, 31, 31, 31, 32, 32, 32, 32, 32, 32, 32, 32, 32,
		32, 32, 32, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
		34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 35, 35, 35,
		35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 36, 36, 36, 36, 36,
		36, 36, 36, 36, 36, 36, 36, 36, 36, 37, 37, 37, 37, 37, 37, 37,
		37, 37, 37, 37, 37, 37, 37, 38, 38, 38, 38, 38, 38, 38, 38, 38,
		38, 38, 38, 38, 38, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
		39, 39, 39, 39, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
		40, 40, 40, 40, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
		41, 41, 41, 41, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
		42, 42, 42, 42, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
		43, 43, 43, 43, 43, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44,
		44, 44, 44, 44, 44, 44, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
		45, 45, 45, 45, 45, 45, 45, 46, 46, 46, 46, 46, 46, 46, 46, 46,
		46, 46, 46, 46, 46, 46, 46, 46, 46, 47, 47, 47, 47, 47, 47, 47,
		47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 48, 48, 48, 48, 48,
		48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 49, 49,
		49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
		49, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
		50, 50, 50, 50, 50, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
		51, 51, 51, 51, 51, 51, 51, 51, 51, 52, 52, 52, 52, 52, 52, 52,
		52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 53, 53, 53,
		53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
		53, 53, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
		54, 54, 54, 54, 54, 54, 54, 54, 55, 55, 55, 55, 55, 55, 55, 55,
		55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 56, 56, 56,
		56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56,
		56, 56, 56, 56, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
		57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 58, 58, 58, 58, 58, 58,
		58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59,
		59, 59, 59, 59, 59, 59, 59, 59, 59, 60, 60, 60, 60, 60, 60, 60,
		60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
		60, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61,
		61, 61, 61, 61, 61, 61, 61, 61, 61, 62, 62, 62, 62, 62, 62, 62,
		62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62,
		62, 62, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
		63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 64, 64, 64, 64,
		64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
		64, 64, 64, 64, 64, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
		65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
		66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
		66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 67, 67, 67, 67, 67,
		67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67,
		67, 67, 67, 67, 67, 67, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68,
		68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68,
		68, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69,
		69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 70, 70,
		70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70,
		70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 71, 71, 71, 71, 71, 71,
		71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71,
		71, 71, 71, 71, 71, 71, 71, 72, 72, 72, 72, 72, 72, 72, 72, 72,
		72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
		72, 72, 72, 72, 72, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
		73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73,
		73, 73, 73, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
		74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
		74, 74, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
		75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
		75, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76,
		76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76,
		77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
		77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
		77, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
		78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
		78, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79,
		79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79,
		79, 79, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
		80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
		80, 80, 80, 80, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81,
		81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81,
		81, 81, 81, 81, 81, 81, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82,
		82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82,
		82, 82, 82, 82, 82, 82, 82, 82, 83, 83, 83, 83, 83, 83, 83, 83,
		83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
		83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 84, 84, 84, 84, 84,
		84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84,
		84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 85,
		85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
		85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
		85, 85, 85, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86,
		86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86, 86,
		86, 86, 86, 86, 86, 86, 86, 86, 87, 87, 87, 87, 87, 87, 87, 87,
		87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87,
		87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 88, 88, 88,
		88, 88, 88, 88, 88, 88, 88, 88, 88, 88, 88, 88, 88, 88, 88, 88,
		88, 88, 88, 88, 88, 88, 88, 88, 88, 88, 88, 88, 88, 88, 88, 88,
		88, 88, 89, 89, 89, 89, 89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
		89, 89, 89, 89, 89, 89, 89, 89, 89, 89, 89, 89, 89, 89, 89, 89,
		89, 89, 89, 89, 89, 89, 89, 89, 90, 90, 90, 90, 90, 90, 90, 90,
		90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90,
		90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 91,
		91, 91, 91, 91, 91, 91, 91, 91, 91, 91, 91, 91, 91, 91, 91, 91,
		91, 91, 91, 91, 91, 91, 91, 91, 91, 91, 91, 91, 91, 91, 91, 91,
		91, 91, 91, 91, 91, 91, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
		92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
		92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 93, 93,
		93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
		93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
		93, 93, 93, 93, 93, 93, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94,
		94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94,
		94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 94, 95,
		95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95,
		95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95,
		95, 95, 95, 95, 95, 95, 95, 95, 96, 96, 96, 96, 96, 96, 96, 96,
		96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
		96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96, 96,
		96, 96, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97,
		97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97,
		97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 97, 98, 98, 98, 98,
		98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
		98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98, 98,
		98, 98, 98, 98, 98, 98, 98, 99, 99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
		100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
		100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 101, 101,
		101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101,
		101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101,
		101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 101, 102, 102, 102, 102, 102,
		102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
		102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102, 102,
		102, 102, 102, 102, 102, 102, 102, 102, 103, 103, 103, 103, 103, 103, 103, 103,
		103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103,
		103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103, 103,
		103, 103, 103, 103, 103, 103, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104,
		104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104,
		104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104, 104,
		104, 104, 104, 104, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105,
		105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105,
		105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105,
		105, 105, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106,
		106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106,
		106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106, 106,
		106, 106, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
		107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
		107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
		107, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108,
		108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108,
		108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108, 108,
		108, 108, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
		109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
		109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109, 109,
		109, 109, 109, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110,
		110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110,
		110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110,
		110, 110, 110, 110, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
		111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
		111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
		111, 111, 111, 111, 111, 111, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112,
		112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112,
		112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112, 112,
		112, 112, 112, 112, 112, 112, 112, 112, 112, 113, 113, 113, 113, 113, 113, 113,
		113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
		113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
		113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 113, 114, 114, 114, 114,
		114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114,
		114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114,
		114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 114, 115,
		115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115,
		115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115,
		115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115, 115,
		115, 115, 115, 115, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116,
		116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116,
		116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116, 116,
		116, 116, 116, 116, 116, 116, 116, 116, 117, 117, 117, 117, 117, 117, 117, 117,
		117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
		117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117,
		117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 117, 118, 118,
		118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
		118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
		118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118, 118,
		118, 118, 118, 118, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119,
		119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119,
		119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 119,
		119, 119, 119, 119, 119, 119, 119, 119, 119, 119, 120, 120, 120, 120, 120, 120,
		120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
		120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
		120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120,
		120, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121,
		121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121,
		121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121, 121,
		121, 121, 121, 121, 121, 121, 121, 121, 121, 122, 122, 122, 122, 122, 122, 122,
		122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122,
		122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122,
		122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122,
		122, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123,
		123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123,
		123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 123,
		123, 123, 123, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124,
		124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124,
		124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124,
		124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124, 124,
		124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125,
		125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125,
		125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125,
		125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126,
		126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
		126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
		126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
		126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127,
		127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
		127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
		127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
		127, 127, 127, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
		128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 129,
		129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129,
		129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129,
		129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129,
		129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 129, 130, 130, 130, 130, 130,
		130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130,
		130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130,
		130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130, 130,
		130, 130, 130, 130, 130, 130, 130, 130, 131, 131, 131, 131, 131, 131, 131, 131,
		131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131,
		131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131,
		131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131, 131,
		131, 131, 131, 131, 131, 131, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
		132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
		132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
		132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
		132, 132, 132, 132, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133,
		133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133,
		133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133,
		133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133, 133,
		133, 133, 133, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134,
		134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134,
		134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134,
		134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134, 134,
		134, 134, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135,
		135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135,
		135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135,
		135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135,
		135, 135, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136,
		136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136,
		136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136,
		136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136, 136,
		136, 136, 136, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137,
		137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137,
		137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137,
		137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137, 137,
		137, 137, 137, 137, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
		138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
		138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
		138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138, 138,
		138, 138, 138, 138, 138, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139,
		139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139,
		139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139,
		139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139, 139,
		139, 139, 139, 139, 139, 139, 139, 139, 140, 140, 140, 140, 140, 140, 140, 140,
		140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140,
		140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140,
		140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140,
		140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 140, 141, 141, 141, 141, 141,
		141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141,
		141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141,
		141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141,
		141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 141, 142, 142,
		142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
		142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
		142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
		142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142, 142,
		142, 142, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143,
		143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143,
		143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143,
		143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143, 143,
		143, 143, 143, 143, 143, 143, 143, 144, 144, 144, 144, 144, 144, 144, 144, 144,
		144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
		144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
		144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
		144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 145, 145, 145,
		145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
		145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
		145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
		145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145, 145,
		145, 145, 145, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
		146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
		146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
		146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146, 146,
		146, 146, 146, 146, 146, 146, 146, 146, 146, 147, 147, 147, 147, 147, 147, 147,
		147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
		147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
		147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
		147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147, 147,
		147, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148,
		148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148,
		148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148,
		148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148, 148,
		148, 148, 148, 148, 148, 148, 148, 148, 149, 149, 149, 149, 149, 149, 149, 149,
		149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,
		149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,
		149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,
		149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149, 149,
		149, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
		150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
		150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
		150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150,
		150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 151, 151, 151, 151, 151, 151,
		151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151,
		151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151,
		151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151,
		151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151, 151,
		151, 151, 151, 151, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152,
		152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152,
		152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152,
		152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152,
		152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 152, 153, 153,
		153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153,
		153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153,
		153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153,
		153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153, 153,
		153, 153, 153, 153, 153, 153, 153, 153, 153, 154, 154, 154, 154, 154, 154, 154,
		154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154,
		154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154,
		154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154,
		154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154,
		154, 154, 154, 154, 154, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155,
		155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155,
		155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155,
		155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155,
		155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155, 155,
		155, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
		156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
		156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
		156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156,
		156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 156, 157, 157,
		157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
		157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
		157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
		157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157,
		157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 158,
		158, 158, 158, 158, 158, 158, 158, 158, 158, 158, 159, 159, 159, 159, 159, 159,
		159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159,
		159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159,
		159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159,
		159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159, 159,
		159, 159, 159, 159, 159, 159, 159, 159, 160, 160, 160, 160, 160, 160, 160, 160,
		160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160,
		160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160,
		160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160,
		160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160,
		160, 160, 160, 160, 160, 160, 160, 160, 161, 161, 161, 161, 161, 161, 161, 161,
		161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161,
		161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161,
		161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161,
		161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 161,
		161, 161, 161, 161, 161, 161, 161, 161, 162, 162, 162, 162, 162, 162, 162, 162,
		162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162,
		162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162,
		162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162,
		162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162, 162,
		162, 162, 162, 162, 162, 162, 162, 162, 162, 163, 163, 163, 163, 163, 163, 163,
		163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
		163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
		163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
		163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 163,
		163, 163, 163, 163, 163, 163, 163, 163, 163, 163, 164, 164, 164, 164, 164, 164,
		164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164,
		164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164,
		164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164,
		164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164,
		164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 165, 165, 165, 165,
		165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165,
		165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165,
		165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165,
		165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165,
		165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 165, 166,
		166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166,
		166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166,
		166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166,
		166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166,
		166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166, 166,
		166, 166, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167,
		167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167,
		167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167,
		167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167,
		167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167,
		167, 167, 167, 167, 167, 167, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
		168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
		168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
		168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
		168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168,
		168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 169, 169, 169, 169, 169, 169,
		169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169,
		169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169,
		169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169,
		169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169,
		169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169, 169,
		170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170,
		170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170,
		170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170,
		170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170,
		170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170,
		170, 170, 170, 170, 170, 170, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171,
		171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171,
		171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171,
		171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171,
		171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171,
		171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 171, 172, 172, 172, 172,
		172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
		172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
		172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
		172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
		172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172,
		172, 172, 172, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173,
		173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173,
		173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173,
		173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173,
		173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173,
		173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 174, 174, 174, 174, 174,
		174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
		174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
		174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
		174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
		174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174, 174,
		174, 174, 174, 174, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175,
		175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175,
		175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175,
		175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175,
		175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175,
		175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 175, 176, 176, 176,
		176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176,
		176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176,
		176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176,
		176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176,
		176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176,
		176, 176, 176, 176, 176, 176, 176, 177, 177, 177, 177, 177, 177, 177, 177, 177,
		177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177,
		177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177,
		177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177,
		177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177,
		177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177,
		177, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178,
		178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178,
		178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178,
		178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178,
		178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178,
		178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 178, 179, 179, 179, 179,
		179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179,
		179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179,
		179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179,
		179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179,
		179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179, 179,
		179, 179, 179, 179, 179, 179, 179, 179, 180, 180, 180, 180, 180, 180, 180, 180,
		180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
		180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
		180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
		180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
		180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
		180, 180, 180, 180, 180, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181,
		181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181,
		181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181,
		181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181,
		181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181,
		181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181,
		181, 181, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
		182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
		182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
		182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
		182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
		182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182,
		183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183,
		183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183,
		183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183,
		183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183,
		183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183,
		183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 184, 184,
		184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184,
		184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184,
		184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184,
		184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184,
		184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184,
		184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 184, 185, 185, 185,
		185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185,
		185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185,
		185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185,
		185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185,
		185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185,
		185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 185, 186, 186, 186,
		186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186,
		186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186,
		186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186,
		186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186,
		186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186,
		186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 187, 187,
		187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187,
		187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187,
		187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187,
		187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187,
		187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187,
		187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 188,
		188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188,
		188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188,
		188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188,
		188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188,
		188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188,
		188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188,
		188, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189,
		189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189,
		189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189,
		189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189,
		189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189,
		189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189,
		189, 189, 189, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190,
		190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190,
		190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190,
		190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190,
		190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190,
		190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190,
		190, 190, 190, 190, 190, 190, 190, 191, 191, 191, 191, 191, 191, 191, 191, 191,
		191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191,
		191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191,
		191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191,
		191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191,
		191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191,
		191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 192, 192, 192, 192, 192,
		192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
		192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
		192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
		192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
		192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
		192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 192, 193,
		193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193,
		193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193,
		193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193,
		193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193,
		193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193,
		193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193, 193,
		193, 193, 193, 193, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194,
		194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194,
		194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194,
		194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194,
		194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194,
		194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194,
		194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 195, 195, 195, 195, 195, 195,
		195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195,
		195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195,
		195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195,
		195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195,
		195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195,
		195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195,
		195, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196,
		196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196,
		196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196,
		196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196,
		196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196,
		196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196,
		196, 196, 196, 196, 196, 196, 196, 196, 197, 197, 197, 197, 197, 197, 197, 197,
		197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197,
		197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197,
		197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197,
		197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197,
		197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197,
		197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197,
		198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198,
		198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198,
		198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198,
		198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198,
		198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198,
		198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198,
		198, 198, 198, 198, 198, 198, 198, 198, 198, 199, 199, 199, 199, 199, 199, 199,
		199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
		199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
		199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
		199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
		199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
		199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199,
		199, 199, 199, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200,
		200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200,
		200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200,
		200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200,
		200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200,
		200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200,
		200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 201, 201, 201,
		201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201,
		201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201,
		201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201,
		201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201,
		201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201,
		201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201,
		201, 201, 201, 201, 201, 201, 201, 201, 202, 202, 202, 202, 202, 202, 202, 202,
		202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202,
		202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202,
		202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202,
		202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202,
		202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202,
		202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202,
		202, 202, 202, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203,
		203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203,
		203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203,
		203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203,
		203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203,
		203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203,
		203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 204,
		204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204,
		204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204,
		204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204,
		204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204,
		204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204,
		204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204,
		204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 205, 205, 205, 205,
		205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205,
		205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205,
		205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205,
		205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205,
		205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205,
		205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205,
		205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 206, 206, 206, 206, 206, 206,
		206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206,
		206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206,
		206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206,
		206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206,
		206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206,
		206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206,
		206, 206, 206, 206, 206, 206, 206, 206, 207, 207, 207, 207, 207, 207, 207, 207,
		207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
		207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
		207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
		207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
		207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
		207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
		207, 207, 207, 207, 207, 207, 207, 208, 208, 208, 208, 208, 208, 208, 208, 208,
		208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208,
		208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208,
		208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208,
		208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208,
		208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208,
		208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208,
		208, 208, 208, 208, 208, 208, 208, 209, 209, 209, 209, 209, 209, 209, 209, 209,
		209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209,
		209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209,
		209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209,
		209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209,
		209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209,
		209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209,
		209, 209, 209, 209, 209, 209, 209, 210, 210, 210, 210, 210, 210, 210, 210, 210,
		210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
		210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
		210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
		210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
		210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
		210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210,
		210, 210, 210, 210, 210, 210, 210, 210, 211, 211, 211, 211, 211, 211, 211, 211,
		211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
		211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
		211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
		211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
		211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
		211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211,
		211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 212, 212, 212, 212, 212, 212,
		212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
		212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
		212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
		212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
		212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
		212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212,
		212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 213, 213, 213,
		213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
		213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
		213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
		213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
		213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
		213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
		213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213,
		214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214,
		214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214,
		214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214,
		214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214,
		214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214,
		214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214,
		214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214,
		214, 214, 214, 214, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215,
		215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215,
		215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215,
		215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215,
		215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215,
		215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215,
		215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215,
		215, 215, 215, 215, 215, 215, 215, 215, 215, 216, 216, 216, 216, 216, 216, 216,
		216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216,
		216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216,
		216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216,
		216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216,
		216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216,
		216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216,
		216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 217, 217,
		217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217,
		217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217,
		217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217,
		217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217,
		217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217,
		217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217,
		217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217,
		217, 217, 217, 217, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218,
		218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218,
		218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218,
		218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218,
		218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218,
		218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218,
		218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218,
		218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 219, 219, 219, 219, 219,
		219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219,
		219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219,
		219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219,
		219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219,
		219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219,
		219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219,
		219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219,
		219, 219, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220,
		220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220,
		220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220,
		220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220,
		220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220,
		220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220,
		220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220,
		220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 221, 221, 221, 221, 221,
		221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221,
		221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221,
		221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221,
		221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221,
		221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221,
		221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221,
		221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221,
		221, 221, 221, 221, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222,
		222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222,
		222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222,
		222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222,
		222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222,
		222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222,
		222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222,
		222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 222, 223, 223, 223,
		223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223,
		223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223,
		223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223,
		223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223,
		223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223,
		223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223,
		223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223, 223,
		223, 223, 223, 223, 223, 223, 223, 223, 224, 224, 224, 224, 224, 224, 224, 224,
		224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
		224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
		224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
		224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
		224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
		224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
		224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
		224, 224, 224, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
		225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
		225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
		225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
		225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
		225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
		225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225,
		225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 226,
		226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226,
		226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226,
		226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226,
		226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226,
		226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226,
		226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226,
		226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226,
		226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 227, 227, 227, 227, 227,
		227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227,
		227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227,
		227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227,
		227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227,
		227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227,
		227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227,
		227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227,
		227, 227, 227, 227, 227, 227, 227, 227, 227, 228, 228, 228, 228, 228, 228, 228,
		228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228,
		228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228,
		228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228,
		228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228,
		228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228,
		228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228,
		228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228,
		228, 228, 228, 228, 228, 228, 228, 229, 229, 229, 229, 229, 229, 229, 229, 229,
		229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229,
		229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229,
		229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229,
		229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229,
		229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229,
		229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229,
		229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229,
		229, 229, 229, 229, 229, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
		230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
		230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
		230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
		230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
		230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
		230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
		230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
		230, 230, 230, 230, 230, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231,
		231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231,
		231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231,
		231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231,
		231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231,
		231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231,
		231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231,
		231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231, 231,
		231, 231, 231, 231, 231, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232,
		232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232,
		232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232,
		232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232,
		232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232,
		232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232,
		232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232,
		232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232,
		232, 232, 232, 232, 232, 232, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233,
		233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233,
		233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233,
		233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233,
		233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233,
		233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233,
		233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233,
		233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233,
		233, 233, 233, 233, 233, 233, 233, 233, 234, 234, 234, 234, 234, 234, 234, 234,
		234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234,
		234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234,
		234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234,
		234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234,
		234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234,
		234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234,
		234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234,
		234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 235, 235, 235, 235, 235, 235,
		235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235,
		235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235,
		235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235,
		235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235,
		235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235,
		235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235,
		235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235,
		235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 236, 236,
		236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
		236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
		236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
		236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
		236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
		236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
		236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
		236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236,
		236, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237,
		237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237,
		237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237,
		237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237,
		237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237,
		237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237,
		237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237,
		237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237,
		237, 237, 237, 237, 237, 237, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
		238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
		238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
		238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
		238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
		238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
		238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
		238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
		238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 239, 239, 239, 239, 239,
		239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
		239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
		239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
		239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
		239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
		239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
		239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
		239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
		239, 239, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
		240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
		240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
		240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
		240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
		240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
		240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
		240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
		240, 240, 240, 240, 240, 240, 240, 240, 240, 241, 241, 241, 241, 241, 241, 241,
		241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
		241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
		241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
		241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
		241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
		241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
		241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
		241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241,
		242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242,
		242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242,
		242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242,
		242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242,
		242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242,
		242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242,
		242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242,
		242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242,
		242, 242, 242, 242, 242, 242, 242, 242, 242, 243, 243, 243, 243, 243, 243, 243,
		243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243,
		243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243,
		243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243,
		243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243,
		243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243,
		243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243,
		243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243,
		243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243,
		243, 243, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244,
		244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244,
		244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244,
		244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244,
		244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244,
		244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244,
		244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244,
		244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244,
		244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 245, 245, 245, 245, 245,
		245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245,
		245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245,
		245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245,
		245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245,
		245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245,
		245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245,
		245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245,
		245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245,
		245, 245, 245, 245, 245, 245, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246,
		246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246,
		246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246,
		246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246,
		246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246,
		246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246,
		246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246,
		246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246,
		246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246,
		246, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
		247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
		247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
		247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
		247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
		247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
		247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
		247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247,
		247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 248, 248,
		248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
		248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
		248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
		248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
		248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
		248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
		248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
		248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248,
		248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 249, 249, 249, 249, 249, 249,
		249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
		249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
		249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
		249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
		249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
		249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
		249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
		249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
		249, 249, 249, 249, 249, 249, 249, 249, 250, 250, 250, 250, 250, 250, 250, 250,
		250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
		250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
		250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
		250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
		250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
		250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
		250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
		250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
		250, 250, 250, 250, 250, 250, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
		251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
		251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
		251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
		251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
		251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
		251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
		251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
		251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251,
		251, 251, 251, 251, 251, 251, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
		252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
		252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
		252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
		252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
		252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
		252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
		252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
		252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252,
		252, 252, 252, 252, 252, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
		253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
		253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
		253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
		253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
		253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
		253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
		253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
		253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
		253, 253, 253, 253, 253, 253, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
		254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
		254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
		254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
		254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
		254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
		254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
		254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
		254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
		254, 254, 254, 254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	};

	bool UtilitiesDriver::ContainsWildcards(const StringSliceView &str) const
	{
		for (char c : str)
		{
			if (c == '?' || c == '*')
				return true;
		}

		return false;
	}

	bool UtilitiesDriver::MatchesWildcard(const StringSliceView &candidateRef, const StringSliceView &wildcardRef) const
	{
		StringSliceView candidate = candidateRef;
		StringSliceView wildcard = wildcardRef;

		size_t minLiteralChars = 0;
		bool prefixHasAsterisk = 0;

		for (char c : wildcardRef)
		{
			if (c != '*')
				minLiteralChars++;
		}

		return MatchesWildcardWithMinLiteralCount(candidateRef, wildcardRef, minLiteralChars);
	}

	bool UtilitiesDriver::DefaultIsPathComponentValid(const StringSliceView &span, bool isFirst, bool allowWildcards) const
	{
		if (span.Length() == 0)
			return false;

		if (span[span.Length() - 1] == '.')
		{
			// Check paths ending with periods
			const bool isParentDir = (span.Length() == 2 && span[0] == '.');
			const bool isCurrentDir = (span.Length() == 1 && span[0] == '.' && isFirst);

			if (!isParentDir && !isCurrentDir)
				return false;
		}
		else
		{
			// Check for consecutive periods
			for (size_t i = 1; i < span.Length(); i++)
			{
				if (span[i] == '.' && span[i - 1] == '.')
					return false;
			}
		}

		// Check for 8.3 truncations
		if (span.Length() <= 11)
		{
			size_t dotPos = span.Length();
			for (size_t ri = 1; ri < span.Length() && ri < 3; ri++)
			{
				size_t i = span.Length() - 1 - ri;
				if (span[i] == '.')
				{
					dotPos = span[i];
					break;
				}
			}

			if (dotPos <= 8 && dotPos >= 2 && span[dotPos - 2] == '~' && span[dotPos - 1] >= '0' && span[dotPos - 1] <= '9')
				return false;
		}

		for (char ch : span)
		{
			if (ch >= 0 && ch < 32)
				return false;

			for (const char *invalidCharPtr = "<>:\"/\\|?*"; *invalidCharPtr; invalidCharPtr++)
			{
				if (ch == *invalidCharPtr)
					return false;
			}
		}

		if (span.Length() == 3 || (span.Length() > 3 && span[3] == '.'))
		{
			const char *bannedNames[] =
			{
				"con", "prn", "aux", "nul"
			};

			for (const char *bannedName : bannedNames)
			{
				bool isBanned = true;
				for (size_t chIndex = 0; chIndex < 3; chIndex++)
				{
					char ch = span[chIndex];
					if (ch >= 'A' && ch <= 'Z')
						ch = (ch - 'A') + 'a';

					if (ch != bannedName[chIndex])
					{
						isBanned = false;
						break;
					}
				}

				if (isBanned)
					return false;
			}
		}

		// Check for Windows banned names
		if (
			((span.Length() == 4 || (span.Length() > 4 && span[4] == '.')) && span[3] >= '0' && span[3] <= '9')
			|| ((span.Length() == 5 || (span.Length() > 5 && span[5] == '.')) && span[3] == '\xc2' && (span[4] == '\xb2' || span[4] == '\xb3' || span[4] == '\xb9'))
			)
		{
			const char *bannedPrefixes[] =
			{
				"com", "lpt"
			};

			for (const char *bannedPrefix : bannedPrefixes)
			{
				bool isBanned = true;
				for (size_t chIndex = 0; chIndex < 3; chIndex++)
				{
					char ch = span[chIndex];
					if (ch >= 'A' && ch <= 'Z')
						ch = (ch - 'A') + 'a';

					if (ch != bannedPrefix[chIndex])
					{
						isBanned = false;
						break;
					}
				}

				if (isBanned)
					return false;
			}
		}

		// Check for invalid UTF-8
		for (size_t i = 0; i < span.Length(); i++)
		{
			const char cchar = span[i];
			const uint8_t codeByte = static_cast<uint8_t>(cchar);

			if (codeByte & 0x80)
			{
				size_t maxCodeLength = span.Length() - i;
				if (maxCodeLength > 4)
					maxCodeLength = 4;

				bool isValidCode = false;
				for (size_t codeLength = 2; codeLength <= maxCodeLength; codeLength++)
				{
					uint8_t mask = (0xf80u >> codeLength) & 0xffu;
					uint8_t checkValue = (0xf00u >> codeLength) & 0xffu;
					uint8_t lowBitsMask = (0x7fu >> codeLength);
					if ((codeByte & mask) == checkValue)
					{
						if (codeByte == checkValue)
							return false;	// Not minimum length

						uint32_t codePoint = static_cast<uint32_t>(codeByte & lowBitsMask) << (6 * (codeLength - 1));

						for (size_t extraByteIndex = 1; extraByteIndex < codeLength; extraByteIndex++)
						{
							const size_t bitPos = (codeLength - extraByteIndex - 1) * 6;
							const uint8_t extraByte = static_cast<uint8_t>(span[i + extraByteIndex]);

							if ((extraByte & 0xc0) != 0x80)
								return false;

							codePoint |= static_cast<uint32_t>(extraByte) << bitPos;
						}

						// Out of range
						if (codePoint > 0x10ffffu)
							return false;

						// UTF-16 surrogates
						if (codePoint >= 0xd800u && codePoint <= 0xdfffu)
							return false;

						isValidCode = true;
						i += codeLength - 1;
						break;
					}
				}

				if (!isValidCode)
					return false;
			}
		}

		return true;
	}

	Result UtilitiesDriver::ConvertUTF16ToUTF8(size_t &outSize, const Span<uint8_t> &dest, const Span<const uint16_t> &src) const
	{
		return TypedConvertUTF16ToUTF8(outSize, dest, src);
	}

	Result UtilitiesDriver::ConvertUTF16WCharToUTF8(size_t &outSize, const Span<uint8_t> &dest, const Span<const wchar_t> &src) const
	{
		return TypedConvertUTF16ToUTF8(outSize, dest, src);
	}

	Result UtilitiesDriver::ConvertUTF8ToUTF16(size_t &outSize, const Span<uint16_t> &dest, const Span<const uint8_t> &src) const
	{
		return TypedConvertUTF8ToUTF16(outSize, dest, src);
	}

	Result UtilitiesDriver::ConvertUTF8ToUTF16WChar(size_t &outSize, const Span<wchar_t> &dest, const Span<const uint8_t> &src) const
	{
		return TypedConvertUTF8ToUTF16(outSize, dest, src);
	}

	bool UtilitiesDriver::IsPathComponentValidOnWindows(const BaseStringSliceView<OSPathChar_t, CharacterEncoding::kOSPath> &span, bool isAbsolute, bool isFirst, bool allowWildcards) const
	{
		if (isAbsolute && isFirst)
		{
			if (span.Length() != 2)
				return false;

			if (span[1] != ':')
				return false;

			wchar_t firstChar = span[0];
			if ((firstChar < 'A' || firstChar > 'Z') && (firstChar < 'a' || firstChar > 'z'))
				return false;

			return true;
		}

		if (span[span.Length() - 1] == '.')
			return false;

		// Check for 8.3 truncations
		if (span.Length() <= 11)
		{
			size_t dotPos = span.Length();
			for (size_t ri = 1; ri < span.Length() && ri < 3; ri++)
			{
				size_t i = span.Length() - 1 - ri;
				if (span[i] == '.')
				{
					dotPos = span[i];
					break;
				}
			}

			if (dotPos <= 8 && dotPos >= 2 && span[dotPos - 2] == '~' && span[dotPos - 1] >= '0' && span[dotPos - 1] <= '9')
				return false;
		}

		for (wchar_t ch : span)
		{
			if (ch >= 0 && ch < 32)
				return false;

			for (const char *invalidCharPtr = "<>:\"/\\|?*"; *invalidCharPtr; invalidCharPtr++)
			{
				if (ch == *invalidCharPtr)
					return false;
			}
		}

		if (span.Length() == 3 || (span.Length() > 3 && span[3] == '.'))
		{
			const wchar_t *bannedNames[] =
			{
				L"con", L"prn", L"aux", L"nul"
			};

			for (const wchar_t *bannedName : bannedNames)
			{
				bool isBanned = true;
				for (size_t chIndex = 0; chIndex < 3; chIndex++)
				{
					wchar_t ch = span[chIndex];
					if (ch >= 'A' && ch <= 'Z')
						ch = (ch - 'A') + 'a';

					if (ch != bannedName[chIndex])
					{
						isBanned = false;
						break;
					}
				}

				if (isBanned)
					return false;
			}
		}

		// Check for Windows banned names
		if (
			((span.Length() == 4 || (span.Length() > 4 && span[4] == '.')) && span[3] >= '0' && span[3] <= '9')
			|| ((span.Length() == 5 || (span.Length() > 5 && span[5] == '.')) && span[3] == 0xc2 && (span[4] == 0xb2 || span[4] == 0xb3 || span[4] == 0xb9))
			)
		{
			const wchar_t *bannedPrefixes[] =
			{
				L"com", L"lpt"
			};

			for (const wchar_t *bannedPrefix : bannedPrefixes)
			{
				bool isBanned = true;
				for (size_t chIndex = 0; chIndex < 3; chIndex++)
				{
					wchar_t ch = span[chIndex];
					if (ch >= 'A' && ch <= 'Z')
						ch = (ch - 'A') + 'a';

					if (ch != bannedPrefix[chIndex])
					{
						isBanned = false;
						break;
					}
				}

				if (isBanned)
					return false;
			}
		}

		// Check for invalid UTF-16
		for (size_t i = 0; i < span.Length(); i++)
		{
			const wchar_t wchar = span[i];

			if (wchar < 0xd7ff || wchar >= 0xe000)
				continue;

			if (wchar >= 0xdc00 && wchar <= 0xdfff)
				return false;	// Orphaned low surrogate

			if (i == span.Length() - 1)
				return false;

			i++;
			const wchar_t lowSurrogate = span[i];

			if (lowSurrogate < 0xdc00 || lowSurrogate > 0xdfff)
				return false;
		}

		return true;
	}

	bool UtilitiesDriver::MatchesWildcardWithMinLiteralCount(const StringSliceView &candidateRef, const StringSliceView &wildcardRef, size_t minLiteralCount)
	{
		StringSliceView candidate = candidateRef;
		StringSliceView wildcard = wildcardRef;

		if (wildcardRef.Length() == 0)
			return candidateRef.Length() == 0;

		size_t prefixAnyCount = 0;
		bool prefixHasAsterisk = false;

		size_t wildcardPrefixSkipSize = 0;

		while (wildcardPrefixSkipSize < wildcard.Length())
		{
			const char c = wildcard[wildcardPrefixSkipSize];

			if (c == '?')
				prefixAnyCount++;
			else if (c == '*')
				prefixHasAsterisk = true;
			else
				break;

			wildcardPrefixSkipSize++;
		}

		candidate = candidate.SubString(prefixAnyCount);
		wildcard = wildcard.SubString(wildcardPrefixSkipSize);

		minLiteralCount -= prefixAnyCount;

		size_t wildcardNonAsteriskChunkSize = 0;

		while (wildcardNonAsteriskChunkSize < wildcard.Length())
		{
			if (wildcard[wildcardNonAsteriskChunkSize] == '*')
				break;

			wildcardNonAsteriskChunkSize++;
		}

		size_t maxStartLoc = 0;
		if (prefixHasAsterisk)
			maxStartLoc = candidate.Length() - minLiteralCount;

		for (;;)
		{
			bool matches = true;

			for (size_t i = 0; i < wildcardNonAsteriskChunkSize; i++)
			{
				const char wcChar = wildcard[i];
				if (wcChar != '?' && wcChar != candidate[i])
				{
					matches = false;
					break;
				}
			}

			if (matches)
			{
				if (MatchesWildcardWithMinLiteralCount(candidate.SubString(wildcardNonAsteriskChunkSize), wildcard.SubString(wildcardNonAsteriskChunkSize), minLiteralCount - wildcardNonAsteriskChunkSize))
					return true;
			}

			// No match here
			if (maxStartLoc == 0)
				return false;

			candidate = candidate.SubString(1);
			maxStartLoc--;
		}
	}

	template<class TWChar>
	Result UtilitiesDriver::TypedConvertUTF16ToUTF8(size_t &outSize, const Span<uint8_t> &dest, const Span<const TWChar> &src)
	{
		size_t availableSrc = src.Count();
		const TWChar *srcWords = src.Ptr();

		size_t availableDest = dest.Count();
		uint8_t *destBytes = dest.Ptr();

		size_t resultSize = 0;

		while (availableSrc > 0)
		{
			size_t wordsDigested = 0;
			uint32_t codePoint = 0;
			if (!UTF16::Decode(srcWords, availableSrc, wordsDigested, codePoint))
				RKIT_THROW(ResultCode::kInvalidUnicode);

			availableSrc -= wordsDigested;
			srcWords += wordsDigested;

			uint8_t encoded[UTF8::kMaxEncodedBytes];
			size_t bytesEmitted = 0;
			UTF8::Encode(encoded, bytesEmitted, codePoint);

			if (availableDest >= bytesEmitted)
			{
				CopySpanNonOverlapping(Span<uint8_t>(destBytes, bytesEmitted), ConstSpan<uint8_t>(encoded, bytesEmitted));

				destBytes += bytesEmitted;
				availableDest -= bytesEmitted;
			}

			resultSize += bytesEmitted;
		}

		outSize = resultSize;

		RKIT_RETURN_OK;
	}

	template<class TWChar>
	Result UtilitiesDriver::TypedConvertUTF8ToUTF16(size_t &outSize, const Span<TWChar> &dest, const Span<const uint8_t> &src)
	{
		size_t availableSrc = src.Count();
		const uint8_t *srcBytes = src.Ptr();

		size_t availableDest = dest.Count();
		TWChar *destChars = dest.Ptr();

		size_t resultSize = 0;

		while (availableSrc > 0)
		{
			size_t bytesDigested = 0;
			uint32_t codePoint = 0;
			if (!UTF8::Decode(srcBytes, availableSrc, bytesDigested, codePoint))
				RKIT_THROW(ResultCode::kInvalidUnicode);

			availableSrc -= bytesDigested;
			srcBytes += bytesDigested;

			uint16_t encoded[UTF16::kMaxEncodedWords];
			size_t dwordsEmitted = 0;
			UTF16::Encode(encoded, dwordsEmitted, codePoint);

			if (availableDest >= dwordsEmitted)
			{
				for (size_t i = 0; i < dwordsEmitted; i++)
					destChars[i] = static_cast<TWChar>(encoded[i]);

				destChars += dwordsEmitted;
				availableDest -= dwordsEmitted;
			}

			resultSize += dwordsEmitted;
		}

		outSize = resultSize;

		RKIT_RETURN_OK;
	}

	bool UtilitiesDriver::TryParseDigit(uint8_t c, uint8_t &outDigit)
	{
		if (c >= '0' && c <= '9')
		{
			outDigit = c - '0';
			return true;
		}

		if (c >= 'a' && c <= 'f')
		{
			outDigit = c - 'a' + 0xa;
			return true;
		}

		if (c >= 'A' && c <= 'F')
		{
			outDigit = c - 'A' + 0xA;
			return true;
		}

		return false;
	}

	template<class TInteger>
	bool UtilitiesDriver::ParsePositiveInt(const ByteStringSliceView &str, size_t startDigit, uint8_t radixLow, TInteger &i)
	{
		TInteger radix = radixLow;
		if (radix < 2 || radix > 16 || startDigit == str.Length())
			return false;

		TInteger result = 0;

		const TInteger posMaxLastMajor = std::numeric_limits<TInteger>::max() / radix;
		const TInteger posMaxDangerZone = std::numeric_limits<TInteger>::max() - radix + 1;

		for (size_t i = startDigit; i < str.Length(); i++)
		{
			uint8_t digit = 0;
			if (!TryParseDigit(str[i], digit))
				return false;

			if (digit >= radixLow)
				return false;

			if (result > posMaxLastMajor)
				return false;	// Overflow

			result *= static_cast<TInteger>(radix);
			if (result > posMaxDangerZone)
			{
				TInteger maxDigit = std::numeric_limits<TInteger>::max() - result;
				if (digit > maxDigit)
					return false;
			}

			result += digit;
		}

		i = result;
		return true;
	}

	template<class TInteger>
	bool UtilitiesDriver::ParseNegativeInt(const ByteStringSliceView &str, size_t startDigit, uint8_t radixLow, TInteger &i)
	{
		int16_t radix = radixLow;
		if (radix < 2 || radix > 16 || startDigit == str.Length())
			return false;

		TInteger result = 0;

		const TInteger posMinLastMajor = std::numeric_limits<TInteger>::min() / radix;
		const TInteger posMinDangerZone = std::numeric_limits<TInteger>::min() + radix - 1;

		for (size_t i = startDigit; i < str.Length(); i++)
		{
			uint8_t digit = 0;
			if (!TryParseDigit(str[i], digit))
				return false;

			if (digit >= radixLow)
				return false;

			if (result < posMinLastMajor)
				return false;	// Overflow

			result *= radix;
			if (result < posMinDangerZone)
			{
				TInteger maxDigit = result - std::numeric_limits<TInteger>::min();
				if (digit > maxDigit)
					return false;
			}

			result -= digit;
		}

		i = result;
		return true;
	}

	template<class TInteger>
	bool UtilitiesDriver::ParseSignedInt(const ByteStringSliceView &str, uint8_t radix, TInteger &i)
	{
		if (str.Length() == 0)
			return false;

		if (str[0] == '-')
			return ParseNegativeInt(str, 1, radix, i);

		if (str[0] == '+')
			return ParsePositiveInt(str, 1, radix, i);

		return ParsePositiveInt(str, 0, radix, i);
	}

	Result UtilitiesDriver::CreateCoro2Thread(UniquePtr<ICoroThread> &thread, size_t stackSize) const
	{
		UniquePtr<utils::Coro2ThreadBase> threadBase;
		RKIT_CHECK(utils::Coro2ThreadBase::Create(threadBase, stackSize));

		thread = std::move(threadBase);

		RKIT_RETURN_OK;
	}

	bool UtilitiesDriver::ParseDouble(const ByteStringSliceView &str, double &d) const
	{
		size_t len = str.Length();
		if (!utils::CharsToDouble(d, str.GetChars(), len))
			return false;

		return len == str.Length();
	}

	bool UtilitiesDriver::ParseFloat(const ByteStringSliceView &str, float &f) const
	{
		size_t len = str.Length();
		if (!utils::CharsToFloat(f, str.GetChars(), len))
			return false;

		return len == str.Length();
	}

	bool UtilitiesDriver::ParseInt32(const ByteStringSliceView &str, uint8_t radix, int32_t &i) const
	{
		return ParseSignedInt<int32_t>(str, radix, i);
	}

	bool UtilitiesDriver::ParseInt64(const ByteStringSliceView &str, uint8_t radix, int64_t &i) const
	{
		return ParseSignedInt<int64_t>(str, radix, i);
	}

	bool UtilitiesDriver::ParseUInt32(const ByteStringSliceView &str, uint8_t radix, uint32_t &i) const
	{
		return ParsePositiveInt<uint32_t>(str, 0, radix, i);
	}

	bool UtilitiesDriver::ParseUInt64(const ByteStringSliceView &str, uint8_t radix, uint64_t &i) const
	{
		return ParsePositiveInt<uint64_t>(str, 0, radix, i);
	}

	Result UtilitiesDriver::CreateImage(const utils::ImageSpec &spec, UniquePtr<utils::IImage> &image) const
	{
		UniquePtr<utils::ImageBase> imageBase;
		RKIT_CHECK(utils::ImageBase::Create(imageBase, spec));
		image = std::move(imageBase);
		RKIT_RETURN_OK;
	}

	Result UtilitiesDriver::CloneImage(UniquePtr<utils::IImage> &outImage, const utils::IImage &image) const
	{
		UniquePtr<utils::ImageBase> imageBase;
		RKIT_CHECK(utils::ImageBase::Create(imageBase, image.GetImageSpec()));

		RKIT_CHECK(BlitImage(*imageBase, image, 0, 0, 0, 0, image.GetWidth(), image.GetHeight()));

		outImage = std::move(imageBase);
		RKIT_RETURN_OK;
	}

	Result UtilitiesDriver::BlitImageSigned(utils::IImage &destImage, const utils::IImage &srcImage, ptrdiff_t srcX, ptrdiff_t srcY, ptrdiff_t destX, ptrdiff_t destY, size_t width, size_t height) const
	{
		const utils::ImageSpec &destSpec = destImage.GetImageSpec();
		const utils::ImageSpec &srcSpec = srcImage.GetImageSpec();

		ptrdiff_t minX = Min(srcX, destX);
		ptrdiff_t minY = Min(srcY, destY);

		// Insets a coordinate and returns true if it is still valid
		auto adjustCoords = [](size_t (&outCoords)[2], const ptrdiff_t (&inCoords)[2], size_t &sizeMeasure) -> bool
			{
				ptrdiff_t minCoord = Min(inCoords[0], inCoords[1]);

				if (minCoord < 0)
				{
					const size_t negativeMinCoord = static_cast<size_t>(-(minCoord + 1)) + 1;

					if (sizeMeasure <= negativeMinCoord)
						return false;

					for (int i = 0; i < 2; i++)
					{
						const ptrdiff_t coord = inCoords[i];

						if (coord < 0)
						{
							const size_t negativeCoord = static_cast<size_t>(-(coord + 1)) + 1;

							outCoords[i] = static_cast<size_t>(negativeCoord - negativeMinCoord);
						}
						else
						{
							const size_t uCoord = static_cast<size_t>(coord);

							if (std::numeric_limits<size_t>::max() - uCoord < negativeMinCoord)
								return false;

							outCoords[i] = uCoord + negativeMinCoord;
						}
					}

					sizeMeasure -= negativeMinCoord;
				}
				else
				{
					for (int i = 0; i < 2; i++)
						outCoords[i] = static_cast<size_t>(inCoords[i]);
				}

				return true;
			};

		size_t srcDestXOut[2] = { 0, 0 };
		ptrdiff_t srcDestXIn[2] = { srcX, destX };

		size_t srcDestYOut[2] = { 0, 0 };
		ptrdiff_t srcDestYIn[2] = { srcY, destY };

		if (!adjustCoords(srcDestXOut, srcDestXIn, width)
			|| !adjustCoords(srcDestYOut, srcDestYIn, height))
		{
			RKIT_RETURN_OK;
		}

		return BlitImage(destImage, srcImage, srcDestXOut[0], srcDestYOut[0], srcDestXOut[1], srcDestYOut[1], width, height);
	}

	Result UtilitiesDriver::BlitImage(utils::IImage &destImage, const utils::IImage &srcImage, size_t srcX, size_t srcY, size_t destX, size_t destY, size_t width, size_t height) const
	{
		const utils::ImageSpec &destSpec = destImage.GetImageSpec();
		const utils::ImageSpec &srcSpec = srcImage.GetImageSpec();

		if (srcX >= srcSpec.m_width || srcY >= srcSpec.m_height || destX >= destSpec.m_width || destY >= destSpec.m_height)
			RKIT_RETURN_OK;

		bool requiresConversion = (destSpec.m_pixelPacking != srcSpec.m_pixelPacking || destSpec.m_numChannels != srcSpec.m_numChannels);

		const size_t maxDestWidth = destSpec.m_width - destX;
		const size_t maxDestHeight = destSpec.m_height - destY;
		const size_t maxSrcWidth = srcSpec.m_width - srcX;
		const size_t maxSrcHeight = srcSpec.m_height - srcY;

		width = rkit::Min(rkit::Min(width, maxDestWidth), maxSrcWidth);
		height = rkit::Min(rkit::Min(height, maxDestHeight), maxSrcHeight);

		if (requiresConversion)
			RKIT_THROW(ResultCode::kNotYetImplemented);

		const size_t bytesPerPixel = utils::img::BytesPerPixel(srcSpec.m_numChannels, srcSpec.m_pixelPacking);
		const size_t srcByteOffset = srcX * bytesPerPixel;
		const size_t destByteOffset = destX * bytesPerPixel;
		const size_t spanSize = width * bytesPerPixel;

		for (size_t relY = 0; relY < height; relY++)
		{
			const size_t srcRow = srcY + relY;
			const size_t destRow = destY + relY;

			utils::img::BlitScanline(destImage, srcImage, srcRow, srcByteOffset, destRow, destByteOffset, spanSize);
		}

		RKIT_RETURN_OK;
	}

	void UtilitiesDriver::FormatSignedInt(IFormatStringWriter<Utf8Char_t> &writer, intmax_t value) const
	{
		if (value == 0)
		{
			writer.WriteChars(ConstSpan<Utf8Char_t>(u8"0", 1));
			return;
		}

		const size_t kMaxChars = ((sizeof(value) * 8) + 2) / 3 + 1;
		Utf8Char_t outChars[kMaxChars];

		const Utf8Char_t *kDigits = u8"0123456789";

		size_t strStartPos = kMaxChars;

		if (value < 0)
		{
			while (value != 0)
			{
				const intmax_t remainder = (value % 10);
				value = value / 10;

				--strStartPos;
				outChars[strStartPos] = kDigits[-remainder];
			}
			--strStartPos;
			outChars[strStartPos] = '-';
		}
		else
		{
			while (value != 0)
			{
				const intmax_t remainder = (value % 10);
				value = value / 10;

				--strStartPos;
				outChars[strStartPos] = kDigits[remainder];
			}
		}

		writer.WriteChars(ConstSpan<Utf8Char_t>(outChars + strStartPos, kMaxChars - strStartPos));
	}

	void UtilitiesDriver::FormatUnsignedInt(IFormatStringWriter<Utf8Char_t> &writer, uintmax_t value) const
	{
		if (value == 0)
		{
			writer.WriteChars(ConstSpan<Utf8Char_t>(u8"0", 1));
			return;
		}

		const size_t kMaxChars = ((sizeof(value) * 8) + 2) / 3;
		Utf8Char_t outChars[kMaxChars];

		const Utf8Char_t *kDigits = u8"0123456789";

		size_t strStartPos = kMaxChars;

		while (value != 0)
		{
			const uintmax_t remainder = (value % 10u);
			value = value / 10u;

			--strStartPos;
			outChars[strStartPos] = kDigits[remainder];
		}

		writer.WriteChars(ConstSpan<Utf8Char_t>(outChars + strStartPos, kMaxChars - strStartPos));
	}

	void UtilitiesDriver::FormatFloat(IFormatStringWriter<Utf8Char_t> &writer, float f) const
	{
		Utf8Char_t floatChars[16];
		int nChars = f2s_buffered_n(f, ReinterpretUtf8CharToAnsiChar(floatChars));

		writer.WriteChars(ConstSpan<Utf8Char_t>(floatChars, static_cast<size_t>(nChars)));
	}

	void UtilitiesDriver::FormatDouble(IFormatStringWriter<Utf8Char_t> &writer, double f) const
	{
		Utf8Char_t doubleChars[25];
		int nChars = d2s_buffered_n(f, ReinterpretUtf8CharToAnsiChar(doubleChars));

		writer.WriteChars(ConstSpan<Utf8Char_t>(doubleChars, static_cast<size_t>(nChars)));
	}

	void UtilitiesDriver::FormatCString(IFormatStringWriter<char> &writer, const char *str) const
	{
		writer.WriteChars(ConstSpan<char>(str, strlen(str)));
	}

	void UtilitiesDriver::FormatUtf8String(IFormatStringWriter<Utf8Char_t> &writer, const Utf8Char_t *str) const
	{
		writer.WriteChars(StringView::FromCString(str).ToSpan());
	}

	template<class TChar, CharacterEncoding TEncoding>
	void UtilitiesDriver::FormatStringImpl(IFormatStringWriter<TChar> &writer, const BaseStringSliceView<TChar, TEncoding> &fmtRef, const FormatParameterList<TChar> &paramListRef)
	{
		const BaseStringSliceView<TChar, TEncoding> fmt = fmtRef;
		const FormatParameterList<TChar> paramList = paramListRef;

		const size_t fmtStringLength = fmt.Length();

		const size_t mul10Limit = paramListRef.Count() / 10u;

		size_t scanPos = 0;
		size_t contiguousStart = 0;
		size_t numUnindexedArgs = 0;
		for (;;)
		{
			if (scanPos == fmtStringLength || fmt[scanPos] == '{')
			{
				const size_t contiguousLength = scanPos - contiguousStart;
				if (contiguousLength > 0)
					writer.WriteChars(fmt.SubString(contiguousStart, contiguousLength).ToSpan());

				if (scanPos == fmtStringLength)
					break;

				scanPos++;
				bool isValid = true;
				bool isIndexed = false;

				size_t argIndex = 0;
				for (;;)
				{
					if (scanPos == fmtStringLength)
					{
						isValid = false;
						break;
					}

					const TChar nextCh = fmt[scanPos];
					if (nextCh == '}')
					{
						scanPos++;
						contiguousStart = scanPos;
						break;
					}
					else if (nextCh >= '0' && nextCh <= '9')
					{
						if (isValid)
						{
							isIndexed = true;
							if (argIndex > mul10Limit)
								isValid = false;
							else
							{
								argIndex *= 10u;

								const uint8_t digit = (nextCh - '0');
								if (digit >= paramList.Count() - argIndex)
									isValid = false;
								else
									argIndex += digit;
							}
						}
					}
					else
						isValid = false;

					scanPos++;
				}

				if (isValid && !isIndexed)
				{
					if (paramList.Count() <= numUnindexedArgs)
						isValid = false;
					else
					{
						argIndex = numUnindexedArgs;
						numUnindexedArgs++;
					}
				}

				if (!isValid)
				{
					const TChar invalidText[] = { '<', 'I', 'N', 'V', 'A', 'L', 'I', 'D', '>' };
					const size_t kNumInvalidChars = sizeof(invalidText) / sizeof(TChar);

					writer.WriteChars(ConstSpan<TChar>(invalidText, kNumInvalidChars));
				}
				else
				{
					const FormatParameter<TChar> &formatParam = paramList[argIndex];
					formatParam.m_formatCallback(writer, formatParam.m_dataPtr);
				}
			}
			else
				scanPos++;
		}
	}

	void UtilitiesDriver::FormatString(IFormatStringWriter<Utf8Char_t> &writer, const StringSliceView &fmt, const FormatParameterList<Utf8Char_t> &paramList) const
	{
		FormatStringImpl(writer, fmt, paramList);
	}

	void UtilitiesDriver::SanitizeClampFloats(const Span<float> &outFloats, const Span<const endian::LittleFloat32_t> &inFloats, int maxMagnitude) const
	{
		const endian::LittleFloat32_t *inFloatsPtr = inFloats.Ptr();
		float *outFloatsPtr = outFloats.Ptr();

		const size_t numFloats = rkit::Min(inFloats.Count(), outFloats.Count());
		const uint32_t magAdjusted = ((maxMagnitude + 0x7f) << 23);

		const uint32_t fracBits = (1u << 23) - 1u;
		const uint32_t signBit = 1u << (23 + 8);
		const uint32_t fracSignMask = fracBits + signBit;

#ifdef RKIT_PLATFORM_ARCH_HAVE_SSE2
		const __m128i magAdjustedVector = _mm_set1_epi32(magAdjusted);
		const __m128i fracSignMaskVector = _mm_set1_epi32(fracSignMask);

		// Single-precision float
		const size_t numVectorFloats = numFloats / 4u * 4u;

		for (size_t startIndex = 0; startIndex < numVectorFloats; startIndex += 4u)
		{
			const __m128i bitsVector = _mm_loadu_si128(reinterpret_cast<const __m128i *>(inFloatsPtr + startIndex));
			const __m128i fracSignBits = _mm_and_si128(fracSignMaskVector, bitsVector);
			const __m128i expBits = _mm_andnot_si128(fracSignMaskVector, bitsVector);

			// This only needs SSE2
			const __m128i clampedExpBits = _mm_min_epi16(expBits, magAdjustedVector);
			const __m128i recombined = _mm_or_si128(fracSignBits, clampedExpBits);

			_mm_storeu_si128(reinterpret_cast<__m128i *>(outFloatsPtr + startIndex), recombined);
		}
#else
		const size_t numVectorFloats = 0;
#endif

		for (size_t i = numVectorFloats; i < numFloats; i++)
		{
			uint32_t floatBits = inFloatsPtr[i].GetBits();

			const uint32_t fracSignBits = (floatBits & fracSignMask);
			const uint32_t expBits = (floatBits & ~fracSignMask);
			const uint32_t clampedExpBits = rkit::Min(expBits, magAdjusted);
			const uint32_t recombined = (clampedExpBits | fracSignBits);

			memcpy(&outFloatsPtr[i], &recombined, 4);
		}
	}

	void UtilitiesDriver::SanitizeClampUInt16s(const Span<uint16_t> &outUInts, const Span<const endian::LittleUInt16_t> &inUInts, uint16_t maxValue) const
	{
		const endian::LittleUInt16_t *inUIntsPtr = inUInts.Ptr();
		uint16_t *outUIntsPtr = outUInts.Ptr();

		const size_t numUInts = rkit::Min(inUInts.Count(), outUInts.Count());

#if RKIT_PLATFORM_ARCH_HAVE_SSE41 != 0
		const __m128i maxValueVector = _mm_set1_epi16(maxValue);

		const size_t numVectorUInts = numUInts / 8u * 8u;

		for (size_t startIndex = 0; startIndex < numVectorUInts; startIndex += 8u)
		{
			const __m128i values = _mm_loadu_si128(reinterpret_cast<const __m128i *>(inUIntsPtr + startIndex));
			const __m128i adjusted = _mm_min_epu16(values, maxValueVector);
			_mm_storeu_si128(reinterpret_cast<__m128i *>(outUIntsPtr + startIndex), adjusted);
		}
#elif RKIT_PLATFORM_ARCH_HAVE_SSE2 != 0
		const __m128i signBitFlip = _mm_set1_epi16(-0x8000);
		const __m128i maxValueFlippedVector = _mm_set1_epi16(maxValue ^ 0x8000u);

		const size_t numVectorUInts = numUInts / 8u * 8u;

		for (size_t startIndex = 0; startIndex < numVectorUInts; startIndex += 8u)
		{
			const __m128i values = _mm_loadu_si128(reinterpret_cast<const __m128i *>(inUIntsPtr + startIndex));
			const __m128i valuesFlipped = _mm_xor_si128(signBitFlip, values);
			const __m128i adjusted = _mm_xor_si128(signBitFlip, _mm_min_epi16(valuesFlipped, maxValueFlippedVector));

			_mm_storeu_si128(reinterpret_cast<__m128i *>(outUIntsPtr + startIndex), adjusted);
		}
#else
		const size_t numVectorUInts = 0;
#endif

		for (size_t i = numVectorUInts; i < numUInts; i++)
		{
			uint16_t result = Min(maxValue, inUInts[i].Get());
			memcpy(&outUInts[i], &result, 2);
		}
	}
}


RKIT_IMPLEMENT_MODULE("RKit", "Utilities", ::rkit::UtilitiesModule)
