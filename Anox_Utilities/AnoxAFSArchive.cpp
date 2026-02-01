#include "AnoxAFSArchive.h"

#include "anox/AFSFormat.h"

#include "rkit/Core/HashTable.h"
#include "rkit/Core/LogDriver.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/QuickSort.h"
#include "rkit/Core/SharedPtr.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/UtilitiesDriver.h"

#include <algorithm>

namespace anox
{
	namespace afs
	{
		class Archive::DirectoryTreeBuilder
		{
		public:
			static rkit::Result BuildFileTree(rkit::Vector<FileInfo> &files, rkit::Vector<DirectoryInfo> &directories);

		private:
			struct Directory
			{
				rkit::Vector<FileInfo> m_files;
				rkit::Vector<Directory> m_subDirectories;

				rkit::AsciiStringSliceView m_fullDirPath;
				rkit::AsciiStringSliceView m_name;
				rkit::HashMap<rkit::ByteStringSliceView, size_t> m_subDirectoriesByName;
				rkit::HashMap<rkit::ByteStringSliceView, size_t> m_filesByName;
			};

			static rkit::Result RecursiveInsertFile(Directory &dir, const FileInfo &file, size_t sliceStart);
			static rkit::Result InsertFile(Directory &dir, const FileInfo &file, const rkit::AsciiStringSliceView &nameSlice);
			static rkit::Result InsertDirectory(Directory &dir, Directory *&outDirectory, const rkit::AsciiStringSliceView &fullDirPath, const rkit::AsciiStringSliceView &nameSlice);
			static void RecursiveSortAndCountDirectories(Directory &dir, size_t &outNumFiles, size_t &outNumDirectories);
			static void RecursiveUnrollDirectory(rkit::Vector<FileInfo> &files, size_t &numFiles, rkit::Vector<DirectoryInfo> &directories, size_t &numDirectories, const Directory &dir, size_t placementIndex);

			static bool SortCompareFile(const FileInfo &a, const FileInfo &b);
			static bool SortCompareDirectory(const Directory &a, const Directory &b);
		};

		rkit::Result Archive::DirectoryTreeBuilder::BuildFileTree(rkit::Vector<FileInfo> &files, rkit::Vector<DirectoryInfo> &directories)
		{
			Directory rootDirectory;

			for (const FileInfo &file : files)
			{
				RKIT_CHECK(RecursiveInsertFile(rootDirectory, file, 0));
			}

			size_t numFiles = 0;
			size_t numDirectories = 1;
			RecursiveSortAndCountDirectories(rootDirectory, numFiles, numDirectories);

			files.Reset();

			RKIT_CHECK(files.Resize(numFiles));
			RKIT_CHECK(directories.Resize(numDirectories));

			numFiles = 0;
			numDirectories = 1;
			RecursiveUnrollDirectory(files, numFiles, directories, numDirectories, rootDirectory, 0);

			return rkit::ResultCode::kOK;
		}

		void Archive::DirectoryTreeBuilder::RecursiveSortAndCountDirectories(Directory &dir, size_t &outNumFiles, size_t &outNumDirectories)
		{
			rkit::QuickSort(dir.m_files.begin(), dir.m_files.end(), SortCompareFile);
			rkit::QuickSort(dir.m_subDirectories.begin(), dir.m_subDirectories.end(), SortCompareDirectory);

			outNumFiles += dir.m_files.Count();
			outNumDirectories += dir.m_subDirectories.Count();

			for (Directory &subDir : dir.m_subDirectories)
				RecursiveSortAndCountDirectories(subDir, outNumFiles, outNumDirectories);
		}

