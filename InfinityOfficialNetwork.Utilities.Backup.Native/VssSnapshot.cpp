#include "pch.h"
#include "VssSnapshot.h"

#include <functional>
#include <vcclr.h>
#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <windows.h>

#include "VssException.h"
#include "VssSnapshotSession.h"
#include "VssSnapshotSessionInternal.h"
#include "VssSnapshotVolume.h"
#include "Core.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "vssapi.lib")

using namespace InfinityOfficialNetwork::Utilities::Backup::Native;
using namespace System::Threading::Tasks;
using namespace System::Threading;
using namespace System;
using namespace Microsoft::Extensions::Logging;
using namespace Microsoft::Extensions::Logging::Abstractions;
using namespace System::Collections::Generic;
using namespace System::ComponentModel;
using namespace System::Runtime::InteropServices;
using namespace Microsoft::Win32::SafeHandles;

static inline void LaunchSnapshotDeleteThread(
	gcroot<TaskCompletionSource^> tcs,
	gcroot<Guid> snap,
	gcroot<ILogger^> logger,
	gcroot<CancellationToken> ct)
{
	std::thread worker([tcs,snap,logger,ct]() mutable {
		VssSnapshotSessionInternal::DeleteSnapshotWorker(
			static_cast<TaskCompletionSource^>(tcs),
			static_cast<Guid>(snap),
			static_cast<ILogger^>(logger),
			static_cast<CancellationToken>(ct)
		); });

	worker.detach();
}

VssSnapshotVolume^ VssSnapshot::OpenSnapshotVolume(VssSnapshotProperties^ snapshotProps)
{
	LoggerExtensions::LogInformation(session->logger, "VSS operation Open Snapshot Volume Handle {SnapshotId}", snapshotProps->SnapshotId);

	HANDLE hSnap = CreateFileW(msclr::interop::marshal_as<std::wstring>(snapshotProps->SnapshotDeviceObject).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL);

	if (hSnap == INVALID_HANDLE_VALUE)
		throw gcnew Win32Exception(Marshal::GetLastWin32Error(), "Failed to open handle to VSS snapshot volume");

	else
		return gcnew VssSnapshotVolume(snapshotProps, gcnew SafeFileHandle((IntPtr)hSnap, true), this->session);
}

inline VssSnapshot::~VssSnapshot() {
	if (!disposedValue)
	{
		DisposeAsync();
	}
	disposedValue = true;

	GC::SuppressFinalize(this);
}

ValueTask VssSnapshot::DisposeAsync()
{
	TaskCompletionSource^ tcs = gcnew TaskCompletionSource();

	LaunchSnapshotDeleteThread(tcs, SnapshotSetId, session->logger, CancellationToken::None);

	snapshotSetId = Guid(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	return ValueTask(tcs->Task);
}
