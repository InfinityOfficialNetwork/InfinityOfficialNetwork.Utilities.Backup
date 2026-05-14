#pragma once

namespace InfinityOfficialNetwork::Utilities::Backup::Native {

	ref class VssSnapshotVolume;
	ref class VssSnapshotSession;

	public ref class UsnJournalReader
	{
		VssSnapshotVolume^ volume;
		VssSnapshotSession^ session;

    internal:
        UsnJournalReader(VssSnapshotVolume^ volume, VssSnapshotSession^ session)
            : volume(volume), session(session)
        {
        }

	public:




        void ExtractAllRecords(InfinityOfficialNetwork::Utilities::Backup::Shared::Database::UsnJournal::UsnJournalScratchDbContext^ ctx);

        ~UsnJournalReader() {

            System::GC::SuppressFinalize(this);
        }

        !UsnJournalReader() {
            this->~UsnJournalReader();
        }
	};

}