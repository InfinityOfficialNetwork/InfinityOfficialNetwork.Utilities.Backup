#include "pch.h"
#include "TokenPriviledgeManager.h"

#include <windows.h>

#pragma comment(lib, "advapi32.lib")

using namespace System;
using namespace System::Runtime::InteropServices;
using namespace System::ComponentModel;

/// <summary>
/// Enables or disables a specific privilege for the current process.
/// </summary>
/// <param name="privilegeName">e.g. SE_BACKUP_NAME or SE_SECURITY_NAME</param>
/// <param name="enable">True to enable, False to disable</param>
inline void InfinityOfficialNetwork::Utilities::Backup::Native::TokenPrivilegeManager::SetPrivilege(String^ privilegeName, bool enable) {
    HANDLE hToken;
    // 1. Open the access token for the current process
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        throw gcnew Win32Exception(Marshal::GetLastWin32Error());
    }

    try {
        LUID luid;
        // Convert managed string to native for LookupPrivilegeValue
        pin_ptr<const wchar_t> pPrivName = PtrToStringChars(privilegeName);

        // 2. Look up the LUID (Locally Unique Identifier) for the privilege name
        if (!LookupPrivilegeValueW(NULL, pPrivName, &luid)) {
            throw gcnew Win32Exception(Marshal::GetLastWin32Error());
        }

        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;

        // 3. Adjust the token privileges
        // Note: AdjustTokenPrivileges returns TRUE even if it failed to adjust 
        // the privilege (if the user doesn't have it). We must check GetLastError.
        if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
            throw gcnew Win32Exception(Marshal::GetLastWin32Error());
        }

        if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
            throw gcnew Exception("The privilege " + privilegeName + " is not held by the user. Ensure you are running as Administrator.");
        }
    }
    finally {
        CloseHandle(hToken);
    }
}

/// <summary>
/// Enables all privileges required for a full NTFS backup (including SACLs and EAs).
/// </summary>
inline void InfinityOfficialNetwork::Utilities::Backup::Native::TokenPrivilegeManager::EnableBackupPrivileges() {
    // Allows reading/writing files bypassing DACLs
    SetPrivilege(L"SeBackupPrivilege", true);
    SetPrivilege(L"SeRestorePrivilege", true);

    // REQUIRED to read/write the SACL (System Access Control List)
    SetPrivilege(L"SeSecurityPrivilege", true);

    // Optional: Allows taking ownership of any file
    SetPrivilege(L"SeTakeOwnershipPrivilege", true);
}
