#pragma once

#include "StreamExtractor.h"

namespace InfinityOfficialNetwork::Utilities::Backup::Native::Streams {


	ref class DataStreamExtractor : StreamExtractor
	{
	public:
		// Inherited via StreamExtractor
	virtual System::Collections::Generic::IEnumerable<System::IO::Stream^>^ ExtractAllStreams(VssSnapshot^ snapshot, UsnJournalFileAggregate^ fileAggregate) override;
	};


}

