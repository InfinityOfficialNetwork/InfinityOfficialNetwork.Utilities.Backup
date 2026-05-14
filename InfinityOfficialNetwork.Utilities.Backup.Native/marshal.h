#pragma once

#include <msclr/marshal.h>
#include <msclr/marshal_atl.h>
#include <msclr/marshal_cppstd.h>
#include <msclr/marshal_windows.h>
#include <guiddef.h>
#include <winnt.h>

namespace msclr {
	namespace interop {
		// Specialization: Native VSS_ID (GUID) -> Managed System::Guid
		template<>
		inline System::Guid marshal_as<System::Guid, GUID>(const GUID& from) {
			return System::Guid(
				from.Data1, from.Data2, from.Data3,
				from.Data4[0], from.Data4[1], from.Data4[2], from.Data4[3],
				from.Data4[4], from.Data4[5], from.Data4[6], from.Data4[7]
			);
		}

		// Optional: Managed System::Guid -> Native VSS_ID (GUID)
		template<>
		inline GUID marshal_as<GUID, System::Guid>(const System::Guid& from) {
			System::Guid from2 = from;
			GUID to{};
			cli::array<System::Byte>^ bytes = from2.ToByteArray();
			pin_ptr<System::Byte> pBytes = &bytes[0];
			memcpy(&to, pBytes, 16);
			return to;
		}

		template<>
		inline System::UInt128 marshal_as<System::UInt128, FILE_ID_128>(const FILE_ID_128& from) {
			return System::UInt128(reinterpret_cast<const uint64_t&>(from.Identifier[0]), reinterpret_cast<const uint64_t&>(from.Identifier[sizeof(uint64_t)]));
		}
	}
}

#include <utility>

template <typename T>
ref class nroot {
private:
	T* value;

public:
	T& get() { return *value; }

	template<typename...TArgs>
	nroot(TArgs&&...targs) {
		value = new T(std::forward<TArgs...>(targs)...);
	}

	~nroot()
	{
		delete value;
		System::GC::SuppressFinalize(this);
	}

	!nroot() {
		this->~nroot();
	}
};