#pragma once

#include "VssSnapshot.h"
namespace InfinityOfficialNetwork::Utilities::Backup::Native {

	ref class VssSnapshotSession;

	ref class VssSnapshotSessionInternal {
	public:
		static void ThreadInit(Microsoft::Extensions::Logging::ILogger^ logger);

		static void ThreadDeinit(Microsoft::Extensions::Logging::ILogger^ logger);

		static void CreateSnapshotWorker(
			VssSnapshotSession^ session,
			System::Threading::Tasks::TaskCompletionSource<VssSnapshot^>^ tcs,
			System::Threading::CancellationToken ct,
			std::vector<std::wstring> volumes,
			Microsoft::Extensions::Logging::ILogger^ logger);

		static void DeleteSnapshotWorker(System::Threading::Tasks::TaskCompletionSource^ tcs, System::Guid snapshotSetId, Microsoft::Extensions::Logging::ILogger^ logger, System::Threading::CancellationToken ct);
	};
}