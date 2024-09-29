#include "AnoxAFSArchive.h"

#include "anox/AFSFormat.h"

#include "rkit/Core/LogDriver.h"
#include "rkit/Core/SharedPtr.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/UtilitiesDriver.h"

namespace anox
{
	namespace afs
	{
		Archive::Archive(rkit::IMallocDriver *alloc)
			: m_files(alloc)
			, m_fileNameChars(alloc)
			, m_alloc(alloc)
		{
		}

		rkit::Result Archive::Open(rkit::UniquePtr<rkit::ISeekableReadStream> &&movedStream)
		{
			rkit::UniquePtr<rkit::ISeekableReadStream> stream(std::move(movedStream));

			rkit::FilePos_t archiveSize = stream->GetSize();

			anox::afs::HeaderData header;
			RKIT_CHECK(stream->SeekStart(0));
			RKIT_CHECK(stream->ReadAll(&header, sizeof(header)));

			if (header.m_magic.Get() != anox::afs::HeaderData::kAFSMagic || header.m_version.Get() != anox::afs::HeaderData::kAFSVersion)
			{
				rkit::log::Error("AFS file header was invalid");
				return rkit::ResultCode::kInvalidParameter;
			}

			uint32_t catalogSize = header.m_catalogSize.Get();

			if (catalogSize % sizeof(afs::FileData) != 0)
			{
				rkit::log::Error("AFS catalog size was invalid");
				return rkit::ResultCode::kInvalidParameter;
			}

			uint32_t numFiles = catalogSize / sizeof(afs::FileData);

			rkit::Vector<afs::FileData> fileDatas(m_alloc);
			RKIT_CHECK(fileDatas.Resize(numFiles));

			RKIT_CHECK(stream->SeekStart(header.m_catalogLocation.Get()));
			RKIT_CHECK(stream->ReadAll(fileDatas.GetBuffer(), catalogSize));

			size_t numFilePathChars = 0;
			for (const afs::FileData &fileData : fileDatas)
			{
				size_t filePathLen = strnlen(fileData.m_filePath, afs::FileData::kFileNameBufferSize);
				numFilePathChars += filePathLen + 1;
			}

			RKIT_CHECK(m_fileNameChars.Resize(numFilePathChars));
			RKIT_CHECK(m_files.Resize(numFiles));

			size_t filePathWritePos = 0;
			for (size_t fileIndex = 0; fileIndex < numFiles; fileIndex++)
			{
				FileInfo &fileInfo = m_files[fileIndex];
				const FileData &fileData = fileDatas[fileIndex];
				size_t filePathLen = strnlen(fileData.m_filePath, afs::FileData::kFileNameBufferSize);

				char *charsWriteLoc = m_fileNameChars.GetBuffer() + filePathWritePos;
				memcpy(charsWriteLoc, fileData.m_filePath, filePathLen);
				charsWriteLoc[filePathLen] = '\0';

				fileInfo.m_name = rkit::StringView(charsWriteLoc, filePathLen);

				RKIT_CHECK(CheckName(fileInfo.m_name));

				fileInfo.m_filePosition = fileData.m_location.Get();
				fileInfo.m_compressedSize = fileData.m_compressedSize.Get();
				fileInfo.m_uncompressedSize = fileData.m_uncompressedSize.Get();

				if (fileInfo.m_filePosition > archiveSize)
					return rkit::ResultCode::kMalformedFile;

				if (archiveSize - fileInfo.m_filePosition < fileInfo.m_compressedSize)
					return rkit::ResultCode::kMalformedFile;

				filePathWritePos += filePathLen + 1;
			}

			// Convert path separators
			for (char &c : m_fileNameChars)
			{
				if (c == '\\')
					c = '/';
			}

			// Finally wrap stream
			RKIT_CHECK(rkit::GetDrivers().m_utilitiesDriver->CreateMutexProtectedReadStream(m_stream, std::move(stream)));

			return rkit::ResultCode::kOK;
		}

		FileHandle Archive::FindFile(const rkit::StringView &fileName) const
		{
			size_t numFiles = m_files.Count();

			for (size_t i = 0; i < numFiles; i++)
			{
				const FileInfo &finfo = m_files[i];

				if (finfo.m_name == fileName)
					return FileHandle(this, static_cast<uint32_t>(i));
			}

			return FileHandle();
		}

