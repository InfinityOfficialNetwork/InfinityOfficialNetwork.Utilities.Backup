#pragma once

namespace InfinityOfficialNetwork::Utilities::Backup::Native {

	ref class VssSnapshotVolume;

	public ref class FileHandle
	{
		VssSnapshotVolume^ volume;
		System::ValueTuple<System::UInt128, bool> fileId;
	
	internal:
		FileHandle(VssSnapshotVolume^ volume, const System::ValueTuple<System::UInt128, bool>& fileId)
			: volume(volume), fileId(fileId)
		{}

	public:
		System::IO::Stream^ OpenDataStream();
		System::IO::Stream^ OpenAlternateDataStream(System::String^ name);
		System::IO::Stream^ OpenSecurityInformation();
		System::IO::Stream^ OpenExtendedAttributes();
		//System::IO::Stream^ OpenStandardInformation(System::ValueTuple<System::UInt128, bool> fileId);
		//System::IO::Stream^ OpenReparsePoint(System::ValueTuple<System::UInt128, bool> fileId);


		System::Collections::Generic::IEnumerable<System::String^>^ GetAlternateDataStreams();
		//cli::array<unsigned char>^ GetObjectId(System::ValueTuple<System::UInt128, bool> fileId);

	};

}
