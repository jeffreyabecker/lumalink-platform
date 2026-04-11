module;

#include "lumalink/platform/Availability.h"
#include "lumalink/platform/ByteStream.h"
#include "lumalink/platform/FileSystem.h"
#include "lumalink/platform/MemoryFileAdapter.h"
#include "lumalink/platform/PathMapper.h"
#include "lumalink/platform/TransportInterfaces.h"
#include "lumalink/platform/TransportTraits.h"
#include "lumalink/platform/TransportFactory.h"
#include "lumalink/platform/ClockTypes.h"
#include "lumalink/platform/Clock.h"
#include "lumalink/platform/ManualClock.h"
#include "lumalink/platform/TimeSource.h"

export module lumalink.platform;

export namespace lumalink::platform::buffers
{
    using ::lumalink::platform::buffers::AvailabilityError;
    using ::lumalink::platform::buffers::AvailabilityErrorCode;
    using ::lumalink::platform::buffers::AvailabilityKind;
    using ::lumalink::platform::buffers::AvailableByteCount;
    using ::lumalink::platform::buffers::AvailableBytes;
    using ::lumalink::platform::buffers::ByteAvailability;
    using ::lumalink::platform::buffers::ConcatByteSource;
    using ::lumalink::platform::buffers::ErrorResult;
    using ::lumalink::platform::buffers::ExhaustedResult;
    using ::lumalink::platform::buffers::HasAvailabilityError;
    using ::lumalink::platform::buffers::HasAvailableBytes;
    using ::lumalink::platform::buffers::IByteChannel;
    using ::lumalink::platform::buffers::IByteSink;
    using ::lumalink::platform::buffers::IByteSource;
    using ::lumalink::platform::buffers::IsExhausted;
    using ::lumalink::platform::buffers::IsTemporarilyUnavailable;
    using ::lumalink::platform::buffers::LegacyAvailableFromResult;
    using ::lumalink::platform::buffers::MakeAvailabilityError;
    using ::lumalink::platform::buffers::PeekByte;
    using ::lumalink::platform::buffers::ReadAsStdString;
    using ::lumalink::platform::buffers::ReadAsVector;
    using ::lumalink::platform::buffers::ReadByte;
    using ::lumalink::platform::buffers::SingleByteSource;
    using ::lumalink::platform::buffers::SpanByteSource;
    using ::lumalink::platform::buffers::StdStringByteSource;
    using ::lumalink::platform::buffers::TemporarilyUnavailableResult;
    using ::lumalink::platform::buffers::VectorByteSource;
}

export namespace lumalink::platform::filesystem
{
    using ::lumalink::platform::filesystem::ContractName;
    using ::lumalink::platform::filesystem::DirectoryEntry;
    using ::lumalink::platform::filesystem::DirectoryEntryCallback;
    using ::lumalink::platform::filesystem::FileHandle;
    using ::lumalink::platform::filesystem::FileOpenMode;
    using ::lumalink::platform::filesystem::IFile;
    using ::lumalink::platform::filesystem::IFileSystem;
}

export namespace lumalink::platform::memory
{
    using ::lumalink::platform::memory::MemoryFileSystem;
}

export namespace lumalink::platform::transport
{
    using ::lumalink::platform::transport::HasStaticCreateClient;
    using ::lumalink::platform::transport::HasStaticCreatePeer;
    using ::lumalink::platform::transport::HasStaticCreateServer;
    using ::lumalink::platform::transport::HasStaticGetHostByName;
    using ::lumalink::platform::transport::IClient;
    using ::lumalink::platform::transport::IPeer;
    using ::lumalink::platform::transport::IServer;
    using ::lumalink::platform::transport::IsStaticTransportFactory;
    using ::lumalink::platform::transport::IsStaticTransportFactoryV;
    using ::lumalink::platform::transport::ITransportFactory;
}

export namespace lumalink::platform::time
{
    using ::lumalink::platform::time::IClock;
    using ::lumalink::platform::time::IMonotonicClock;
    using ::lumalink::platform::time::ISettableUtcClock;
    using ::lumalink::platform::time::IUtcClock;
    using ::lumalink::platform::time::IUtcSynchronizer;
    using ::lumalink::platform::time::IUtcTimeSource;
    using ::lumalink::platform::time::ManualClock;
    using ::lumalink::platform::time::MonotonicMillis;
    using ::lumalink::platform::time::OptionalUnixTime;
    using ::lumalink::platform::time::UnixTime;
    using ::lumalink::platform::time::UtcSynchronizer;
    using ::lumalink::platform::time::UtcTimeError;
    using ::lumalink::platform::time::UtcTimeFetchResult;
}

export namespace lumalink::platform
{
    using ::lumalink::platform::PosixPathMapper;
    using ::lumalink::platform::TransportFactoryWrapper;
    using ::lumalink::platform::WindowsPathMapper;
}
