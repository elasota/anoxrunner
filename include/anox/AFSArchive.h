#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/Vector.h"
#include "rkit/Core/StringProto.h"

#include <stdint.h>

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct IReadStream;
}

namespace anox
{
	namespace afs
	{
		struct IArchive;

		class FileHandle
		{
		public:
			FileHandle();
			FileHandle(const IArchive *archive, uint32_t fileIndex, bool isDirectory);

			rkit::Result Open(rkit::UniquePtr<rkit::ISeekableReadStream> &outStream) const;
			bool IsValid() const;
			bool IsDirectory() const;
			uint32_t GetFileSize() const;
			rkit::StringView GetFilePath() const;
			rkit::StringSliceView GetDirectoryPath() const;

			uint32_t GetNumFiles() const;
			FileHandle GetFileByIndex(uint32_t index) const;
			uint32_t GetNumSubdirectories() const;
			FileHandle GetSubdirectoryByIndex(uint32_t index) const;

		private:
			const IArchive *m_archive;
			uint32_t m_fileIndex;
			bool m_isDirectory;
		};

		class ArchiveFileIterator
		{
		public:
			ArchiveFileIterator();
			ArchiveFileIterator(const IArchive *archive, uint32_t fileIndex);

			ArchiveFileIterator &operator++();
			ArchiveFileIterator operator++(int);

			bool operator==(const ArchiveFileIterator &other) const;
			bool operator!=(const ArchiveFileIterator &other) const;

			FileHandle operator*() const;

		private:
			const IArchive *m_archive;
			uint32_t m_fileIndex;
		};

		class ArchiveFileListView
		{
		public:
			explicit ArchiveFileListView(const IArchive *archive);

			ArchiveFileIterator begin();
			ArchiveFileIterator end();

		private:
			ArchiveFileListView() = delete;

			const IArchive *m_archive;
		};

		struct IArchive
		{
			friend class ArchiveFileListView;
			friend class FileHandle;

			virtual ~IArchive() {}

			virtual FileHandle FindFile(const rkit::StringSliceView &fileName, bool allowDirectories) const = 0;

			ArchiveFileListView GetFiles() const;
			FileHandle GetRootDirectory() const;

		protected:
			virtual uint32_t GetNumFiles() const = 0;
			virtual FileHandle GetFileByIndex(uint32_t fileIndex) const = 0;
			virtual rkit::Result OpenFileByIndex(uint32_t fileIndex, rkit::UniquePtr<rkit::ISeekableReadStream> &outStream) const = 0;
			virtual uint32_t GetFileSizeByIndex(uint32_t fileIndex) const = 0;
			virtual rkit::StringView GetFilePathByIndex(uint32_t fileIndex) const = 0;
			virtual rkit::StringSliceView GetDirectoryPathByIndex(uint32_t fileIndex) const = 0;

			virtual FileHandle GetDirectoryByIndex(uint32_t dirIndex) const = 0;
			virtual uint32_t GetDirectoryFirstFile(uint32_t dirIndex) const = 0;
			virtual uint32_t GetDirectoryFirstSubDir(uint32_t dirIndex) const = 0;
			virtual uint32_t GetDirectoryFileCount(uint32_t dirIndex) const = 0;
			virtual uint32_t GetDirectorySubDirCount(uint32_t dirIndex) const = 0;
		};
	}
}

#include "rkit/Core/Result.h"

inline anox::afs::FileHandle::FileHandle()
	: m_archive(nullptr)
	, m_fileIndex(0)
	, m_isDirectory(false)
{
}

inline anox::afs::FileHandle::FileHandle(const IArchive *archive, uint32_t fileIndex, bool isDirectory)
	: m_archive(archive)
	, m_fileIndex(fileIndex)
	, m_isDirectory(isDirectory)
{
}

inline rkit::Result anox::afs::FileHandle::Open(rkit::UniquePtr<rkit::ISeekableReadStream> &outStream) const
{
	if (m_archive == nullptr || m_isDirectory)
		return rkit::ResultCode::kFileOpenError;

	return m_archive->OpenFileByIndex(m_fileIndex, outStream);
}

