using System;
using System.Collections.Generic;
using System.Text;

namespace InfinityOfficialNetwork.Utilities.Backup.Shared.Database.UsnJournal;
partial class UsnDatabaseContext
{
    const string S0000_InitializeDatabase =
        """
        -- Common Header
        CREATE TABLE IF NOT EXISTS UsnRecords (
            Usn INTEGER PRIMARY KEY,
            MajorVersion INTEGER,
            MinorVersion INTEGER
        );

        -- V2 Records (64-bit FRNs)
        CREATE TABLE IF NOT EXISTS UsnRecordsV2 (
            Usn INTEGER PRIMARY KEY REFERENCES UsnRecords(Usn),
            FileReferenceNumber INTEGER,
            ParentFileReferenceNumber INTEGER,
            Timestamp INTEGER, -- Stored as Ticks for speed
            Reason INTEGER,
            SourceInfo INTEGER,
            FileName TEXT
        );

        -- V3 Records (128-bit FRNs)
        CREATE TABLE IF NOT EXISTS UsnRecordsV3 (
            Usn INTEGER PRIMARY KEY REFERENCES UsnRecords(Usn),
            FileReferenceNumber BLOB, -- 16 bytes
            ParentFileReferenceNumber BLOB, -- 16 bytes
            Timestamp INTEGER,
            Reason INTEGER,
            SourceInfo INTEGER,
            FileName TEXT
        );

        -- V4 Records (128-bit FRNs + Extents)
        CREATE TABLE IF NOT EXISTS UsnRecordsV4 (
            Usn INTEGER PRIMARY KEY REFERENCES UsnRecords(Usn),
            FileReferenceNumber BLOB,
            ParentFileReferenceNumber BLOB,
            Reason INTEGER,
            SourceInfo INTEGER,
            Extents BLOB -- Raw binary dump of offset/length pairs
        );

        -- Indices for fast lookup
        CREATE INDEX IF NOT EXISTS IX_V2_FRN ON UsnRecordsV2 (FileReferenceNumber);
        CREATE INDEX IF NOT EXISTS IX_V3_FRN ON UsnRecordsV3 (FileReferenceNumber);
        CREATE INDEX IF NOT EXISTS IX_V4_FRN ON UsnRecordsV4 (FileReferenceNumber);
        """;
}
