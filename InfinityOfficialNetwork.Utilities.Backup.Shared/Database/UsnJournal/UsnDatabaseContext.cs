using Microsoft.Data.Sqlite;
using System.Runtime.InteropServices;

namespace InfinityOfficialNetwork.Utilities.Backup.Shared.Database.UsnJournal;



public readonly record struct UsnExtent(ulong Offset, ulong Length);

public readonly record struct UsnRecordV2
{
    public ulong Usn { get; init; }
    public long FileReferenceNumber { get; init; }
    public long ParentFileReferenceNumber { get; init; }
    public long TimestampTicks { get; init; }
    public uint Reason { get; init; }
    public uint SourceInfo { get; init; }
    public string FileName { get; init; }
}

public readonly record struct UsnRecordV3
{
    public ulong Usn { get; init; }
    public UInt128 FileReferenceNumber { get; init; }
    public UInt128 ParentFileReferenceNumber { get; init; }
    public long TimestampTicks { get; init; }
    public int Reason { get; init; }
    public uint SourceInfo { get; init; }
    public string FileName { get; init; }
}

public readonly record struct UsnRecordV4
{
    public ulong Usn { get; init; }
    public UInt128 FileReferenceNumber { get; init; }
    public UInt128 ParentFileReferenceNumber { get; init; }
    public int Reason { get; init; }
    public uint SourceInfo { get; init; }
    public ReadOnlyMemory<UsnExtent> Extents { get; init; }
}

public partial class UsnDatabaseContext
{
    SqliteConnection connection;

    public UsnDatabaseContext(string connectionString)
    {
        connection = new(connectionString);

        string a = S0000_InitializeDatabase;

        
    }


}