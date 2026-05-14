#pragma once

#include "VssSnapshotProperties.h"

namespace InfinityOfficialNetwork::Utilities::Backup::Native {

	ref class VssSnapshotSession;
	ref class VssSnapshotVolume;

	public ref class VssSnapshot : System::IAsyncDisposable
	{
	private:
		System::Collections::Generic::List<VssSnapshotProperties^>^ snapshots;
		System::Guid snapshotSetId;
		VssSnapshotSession^ session;

		bool disposedValue = false;

	public:
		VssSnapshot(System::Collections::Generic::List<VssSnapshotProperties^>^ snapshots, System::Guid snapshotSetId, VssSnapshotSession^ session) : snapshots(snapshots), snapshotSetId(snapshotSetId), session(session) {}

		property System::Collections::Generic::IReadOnlyList<VssSnapshotProperties^>^ Snapshots {
			System::Collections::Generic::IReadOnlyList<VssSnapshotProperties^>^ get() {
				return snapshots;
			}
		}

		property System::Guid SnapshotSetId {
			System::Guid get() {
				return snapshotSetId;
			}
		}

		VssSnapshotVolume^ OpenSnapshotVolume(VssSnapshotProperties^ snapshotProps);

		~VssSnapshot();

		!VssSnapshot() {
			this->~VssSnapshot();
		}

		// Inherited via IAsyncDisposable
		virtual System::Threading::Tasks::ValueTask DisposeAsync();
	};

}