inline bool anox::afs::FileHandle::IsValid() const
{
	return m_archive != nullptr;
}

inline uint32_t anox::afs::FileHandle::GetFileSize() const
{
	if (m_archive == nullptr || m_isDirectory)
		return 0;

	return m_archive->GetFileSizeByIndex(m_fileIndex);
}

inline bool anox::afs::FileHandle::IsDirectory() const
{
	return m_isDirectory;
}

inline rkit::StringView anox::afs::FileHandle::GetFilePath() const
{
	if (m_archive == nullptr)
		return rkit::StringView();

	RKIT_ASSERT(!m_isDirectory);

	return m_archive->GetFilePathByIndex(m_fileIndex);
}

inline rkit::StringSliceView anox::afs::FileHandle::GetDirectoryPath() const
{
	if (m_archive == nullptr)
		return rkit::StringSliceView();

	RKIT_ASSERT(m_isDirectory);

	return m_archive->GetDirectoryPathByIndex(m_fileIndex);
}

inline uint32_t anox::afs::FileHandle::GetNumFiles() const
{
	if (m_archive == nullptr || !m_isDirectory)
		return 0;

	return m_archive->GetDirectoryFileCount(m_fileIndex);
}

inline anox::afs::FileHandle anox::afs::FileHandle::GetFileByIndex(uint32_t index) const
{
	RKIT_ASSERT(m_isDirectory);

	return m_archive->GetFileByIndex(m_archive->GetDirectoryFirstFile(m_fileIndex) + index);
}

inline uint32_t anox::afs::FileHandle::GetNumSubdirectories() const
{
	if (m_archive == nullptr || !m_isDirectory)
		return 0;

	return m_archive->GetDirectoryFileCount(m_fileIndex);
}

inline anox::afs::FileHandle anox::afs::FileHandle::GetSubdirectoryByIndex(uint32_t index) const
{
	RKIT_ASSERT(m_isDirectory);

	return m_archive->GetDirectoryByIndex(m_archive->GetDirectoryFirstSubDir(m_fileIndex) + index);
}

inline anox::afs::ArchiveFileIterator::ArchiveFileIterator()
	: m_archive(nullptr)
	, m_fileIndex(0)
{
}

inline anox::afs::ArchiveFileIterator::ArchiveFileIterator(const IArchive *archive, uint32_t fileIndex)
	: m_archive(archive)
	, m_fileIndex(fileIndex)
{
}

inline anox::afs::ArchiveFileIterator &anox::afs::ArchiveFileIterator::operator++()
{
	++m_fileIndex;
	return *this;
}

inline anox::afs::ArchiveFileIterator anox::afs::ArchiveFileIterator::operator++(int)
{
	uint32_t oldIndex = m_fileIndex++;
	return ArchiveFileIterator(m_archive, oldIndex);
}

inline bool anox::afs::ArchiveFileIterator::operator==(const ArchiveFileIterator &other) const
{
	return m_archive == other.m_archive && m_fileIndex == other.m_fileIndex;
}

inline bool anox::afs::ArchiveFileIterator::operator!=(const ArchiveFileIterator &other) const
{
	return !((*this) == other);
}

inline anox::afs::FileHandle anox::afs::ArchiveFileIterator::operator*() const
{
	return FileHandle(m_archive, m_fileIndex, false);
}

inline anox::afs::ArchiveFileListView::ArchiveFileListView(const IArchive *archive)
	: m_archive(archive)
{
}

inline anox::afs::ArchiveFileIterator anox::afs::ArchiveFileListView::begin()
{
	return anox::afs::ArchiveFileIterator(m_archive, 0);
}

inline anox::afs::ArchiveFileIterator anox::afs::ArchiveFileListView::end()
{
	return anox::afs::ArchiveFileIterator(m_archive, m_archive->GetNumFiles());
}


inline anox::afs::ArchiveFileListView anox::afs::IArchive::GetFiles() const
{
	return anox::afs::ArchiveFileListView(this);
}


inline anox::afs::FileHandle anox::afs::IArchive::GetRootDirectory() const
{
	return this->GetDirectoryByIndex(0);
}
