

namespace InfinityOfficialNetwork::Utilities::Backup::Native {

    public ref class TokenPrivilegeManager {
    public:
        /// <summary>
        /// Enables or disables a specific privilege for the current process.
        /// </summary>
        /// <param name="privilegeName">e.g. SE_BACKUP_NAME or SE_SECURITY_NAME</param>
        /// <param name="enable">True to enable, False to disable</param>
        static void SetPrivilege(System::String^ privilegeName, bool enable);

        /// <summary>
        /// Enables all privileges required for a full NTFS backup (including SACLs and EAs).
        /// </summary>
        static void EnableBackupPrivileges();
    };
}