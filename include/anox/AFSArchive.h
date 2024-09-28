#include "rkit/Core/Vector.h"
#include "rkit/Core/StringProto.h"

#include <stdint.h>

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct Result;
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
			FileHandle(const IArchive *archive, uint32_t fileIndex);

			rkit::Result Open(rkit::UniquePtr<rkit::IReadStream> &outStream) const;
			bool IsValid() const;
			uint32_t GetFileSize() const;
			rkit::StringView GetFilePath() const;

		private:
			const IArchive *m_archive;
			uint32_t m_fileIndex;
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

			virtual FileHandle FindFile(const rkit::StringView &fileName) const = 0;

			ArchiveFileListView GetFiles() const;

		protected:
			virtual uint32_t GetNumFiles() const = 0;
			virtual FileHandle GetFileByIndex(uint32_t fileIndex) const = 0;
			virtual rkit::Result OpenFileByIndex(uint32_t fileIndex, rkit::UniquePtr<rkit::IReadStream> &outStream) const = 0;
			virtual uint32_t GetFileSizeByIndex(uint32_t fileIndex) const = 0;
			virtual rkit::StringView GetFilePathByIndex(uint32_t fileIndex) const = 0;
		};
	}
}

#include "rkit/Core/Result.h"

inline anox::afs::FileHandle::FileHandle()
	: m_archive(nullptr)
	, m_fileIndex(0)
{
}

inline anox::afs::FileHandle::FileHandle(const IArchive *archive, uint32_t fileIndex)
	: m_archive(archive)
	, m_fileIndex(fileIndex)
{
}

inline rkit::Result anox::afs::FileHandle::Open(rkit::UniquePtr<rkit::IReadStream> &outStream) const
{
	if (m_archive == nullptr)
		return rkit::ResultCode::kFileOpenError;

	return m_archive->OpenFileByIndex(m_fileIndex, outStream);
}

inline bool anox::afs::FileHandle::IsValid() const
{
	return m_archive != nullptr;
}

inline uint32_t anox::afs::FileHandle::GetFileSize() const
{
	if (m_archive == nullptr)
		return 0;

	return m_archive->GetFileSizeByIndex(m_fileIndex);
}

inline rkit::StringView anox::afs::FileHandle::GetFilePath() const
{
	if (m_archive == nullptr)
		return rkit::StringView();

	return m_archive->GetFilePathByIndex(m_fileIndex);
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
	return FileHandle(m_archive, m_fileIndex);
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
