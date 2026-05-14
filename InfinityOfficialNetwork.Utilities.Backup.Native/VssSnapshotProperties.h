#pragma once

#include <vector>
#include <string>
#include <stdint.h>
#include <msclr/marshal_cppstd.h>

namespace InfinityOfficialNetwork::Utilities::Backup::Native {

	public enum class VssSnapshotState
	{
		Unknown = 0,
		Preparing = (Unknown + 1),
		ProcessingPrepare = (Preparing + 1),
		Prepared = (ProcessingPrepare + 1),
		ProcessingPrecommit = (Prepared + 1),
		Precommitted = (ProcessingPrecommit + 1),
		ProcessingCommit = (Precommitted + 1),
		Committed = (ProcessingCommit + 1),
		ProcessingPostCommit = (Committed + 1),
		ProcessingPreFinalCommit = (ProcessingPostCommit + 1),
		PreFinalCommitted = (ProcessingPreFinalCommit + 1),
		ProcessingPostFinalCommit = (PreFinalCommitted + 1),
		Created = (ProcessingPostFinalCommit + 1),
		Aborted = (Created + 1),
		Deleted = (Aborted + 1),
		PostCommitted = (Deleted + 1),
		Count = (PostCommitted + 1)
	};

	public ref class VssSnapshotProperties {
	public:
		property System::Guid SnapshotId;
		property System::Guid SnapshotSetId;
		property int64_t SnapshotsCount;
		property System::String^ SnapshotDeviceObject;
		property System::String^ OriginalVolumeName;
		property System::String^ OriginatingMachine;
		property System::String^ ServiceMachine;
		property System::String^ ExposedName;
		property System::String^ ExposedPath;
		property System::Guid ProviderId;
		property int64_t SnapshotAttributes;
		property System::DateTime CreationTimestamp;
		property VssSnapshotState Status;
	};
}