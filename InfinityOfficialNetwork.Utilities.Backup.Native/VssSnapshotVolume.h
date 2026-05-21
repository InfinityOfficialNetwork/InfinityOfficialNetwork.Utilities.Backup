#pragma once

#include "VssSnapshotProperties.h"

namespace InfinityOfficialNetwork::Utilities::Backup::Native {

	ref class VssSnapshotSession;
	ref class UsnJournalReader;
	ref class FileHandle;

	public ref class VssSnapshotVolume
	{
		VssSnapshotProperties^ props;
		Microsoft::Win32::SafeHandles::SafeFileHandle^ snapshotHandle;
	internal:
		VssSnapshotSession^ session;

	internal:
		VssSnapshotVolume(VssSnapshotProperties^ props, Microsoft::Win32::SafeHandles::SafeFileHandle^ snapshotHandle, VssSnapshotSession^ session) : props(props), snapshotHandle(snapshotHandle), session(session)
		{
		}

	public:
		property VssSnapshotProperties^ SnapshotProperties {
			VssSnapshotProperties^ get() { return props; }
		}

		property Microsoft::Win32::SafeHandles::SafeFileHandle^ SnapshotVolumeHandle {
			Microsoft::Win32::SafeHandles::SafeFileHandle^ get() { return snapshotHandle; }
		}

		UsnJournalReader^ OpenUsnJournalReader();

		~VssSnapshotVolume()
		{
			delete snapshotHandle;
		}

		FileHandle^ OpenFileById(System::ValueTuple<System::UInt128, bool> fileId);
	};

}