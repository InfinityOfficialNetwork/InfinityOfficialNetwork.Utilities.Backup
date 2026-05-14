#include "pch.h"
#include "VssSnapshotSession.h"

#include <chrono>
#include <thread>
#include <functional>
#include <vcclr.h>
#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <windows.h>

#include "VssException.h"
#include "VssSnapshotSessionInternal.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "vssapi.lib")

using namespace InfinityOfficialNetwork::Utilities::Backup::Native;
using namespace System::Threading::Tasks;
using namespace System::Threading;
using namespace System;
using namespace Microsoft::Extensions::Logging;
using namespace System::Collections::Generic;




static inline void LaunchSnapshotThread(
	gcroot<VssSnapshotSession^> session,
	gcroot<TaskCompletionSource<VssSnapshot^>^> tcs,
	gcroot<CancellationToken> ct,
	std::vector<std::wstring> volumes,
	gcroot<ILogger^> logger)
{
	std::thread worker([session, tcs, ct, volumes, logger]() mutable {
		VssSnapshotSessionInternal::CreateSnapshotWorker(
			static_cast<VssSnapshotSession^>(session),
			static_cast<TaskCompletionSource<VssSnapshot^>^>(tcs),
			static_cast<CancellationToken>(ct),
			volumes,
			static_cast<ILogger^>(logger)
		); });

	worker.detach();
}

Task<VssSnapshot^>^ VssSnapshotSession::CreateSnapshotAsync(CancellationToken ct) {
	TaskCompletionSource<VssSnapshot^>^ tcs = gcnew TaskCompletionSource<VssSnapshot^>();

	LaunchSnapshotThread(this, tcs, ct, this->volumes->get(), this->logger);
	return tcs->Task;
}