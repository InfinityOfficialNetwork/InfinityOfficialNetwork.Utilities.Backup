#pragma once

#include <msclr/marshal.h>
#include <msclr/marshal_atl.h>
#include <msclr/marshal_cppstd.h>
#include <msclr/marshal_windows.h>
#include "VssSnapshotProperties.h"
#include "vss.h"

#include <map>

namespace msclr {
	namespace interop {

		template<>
		inline InfinityOfficialNetwork::Utilities::Backup::Native::VssSnapshotState marshal_as< InfinityOfficialNetwork::Utilities::Backup::Native::VssSnapshotState, VSS_SNAPSHOT_STATE>(const VSS_SNAPSHOT_STATE& from) {
			using namespace InfinityOfficialNetwork::Utilities::Backup::Native;

			static std::map<VSS_SNAPSHOT_STATE, short> map = {
				{VSS_SNAPSHOT_STATE::VSS_SS_UNKNOWN,					(short)VssSnapshotState::Unknown},
				{VSS_SNAPSHOT_STATE::VSS_SS_PREPARING,					(short)VssSnapshotState::Preparing},
				{VSS_SNAPSHOT_STATE::VSS_SS_PROCESSING_PREPARE,			(short)VssSnapshotState::ProcessingPrepare},
				{VSS_SNAPSHOT_STATE::VSS_SS_PREPARED,					(short)VssSnapshotState::Prepared},
				{VSS_SNAPSHOT_STATE::VSS_SS_PROCESSING_PRECOMMIT,		(short)VssSnapshotState::ProcessingPrecommit},
				{VSS_SNAPSHOT_STATE::VSS_SS_PRECOMMITTED,				(short)VssSnapshotState::Precommitted},
				{VSS_SNAPSHOT_STATE::VSS_SS_PROCESSING_COMMIT,			(short)VssSnapshotState::ProcessingCommit},
				{VSS_SNAPSHOT_STATE::VSS_SS_COMMITTED,					(short)VssSnapshotState::Committed},
				{VSS_SNAPSHOT_STATE::VSS_SS_PROCESSING_POSTCOMMIT,		(short)VssSnapshotState::ProcessingPostCommit},
				{VSS_SNAPSHOT_STATE::VSS_SS_PROCESSING_PREFINALCOMMIT,	(short)VssSnapshotState::ProcessingPreFinalCommit},
				{VSS_SNAPSHOT_STATE::VSS_SS_PREFINALCOMMITTED,			(short)VssSnapshotState::PreFinalCommitted},
				{VSS_SNAPSHOT_STATE::VSS_SS_PROCESSING_POSTFINALCOMMIT,	(short)VssSnapshotState::ProcessingPostFinalCommit},
				{VSS_SNAPSHOT_STATE::VSS_SS_CREATED,					(short)VssSnapshotState::Created},
				{VSS_SNAPSHOT_STATE::VSS_SS_ABORTED,					(short)VssSnapshotState::Aborted},
				{VSS_SNAPSHOT_STATE::VSS_SS_DELETED,					(short)VssSnapshotState::Deleted},
				{VSS_SNAPSHOT_STATE::VSS_SS_POSTCOMMITTED,				(short)VssSnapshotState::PostCommitted},
				{VSS_SNAPSHOT_STATE::VSS_SS_COUNT,						(short)VssSnapshotState::Count},
			};

			return (VssSnapshotState)map.at(from);
		}

		template<>
		inline InfinityOfficialNetwork::Utilities::Backup::Native::VssSnapshotProperties^ marshal_as<InfinityOfficialNetwork::Utilities::Backup::Native::VssSnapshotProperties^, VSS_SNAPSHOT_PROP>(const VSS_SNAPSHOT_PROP& from) {
			using namespace InfinityOfficialNetwork::Utilities::Backup::Native;
			VssSnapshotProperties^ props = gcnew VssSnapshotProperties();

			props->SnapshotId = msclr::interop::marshal_as<System::Guid>(from.m_SnapshotId);
			props->SnapshotSetId = msclr::interop::marshal_as<System::Guid>(from.m_SnapshotSetId);
			props->SnapshotsCount = (from.m_lSnapshotsCount);
			props->SnapshotDeviceObject = msclr::interop::marshal_as<System::String^>(from.m_pwszSnapshotDeviceObject);
			props->OriginalVolumeName = msclr::interop::marshal_as<System::String^>(from.m_pwszOriginalVolumeName);
			props->OriginatingMachine = msclr::interop::marshal_as<System::String^>(from.m_pwszOriginatingMachine);
			props->ServiceMachine = msclr::interop::marshal_as<System::String^>(from.m_pwszServiceMachine);
			props->ExposedName = msclr::interop::marshal_as<System::String^>(from.m_pwszExposedName);
			props->ExposedPath = msclr::interop::marshal_as<System::String^>(from.m_pwszExposedPath);
			props->ProviderId = msclr::interop::marshal_as<System::Guid>(from.m_ProviderId);
			props->SnapshotAttributes = (from.m_lSnapshotAttributes);
			props->CreationTimestamp = System::DateTime::FromFileTimeUtc(from.m_tsCreationTimestamp);
			props->Status = msclr::interop::marshal_as<VssSnapshotState>(from.m_eStatus);

			return props;
		}



		//template<>
		//inline VSS_SNAPSHOT_PROP marshal_as<VSS_SNAPSHOT_PROP, InfinityOfficialNetwork::Utilities::Backup::Native::VssSnapshotProperties>(const InfinityOfficialNetwork::Utilities::Backup::Native::VssSnapshotProperties& from) {

		//}
	}
}
