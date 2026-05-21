#pragma once

namespace InfinityOfficialNetwork::Utilities::Backup::Native {
	ref class UsnJournalFileAggregate;
	ref class VssSnapshot;
}

namespace InfinityOfficialNetwork::Utilities::Backup::Native::Streams {
	
	public ref class StreamExtractor abstract {
	public:
		virtual System::Collections::Generic::IEnumerable<System::IO::Stream^>^ ExtractAllStreams(VssSnapshot^ snapshot, UsnJournalFileAggregate^ fileAggregate) abstract;
	};
}