		rkit::Result Archive::OpenFileByIndex(uint32_t fileIndex, rkit::UniquePtr<rkit::IReadStream> &outStream) const
		{
			rkit::IUtilitiesDriver *utils = rkit::GetDrivers().m_utilitiesDriver;

			const FileInfo &fileInfo = m_files[fileIndex];

			rkit::UniquePtr<rkit::ISeekableReadStream> mutualAccessorStream;
			RKIT_CHECK(m_stream->CreateReadStream(mutualAccessorStream));

			if (fileInfo.m_compressedSize > 0)
			{
				rkit::UniquePtr<rkit::IReadStream> sliceStream;
				RKIT_CHECK(utils->CreateRangeLimitedReadStream(sliceStream, std::move(mutualAccessorStream), fileInfo.m_filePosition, fileInfo.m_compressedSize));

				RKIT_CHECK(utils->CreateDeflateDecompressStream(outStream, std::move(sliceStream)));
			}
			else
			{
				RKIT_CHECK(utils->CreateRangeLimitedReadStream(outStream, std::move(mutualAccessorStream), fileInfo.m_filePosition, fileInfo.m_uncompressedSize));
			}

			return rkit::ResultCode::kOK;
		}

		uint32_t Archive::GetFileSizeByIndex(uint32_t fileIndex) const
		{
			return m_files[fileIndex].m_uncompressedSize;
		}

		rkit::StringView Archive::GetFilePathByIndex(uint32_t fileIndex) const
		{
			return m_files[fileIndex].m_name;
		}

		uint32_t Archive::GetNumFiles() const
		{
			return static_cast<uint32_t>(m_files.Count());
		}

		FileHandle Archive::GetFileByIndex(uint32_t fileIndex) const
		{
			return FileHandle(this, fileIndex);
		}

		Archive::FileInfo::FileInfo()
			: m_filePosition(0)
			, m_compressedSize(0)
			, m_uncompressedSize(0)
		{
		}

		rkit::Result Archive::CheckName(const rkit::StringView &name)
		{
			size_t sliceStart = 0;
			for (size_t i = 0; i < name.Length(); i++)
			{
				if (name[i] == '\\')
				{
					RKIT_CHECK(CheckSlice(name.SubString(sliceStart, i - sliceStart)));
					sliceStart = i + 1;
				}
			}

			RKIT_CHECK(CheckSlice(name.SubString(sliceStart, name.Length() - sliceStart)));

			return rkit::ResultCode::kOK;
		}

		rkit::Result Archive::CheckSlice(const rkit::StringView &sliceName)
		{
			if (sliceName.Length() == 0)
				return rkit::ResultCode::kMalformedFile;

			char firstChar = sliceName[0];
			char lastChar = sliceName[sliceName.Length() - 1];

			if (firstChar == ' ' || lastChar == ' ' || lastChar == '.')
				return rkit::ResultCode::kMalformedFile;

			const char *bannedNames[] =
			{
				"CON",
				"PRN",
				"AUX",
				"NUL",
				"COM1",
				"COM2",
				"COM3",
				"COM4",
				"COM5",
				"COM6",
				"COM7",
				"COM8",
				"COM9",
				"LPT1",
				"LPT2",
				"LPT3",
				"LPT4",
				"LPT5",
				"LPT6",
				"LPT7",
				"LPT8",
				"LPT9"
			};

			for (const char *bannedName : bannedNames)
			{
				if (sliceName == rkit::StringView(bannedName, strlen(bannedName)))
					return rkit::ResultCode::kMalformedFile;
			}

			for (size_t i = 0; i < sliceName.Length(); i++)
			{
				char c = sliceName[i];
				if (c == '.' && i > 0 && sliceName[i - 1] == '.')
					return rkit::ResultCode::kMalformedFile;

				bool isValidChar = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');

				if (!isValidChar)
				{
					const char *extChars = "_-. +~#";

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
					return rkit::ResultCode::kMalformedFile;
			}

			return rkit::ResultCode::kOK;
		}
	}
}
