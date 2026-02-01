#pragma once

#include "anox/AFSArchive.h"

#include "rkit/Core/NoCopy.h"
#include "rkit/Core/String.h"
#include "rkit/Core/SharedPtr.h"
#include "rkit/Core/Vector.h"

namespace rkit
{
	struct IMallocDriver;
	struct ISeekableReadStream;
	struct IMutexProtectedReadStream;

	template<class T>
	class UniquePtr;
}

namespace anox
{
	namespace afs
	{
		class Archive final : public IArchive, public rkit::NoCopy
		{
		public:
			explicit Archive(rkit::IMallocDriver *alloc);

			rkit::Result Open(rkit::UniquePtr<rkit::ISeekableReadStream> &&stream, bool allowBrokenFilePaths);

			FileHandle FindFile(const rkit::ByteStringSliceView &fileName, bool allowDirectories) const override;

		private:
			class DirectoryTreeBuilder;

			struct FileInfo
			{
				FileInfo();

				rkit::AsciiStringView m_fullPath;
				rkit::AsciiStringSliceView m_fileName;
				uint32_t m_filePosition;
				uint32_t m_compressedSize;
				uint32_t m_uncompressedSize;
			};

			struct DirectoryInfo
			{
				DirectoryInfo();
				
				rkit::AsciiStringSliceView m_fullPath;
				rkit::AsciiStringSliceView m_name;
				uint32_t m_firstFile;
				uint32_t m_numFiles;
				uint32_t m_firstSubDirectory;
				uint32_t m_numSubDirectories;
			};

			uint32_t GetNumFiles() const override;
			FileHandle GetFileByIndex(uint32_t fileIndex) const override;
			rkit::Result OpenFileByIndex(uint32_t fileIndex, rkit::UniquePtr<rkit::ISeekableReadStream> &outStream) const override;
			uint32_t GetFileSizeByIndex(uint32_t fileIndex) const override;
			rkit::AsciiStringView GetFilePathByIndex(uint32_t fileIndex) const override;
			rkit::AsciiStringSliceView GetDirectoryPathByIndex(uint32_t fileIndex) const override;

			FileHandle GetDirectoryByIndex(uint32_t dirIndex) const override;
			uint32_t GetDirectoryFirstFile(uint32_t dirIndex) const override;
			uint32_t GetDirectoryFirstSubDir(uint32_t dirIndex) const override;
			uint32_t GetDirectoryFileCount(uint32_t dirIndex) const override;
			uint32_t GetDirectorySubDirCount(uint32_t dirIndex) const override;

			static size_t FixBrokenFilePath(char *chars, size_t len);
			static rkit::Result CheckName(const rkit::Span<const char> &name);
			static rkit::Result CheckSlice(const rkit::Span<const char> &sliceName);

			rkit::SharedPtr<rkit::IMutexProtectedReadStream> m_stream;
			rkit::Vector<FileInfo> m_files;
			rkit::Vector<DirectoryInfo> m_directories;
			rkit::Vector<char> m_fileNameChars;

			rkit::IMallocDriver *m_alloc;
		};
	}
}