		void Archive::DirectoryTreeBuilder::RecursiveUnrollDirectory(rkit::Vector<FileInfo> &files, size_t &numFiles, rkit::Vector<DirectoryInfo> &directories, size_t &numDirectories, const Directory &dir, size_t placementIndex)
		{
			const size_t firstFile = numFiles;
			const size_t firstDir = numDirectories;
			const size_t dirNumFiles = dir.m_files.Count();
			const size_t dirNumSubDirs = dir.m_subDirectories.Count();

			DirectoryInfo &dirInfo = directories[placementIndex];

			dirInfo.m_firstFile = static_cast<uint32_t>(firstFile);
			dirInfo.m_numFiles = static_cast<uint32_t>(dirNumFiles);
			dirInfo.m_firstSubDirectory = static_cast<uint32_t>(firstDir);
			dirInfo.m_numSubDirectories = static_cast<uint32_t>(dirNumSubDirs);
			dirInfo.m_fullPath = dir.m_fullDirPath;
			dirInfo.m_name = dir.m_name;

			numFiles += dirNumFiles;
			numDirectories += dirNumSubDirs;

			for (size_t i = 0; i < dirNumFiles; i++)
				files[i + firstFile] = dir.m_files[i];

			for (size_t i = 0; i < dirNumSubDirs; i++)
				RecursiveUnrollDirectory(files, numFiles, directories, numDirectories, dir.m_subDirectories[i], firstDir + i);
		}

		bool Archive::DirectoryTreeBuilder::SortCompareFile(const FileInfo &a, const FileInfo &b)
		{
			return a.m_fileName < b.m_fileName;
		}

		bool Archive::DirectoryTreeBuilder::SortCompareDirectory(const Directory &a, const Directory &b)
		{
			return a.m_name < b.m_name;
		}

		rkit::Result Archive::DirectoryTreeBuilder::RecursiveInsertFile(Directory &dir, const FileInfo &file, size_t sliceStart)
		{
			rkit::AsciiStringView fullName = file.m_fullPath;
			rkit::Optional<size_t> slashPos;

			for (size_t i = sliceStart; i < fullName.Length(); i++)
			{
				if (fullName[i] == '/')
				{
					slashPos = i;
					break;
				}
			}

			if (slashPos.IsSet())
			{
				Directory *subDirectory = nullptr;
				RKIT_CHECK(InsertDirectory(dir, subDirectory, fullName.SubString(0, slashPos.Get()), fullName.SubString(sliceStart, slashPos.Get() - sliceStart)));
				RKIT_CHECK(RecursiveInsertFile(*subDirectory, file, slashPos.Get() + 1));
			}
			else
			{
				FileInfo filledFile = file;
				filledFile.m_fileName = fullName.SubString(sliceStart);
				RKIT_CHECK(InsertFile(dir, filledFile, fullName.SubString(sliceStart)));
			}

			return rkit::ResultCode::kOK;
		}

		rkit::Result Archive::DirectoryTreeBuilder::InsertFile(Directory &dir, const FileInfo &file, const rkit::AsciiStringSliceView &nameSlice)
		{
			if (nameSlice.Length() == 0)
				return rkit::ResultCode::kDataError;

			rkit::HashValue_t nameHash = rkit::Hasher<rkit::ByteStringSliceView>::ComputeHash(0, nameSlice.RemoveEncoding());

			if (dir.m_subDirectoriesByName.FindPrehashed(nameHash, nameSlice.RemoveEncoding()) != dir.m_subDirectoriesByName.end())
				return rkit::ResultCode::kDataError;

			rkit::HashMap<rkit::ByteStringSliceView, size_t>::ConstIterator_t it = dir.m_filesByName.FindPrehashed(nameHash, nameSlice.RemoveEncoding());

			if (it != dir.m_filesByName.end())
				return rkit::ResultCode::kDataError;

			// New file
			size_t fileIndex = dir.m_files.Count();

			RKIT_CHECK(dir.m_files.Append(file));

			RKIT_CHECK(dir.m_filesByName.SetPrehashed(nameHash, nameSlice.RemoveEncoding(), fileIndex));

			return rkit::ResultCode::kOK;
		}

