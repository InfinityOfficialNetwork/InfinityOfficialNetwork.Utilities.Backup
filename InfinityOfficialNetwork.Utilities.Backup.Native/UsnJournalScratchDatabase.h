#pragma once

namespace InfinityOfficialNetwork::Utilities::Backup::Native {

	//ref class UsnJournalScratchDatabaseImpl;

	public ref class UsnJournalScratchDatabase
	{
	private:
		System::String^ fileName;
		//UsnJournalScratchDatabaseImpl^ impl;

	public: 
		UsnJournalScratchDatabase();
	};

}