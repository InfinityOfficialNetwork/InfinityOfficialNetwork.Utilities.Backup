#include "pch.h"
#include "VssSnapshotVolume.h"
#include "UsnJournalReader.h"

using namespace InfinityOfficialNetwork::Utilities::Backup::Native;

UsnJournalReader^ VssSnapshotVolume::OpenUsnJournalReader()
{
    return gcnew UsnJournalReader(this, session);
}