		rkit::Result Archive::DirectoryTreeBuilder::InsertDirectory(Directory &dir, Directory *&outDirectory, const rkit::AsciiStringSliceView &fullDirPath, const rkit::AsciiStringSliceView &nameSlice)
		{
			if (nameSlice.Length() == 0)
				return rkit::ResultCode::kDataError;

			rkit::HashValue_t nameHash = rkit::Hasher<rkit::ByteStringSliceView>::ComputeHash(0, nameSlice.RemoveEncoding());

			if (dir.m_filesByName.FindPrehashed(nameHash, nameSlice.RemoveEncoding()) != dir.m_filesByName.end())
				return rkit::ResultCode::kDataError;

			rkit::HashMap<rkit::ByteStringSliceView, size_t>::ConstIterator_t it = dir.m_subDirectoriesByName.FindPrehashed(nameHash, nameSlice.RemoveEncoding());

			if (it != dir.m_subDirectoriesByName.end())
			{
				outDirectory = &dir.m_subDirectories[it.Value()];
				return rkit::ResultCode::kOK;
			}

			// New directory
			size_t dirIndex = dir.m_subDirectories.Count();
			RKIT_CHECK(dir.m_subDirectories.Append(Directory()));

			Directory *newDir = &dir.m_subDirectories[dirIndex];
			newDir->m_fullDirPath = fullDirPath;
			newDir->m_name = nameSlice;

			RKIT_CHECK(dir.m_subDirectoriesByName.SetPrehashed(nameHash, nameSlice.RemoveEncoding(), dirIndex));

			outDirectory = newDir;
			return rkit::ResultCode::kOK;
		}

		Archive::Archive(rkit::IMallocDriver *alloc)
			: m_files(alloc)
			, m_fileNameChars(alloc)
			, m_alloc(alloc)
		{
		}

