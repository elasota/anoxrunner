#include "rkit/Core/DriverModuleStub.h"
#include "rkit/Core/UtilitiesDriver.h"
#include "rkit/Core/ModuleGlue.h"
#include "rkit/Core/NewDelete.h"
#include "rkit/Core/String.h"
#include "rkit/Core/Vector.h"

#include "DeflateDecompressStream.h"
#include "Json.h"
#include "MutexProtectedStream.h"
#include "RangeLimitedReadStream.h"
#include "Sha2Calculator.h"
#include "TextParser.h"

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

		Result InitDriver();
		void ShutdownDriver();

		Result CreateJsonDocument(UniquePtr<utils::IJsonDocument> &outDocument, IMallocDriver *alloc, IReadStream *readStream) const override;

		Result CreateMutexProtectedReadWriteStream(SharedPtr<IMutexProtectedReadWriteStream> &outStream, UniquePtr<ISeekableReadWriteStream> &&stream) const override;
		Result CreateMutexProtectedReadStream(SharedPtr<IMutexProtectedReadStream> &outStream, UniquePtr<ISeekableReadStream> &&stream) const override;
		Result CreateMutexProtectedWriteStream(SharedPtr<IMutexProtectedWriteStream> &outStream, UniquePtr<ISeekableWriteStream> &&stream) const override;

		Result CreateDeflateDecompressStream(UniquePtr<IReadStream> &outStream, UniquePtr<IReadStream> &&compressedStream) const override;
		Result CreateRangeLimitedReadStream(UniquePtr<IReadStream> &outStream, UniquePtr<ISeekableReadStream> &&stream, FilePos_t startPos, FilePos_t size) const override;

		HashValue_t ComputeHash(HashValue_t baseHash, const void *data, size_t size) const override;

		Result CreateTextParser(const Span<const char> &contents, utils::TextParserCommentType commentType, utils::TextParserLexerType lexType, UniquePtr<utils::ITextParser> &outParser) const override;
		Result ReadEntireFile(ISeekableReadStream &stream, Vector<uint8_t> &outBytes) const override;

		bool ValidateFilePath(const Span<const char> &fileName) const override;

		void NormalizeFilePath(const Span<char> &chars) const override;
		bool FindFilePathExtension(const StringView &str, StringView &outExt) const override;

		Result EscapeCStringInPlace(const Span<char> &chars, size_t &outNewLength) const override;

		const utils::ISha256Calculator *GetSha256Calculator() const override;

		Result VFormatString(char *buffer, size_t bufferSize, void *oversizedUserdata, AllocateDynamicStringCallback_t oversizedCallback, size_t &outLength, const char *fmt, va_list list) const override;

		Result SetProgramName(const StringView &str) override;
		StringView GetProgramName() const override;

		void SetProgramVersion(uint32_t major, uint32_t minor, uint32_t patch) override;
		void GetProgramVersion(uint32_t &outMajor, uint32_t &outMinor, uint32_t &outPatch) const override;

		void GetRKitVersion(uint32_t &outMajor, uint32_t &outMinor, uint32_t &outPatch) const override;

	private:
		static bool ValidateFilePathSlice(const Span<const char> &name);

		utils::Sha256Calculator m_sha256Calculator;

		String m_programName;

		uint32_t m_programVersion[3];
	};

	typedef DriverModuleStub<UtilitiesDriver, IUtilitiesDriver, &Drivers::m_utilitiesDriver> UtilitiesModule;

	UtilitiesDriver::UtilitiesDriver()
		: m_programVersion{ 1, 0, 0 }
	{
	}

	Result UtilitiesDriver::InitDriver()
	{
		return ResultCode::kOK;
	}

	void UtilitiesDriver::ShutdownDriver()
	{
	}

	Result UtilitiesDriver::CreateJsonDocument(UniquePtr<utils::IJsonDocument> &outDocument, IMallocDriver *alloc, IReadStream *readStream) const
	{
		return utils::CreateJsonDocument(outDocument, alloc, readStream);
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


	HashValue_t UtilitiesDriver::ComputeHash(HashValue_t baseHash, const void *value, size_t size) const
	{
		const uint8_t *bytes = static_cast<const uint8_t *>(value);

		// TODO: Improve this
		HashValue_t hash = baseHash;
		for (size_t i = 0; i < size; i++)
			hash = hash * 223u + bytes[i] * 4447u;

		return hash;
	}

	Result UtilitiesDriver::CreateTextParser(const Span<const char> &contents, utils::TextParserCommentType commentType, utils::TextParserLexerType lexType, UniquePtr<utils::ITextParser> &outParser) const
	{
		UniquePtr<utils::TextParserBase> parser;
		RKIT_CHECK(utils::TextParserBase::Create(contents, commentType, lexType, parser));

		outParser = std::move(parser);

		return ResultCode::kOK;
	}

	Result UtilitiesDriver::ReadEntireFile(ISeekableReadStream &stream, Vector<uint8_t> &outBytes) const
	{
		FilePos_t fileSizeRemaining = stream.GetSize() - stream.Tell();

		if (fileSizeRemaining == 0)
			outBytes.Reset();
		else
		{
			if (fileSizeRemaining > std::numeric_limits<size_t>::max())
				return ResultCode::kOutOfMemory;

			RKIT_CHECK(outBytes.Resize(static_cast<size_t>(fileSizeRemaining)));

			RKIT_CHECK(stream.ReadAll(outBytes.GetBuffer(), static_cast<size_t>(fileSizeRemaining)));
		}

		return ResultCode::kOK;
	}

	bool UtilitiesDriver::ValidateFilePathSlice(const rkit::Span<const char> &sliceName)
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

	bool UtilitiesDriver::ValidateFilePath(const Span<const char> &name) const
	{
		size_t sliceStart = 0;
		for (size_t i = 0; i < name.Count(); i++)
		{
			if (name[i] == '/')
			{
				if (!ValidateFilePathSlice(name.SubSpan(sliceStart, i - sliceStart)))
					return false;

				sliceStart = i + 1;
			}
		}

		return ValidateFilePathSlice(name.SubSpan(sliceStart, name.Count() - sliceStart));
	}

	void UtilitiesDriver::NormalizeFilePath(const Span<char> &chars) const
	{
		for (char &ch : chars)
		{
			if (ch == '\\')
				ch = '/';
			else if (ch >= 'A' && ch <= 'Z')
				ch = ch - 'A' + 'a';
		}
	}

	bool UtilitiesDriver::FindFilePathExtension(const StringView &str, StringView &outExt) const
	{
		for (size_t ri = 0; ri < str.Length(); ri++)
		{
			size_t i = str.Length() - 1 - ri;

			if (str[i] == '.')
			{
				outExt = StringView(str.GetChars() + i + 1, ri);
				return true;
			}
		}

		return false;
	}

	Result UtilitiesDriver::EscapeCStringInPlace(const Span<char> &charsRef, size_t &outNewLength) const
	{
		Span<char> span = charsRef;

		if (span.Count() < 2)
			return ResultCode::kInvalidCString;

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
					return ResultCode::kInvalidCString;

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
					return ResultCode::kInvalidCString;

				span[outPos++] = c;
			}
			else
				span[outPos++] = c;
		}

		outNewLength = outPos;

		return ResultCode::kOK;
	}

	const utils::ISha256Calculator *UtilitiesDriver::GetSha256Calculator() const
	{
		return &m_sha256Calculator;
	}

	Result UtilitiesDriver::VFormatString(char *buffer, size_t bufferSize, void *oversizedUserdata, AllocateDynamicStringCallback_t oversizedCallback, size_t &outLength, const char *fmt, va_list list) const
	{
		if (bufferSize > std::numeric_limits<int>::max())
			bufferSize = std::numeric_limits<int>::max();

		va_list firstAttemptList;
		va_copy(firstAttemptList, list);

		int formattedLength = vsnprintf(buffer, bufferSize, fmt, firstAttemptList);
		va_end(firstAttemptList);

		if (formattedLength < 0 || formattedLength == std::numeric_limits<int>::max())
			return ResultCode::kFormatError;

		if (static_cast<size_t>(formattedLength) < bufferSize)
		{
			outLength = static_cast<size_t>(formattedLength);
			return ResultCode::kOK;
		}

		void *newBuffer = nullptr;
		RKIT_CHECK(oversizedCallback(oversizedUserdata, static_cast<size_t>(formattedLength), newBuffer));

		va_list secondAttemptList;
		va_copy(secondAttemptList, list);

		int formattedLength2 = vsnprintf(static_cast<char *>(newBuffer), static_cast<size_t>(formattedLength) + 1, fmt, secondAttemptList);

		va_end(secondAttemptList);

		if (formattedLength2 != formattedLength)
			return ResultCode::kInternalError;

		outLength = static_cast<size_t>(formattedLength);
		return ResultCode::kOK;
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
}


RKIT_IMPLEMENT_MODULE("RKit", "Utilities", ::rkit::UtilitiesModule)
