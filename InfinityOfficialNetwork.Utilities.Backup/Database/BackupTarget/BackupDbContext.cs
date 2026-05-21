using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.ComponentModel.DataAnnotations.Schema;
using System.Text;

namespace InfinityOfficialNetwork.Utilities.Backup.Database.BackupTarget;
public partial class BackupDbContext : DbContext
{
	public BackupDbContext(DbContextOptions options) : base(options)
	{}

	public DbSet<VersionRecord> VersionRecords => Set<VersionRecord>();
	public DbSet<FileRecord> FileRecords => Set<FileRecord>();
	public DbSet<FileDirectoryMapRecord> FileDirectoryMapRecords => Set<FileDirectoryMapRecord>();
	public DbSet<StreamRecord> StreamRecords => Set<StreamRecord>();
	public DbSet<SegmentRecord> BlobRecords => Set<SegmentRecord>();
}


public record VersionRecord
{
	public required long Id { get; set; }
	public required DateTimeOffset Timestamp { get; set; }
}

public record FileRecord
{
	public required long Id { get; set; }

	public required long FirstVersionId { get; set; }
	public required long LastVersionId { get; set; } = long.MaxValue;

	public required ulong FileReferenceNumberLower { get; set; }
	public ulong? FileReferenceNumberUpper { get; set; }//if null, means 64-bit frn

	public required bool IsDirectory { get; set; }
}

public record FileDirectoryMapRecord
{
	public required long FileId { get; set; }

	//null parent is for root
	public long? ParentId { get; set; }

	public required long FirstVersionId { get; set; }
	public required long LastVersionId { get; set; } = long.MaxValue;

	public required byte[] FileName { get; set; }
}

public record StreamRecord
{
	public required long Id { get; set; }

	public required long FirstVersionId { get; set; }
	public required long LastVersionId { get; set; } = long.MaxValue;

	public required long FileId { get; set; }

	//ads streams will be in form 'ADS:$NAME' to avoid conflict
	public required byte[] StreamName { get; set; }

	public required long StreamLength { get; set; }
}

public record FileStreamMapRecord
{
	public required long FileId { get; set; }   // FK to File
	public required long StreamId { get; set; } // FK to Stream

	public required long FirstVersionId { get; set; }
	public required long LastVersionId { get; set; } = long.MaxValue;
}

public record SegmentRecord
{
	public required long Id { get; set; }
	public required long PageNumber { get; set; }

	public required long FirstVersionId { get; set; }
	public required long LastVersionId { get; set; } = long.MaxValue;

	public required byte[] Data { get; set; }

	public required bool IsDiff { get; set; }
}

public record StreamSegmentMapRecord
{
	public required long StreamId { get; set; } // FK to Stream
	public required long BlobId { get; set; }   
	public required long BlobPageNumber { get; set; }   // CFK to Blob

	public required long FirstVersionId { get; set; }
	public required long LastVersionId { get; set; } = long.MaxValue;
}

public record BlobRecord
{
	public required long Id { get; set; }
	public required byte[] Data { get; set; }

	public required byte[] Sha128Hash { get; set; }
}