		rkit::Result Archive::Open(rkit::UniquePtr<rkit::ISeekableReadStream> &&movedStream, bool allowBrokenFilePaths)
		{
			rkit::UniquePtr<rkit::ISeekableReadStream> stream(std::move(movedStream));

			rkit::FilePos_t archiveSize = stream->GetSize();

			anox::afs::HeaderData header;
			RKIT_CHECK(stream->SeekStart(0));
			RKIT_CHECK(stream->ReadAll(&header, sizeof(header)));

			if (header.m_magic.Get() != anox::afs::HeaderData::kAFSMagic || header.m_version.Get() != anox::afs::HeaderData::kAFSVersion)
			{
				rkit::log::Error(u8"AFS file header was invalid");
				return rkit::ResultCode::kInvalidParameter;
			}

			uint32_t catalogSize = header.m_catalogSize.Get();

			if (catalogSize % sizeof(afs::FileData) != 0)
			{
				rkit::log::Error(u8"AFS catalog size was invalid");
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

				if (!allowBrokenFilePaths)
					filePathLen = FixBrokenFilePath(charsWriteLoc, filePathLen);

				charsWriteLoc[filePathLen] = '\0';

				if (!rkit::CharacterEncodingValidator<rkit::CharacterEncoding::kASCII>::ValidateSpan(rkit::Span<const char>(charsWriteLoc, filePathLen)))
				{
					rkit::log::Error(u8"Archive file name had invalid ASCII characters");
					return rkit::ResultCode::kInvalidUnicode;
				}

				fileInfo.m_fullPath = rkit::AsciiStringView(charsWriteLoc, filePathLen);

				if (!allowBrokenFilePaths)
				{
					RKIT_CHECK(CheckName(fileInfo.m_fullPath.ToSpan()));
				}

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

			RKIT_CHECK(DirectoryTreeBuilder::BuildFileTree(m_files, m_directories));

			// Finally wrap stream
			RKIT_CHECK(rkit::GetDrivers().m_utilitiesDriver->CreateMutexProtectedReadStream(m_stream, std::move(stream)));

			return rkit::ResultCode::kOK;
		}

		FileHandle Archive::FindFile(const rkit::ByteStringSliceView &fileName, bool allowDirectories) const
		{
			size_t numFiles = m_files.Count();

			for (size_t i = 0; i < numFiles; i++)
			{
				const FileInfo &finfo = m_files[i];

				if (finfo.m_fullPath.RemoveEncoding() == fileName)
					return FileHandle(this, static_cast<uint32_t>(i), false);
			}

			return FileHandle();
		}

		rkit::Result Archive::OpenFileByIndex(uint32_t fileIndex, rkit::UniquePtr<rkit::ISeekableReadStream> &outStream) const
		{
			rkit::IUtilitiesDriver *utils = rkit::GetDrivers().m_utilitiesDriver;

			const FileInfo &fileInfo = m_files[fileIndex];

			rkit::UniquePtr<rkit::ISeekableReadStream> mutualAccessorStream;
			RKIT_CHECK(m_stream->CreateReadStream(mutualAccessorStream));

			if (fileInfo.m_compressedSize > 0)
			{
				rkit::UniquePtr<rkit::ISeekableReadStream> sliceStream;
				RKIT_CHECK(utils->CreateRangeLimitedReadStream(sliceStream, std::move(mutualAccessorStream), fileInfo.m_filePosition, fileInfo.m_compressedSize));

				RKIT_CHECK(utils->CreateRestartableDeflateDecompressStream(outStream, std::move(sliceStream), fileInfo.m_uncompressedSize));
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

		rkit::AsciiStringView Archive::GetFilePathByIndex(uint32_t fileIndex) const
		{
			return m_files[fileIndex].m_fullPath;
		}

		rkit::AsciiStringSliceView Archive::GetDirectoryPathByIndex(uint32_t fileIndex) const
		{
			return m_directories[fileIndex].m_fullPath;
		}

		FileHandle Archive::GetDirectoryByIndex(uint32_t dirIndex) const
		{
			return FileHandle(this, dirIndex, true);
		}

		uint32_t Archive::GetDirectoryFirstFile(uint32_t dirIndex) const
		{
			return m_directories[dirIndex].m_firstFile;
		}

		uint32_t Archive::GetDirectoryFirstSubDir(uint32_t dirIndex) const
		{
			return m_directories[dirIndex].m_firstSubDirectory;
		}

		uint32_t Archive::GetDirectoryFileCount(uint32_t dirIndex) const
		{
			return m_directories[dirIndex].m_numFiles;
		}

		uint32_t Archive::GetDirectorySubDirCount(uint32_t dirIndex) const
		{
			return m_directories[dirIndex].m_numSubDirectories;
		}


		uint32_t Archive::GetNumFiles() const
		{
			return static_cast<uint32_t>(m_files.Count());
		}

		FileHandle Archive::GetFileByIndex(uint32_t fileIndex) const
		{
			return FileHandle(this, fileIndex, false);
		}

		Archive::FileInfo::FileInfo()
			: m_filePosition(0)
			, m_compressedSize(0)
			, m_uncompressedSize(0)
		{
		}

		Archive::DirectoryInfo::DirectoryInfo()
			: m_firstFile(0)
			, m_numFiles(0)
			, m_firstSubDirectory(0)
			, m_numSubDirectories(0)
		{
		}

		size_t Archive::FixBrokenFilePath(char *chars, size_t len)
		{
			for (size_t i = 0; i < len - 1; )
			{
				if ((chars[i] == '/' || chars[i] == '\\') && chars[i + 1] == ' ')
				{
					for (size_t j = i + 1; j < len - 1; j++)
						chars[j] = chars[j + 1];

					len--;
				}
				else
					i++;
			}

			return len;
		}

		rkit::Result Archive::CheckName(const rkit::Span<const char> &name)
		{
			size_t sliceStart = 0;
			for (size_t i = 0; i < name.Count(); i++)
			{
				if (name[i] == '\\')
				{
					RKIT_CHECK(CheckSlice(name.SubSpan(sliceStart, i - sliceStart)));
					sliceStart = i + 1;
				}
			}

			RKIT_CHECK(CheckSlice(name.SubSpan(sliceStart, name.Count() - sliceStart)));

			return rkit::ResultCode::kOK;
		}

		rkit::Result Archive::CheckSlice(const rkit::Span<const char> &sliceName)
		{
			if (sliceName.Count() == 0)
				return rkit::ResultCode::kMalformedFile;

			char firstChar = sliceName[0];
			char lastChar = sliceName[sliceName.Count() - 1];

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
				size_t bannedNameLength = strlen(bannedName);
				if (sliceName.Count() == bannedNameLength && !memcmp(bannedName, sliceName.Ptr(), bannedNameLength))
					return rkit::ResultCode::kMalformedFile;
			}

			for (size_t i = 0; i < sliceName.Count(); i++)
			{
				char c = sliceName[i];
				if (c == '.' && i > 0 && sliceName[i - 1] == '.')
					return rkit::ResultCode::kMalformedFile;

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
					return rkit::ResultCode::kMalformedFile;
			}

			return rkit::ResultCode::kOK;
		}
	}
}
