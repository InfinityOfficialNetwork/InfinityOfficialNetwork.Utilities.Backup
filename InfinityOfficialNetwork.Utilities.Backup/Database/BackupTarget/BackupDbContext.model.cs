using Microsoft.EntityFrameworkCore;
using Microsoft.EntityFrameworkCore.Metadata.Builders;

namespace InfinityOfficialNetwork.Utilities.Backup.Database.BackupTarget;

#pragma warning disable CS9113 // Parameter is unread.

partial class BackupDbContext
{
	protected override void OnModelCreating(ModelBuilder modelBuilder)
	{
		string provider = Database.ProviderName ?? throw new InvalidOperationException("Provider not found");

		modelBuilder.ApplyConfiguration(new VersionRecordConfiguration(provider));
		modelBuilder.ApplyConfiguration(new FileRecordConfiguration(provider));
		modelBuilder.ApplyConfiguration(new FileDirectoryMapRecordConfiguration(provider));
		modelBuilder.ApplyConfiguration(new StreamRecordConfiguration(provider));
		modelBuilder.ApplyConfiguration(new FileStreamMapRecordConfiguration(provider));
		modelBuilder.ApplyConfiguration(new BlobRecordConfiguration(provider));
		modelBuilder.ApplyConfiguration(new StreamBlobMapRecordConfiguration(provider));
	}

	internal const string SqlServerProviderString = "Microsoft.EntityFrameworkCore.SqlServer";
	internal const string SqliteProviderString = "Microsoft.EntityFrameworkCore.Sqlite";
	internal const string PostgreSqlProviderString = "Npgsql.EntityFrameworkCore.PostgreSQL";
}

public class VersionRecordConfiguration(string providerName) : IEntityTypeConfiguration<VersionRecord>
{
	public void Configure(EntityTypeBuilder<VersionRecord> builder)
	{
		builder.HasKey(v => v.Id);
		builder.Property(v => v.Id).ValueGeneratedNever();
		builder.Property(v => v.Timestamp).IsRequired();
		builder.HasIndex(v => v.Timestamp).IsUnique();

		builder.HasData(new VersionRecord() { Id = long.MaxValue, Timestamp = DateTimeOffset.MaxValue });
	}
}

public class FileRecordConfiguration(string providerName) : IEntityTypeConfiguration<FileRecord>
{
	public void Configure(EntityTypeBuilder<FileRecord> builder)
	{
		builder.HasKey(f => new { f.Id, f.FirstVersionId });

		// Foreign Keys for Window boundaries
		builder.HasOne<VersionRecord>().WithMany().HasForeignKey(f => f.FirstVersionId).OnDelete(DeleteBehavior.Restrict);
		builder.HasOne<VersionRecord>().WithMany().HasForeignKey(f => f.LastVersionId).OnDelete(DeleteBehavior.Restrict);

		builder.HasIndex(f => new { f.FileReferenceNumberLower, f.FileReferenceNumberUpper, f.FirstVersionId, f.LastVersionId });

		// Filtered Index for "Current" state
		var activeIndex = builder.HasIndex(f => f.Id).HasDatabaseName("IX_FileRecord_Active");
		string? filter = string.Format(providerName switch
		{
			BackupDbContext.SqlServerProviderString => "[{0}] = {1}",
			BackupDbContext.SqliteProviderString => "\"{0}\" = {1}",
			BackupDbContext.PostgreSqlProviderString  => "\"{0}\" = {1}",
			_ => ""
		}, nameof(FileRecord.LastVersionId), ulong.MaxValue);
		if (filter != "") activeIndex.HasFilter(filter);
	}
}

public class FileDirectoryMapRecordConfiguration(string providerName) : IEntityTypeConfiguration<FileDirectoryMapRecord>
{
	public void Configure(EntityTypeBuilder<FileDirectoryMapRecord> builder)
	{
		builder.HasKey(m => new { m.FileId, m.FirstVersionId });
		builder.HasOne<VersionRecord>().WithMany().HasForeignKey(m => m.FirstVersionId).OnDelete(DeleteBehavior.Restrict);
		builder.HasOne<VersionRecord>().WithMany().HasForeignKey(m => m.LastVersionId).OnDelete(DeleteBehavior.Restrict);

		builder.Property(m => m.FileName).HasMaxLength(512).IsRequired();
		builder.HasIndex(m => new { m.ParentId, m.FirstVersionId, m.LastVersionId });

		var activeIndex = builder.HasIndex(m => m.FileId).HasDatabaseName("IX_FileDirectoryMap_Active");
		string? filter = providerName switch
		{
			BackupDbContext.SqlServerProviderString  => $"[{nameof(FileDirectoryMapRecord.LastVersionId)}] = {ulong.MaxValue}",
			BackupDbContext.SqliteProviderString  => $"\"{nameof(FileDirectoryMapRecord.LastVersionId)}\" = {ulong.MaxValue}",
			BackupDbContext.PostgreSqlProviderString => $"\"{nameof(FileDirectoryMapRecord.LastVersionId)}\" = {ulong.MaxValue}",
			_ => null
		};
		if (filter != null) activeIndex.HasFilter(filter);
	}
}

public class StreamRecordConfiguration(string providerName) : IEntityTypeConfiguration<StreamRecord>
{
	public void Configure(EntityTypeBuilder<StreamRecord> builder)
	{
		builder.HasKey(s => new { s.Id, s.FirstVersionId });
		builder.HasOne<VersionRecord>().WithMany().HasForeignKey(s => s.FirstVersionId).OnDelete(DeleteBehavior.Restrict);
		builder.HasOne<VersionRecord>().WithMany().HasForeignKey(s => s.LastVersionId).OnDelete(DeleteBehavior.Restrict);

		builder.Property(s => s.StreamName).IsRequired().HasMaxLength(512);
		builder.HasIndex(s => new { s.FileId, s.FirstVersionId, s.LastVersionId });
	}
}

public class FileStreamMapRecordConfiguration(string providerName) : IEntityTypeConfiguration<FileStreamMapRecord>
{
	public void Configure(EntityTypeBuilder<FileStreamMapRecord> builder)
	{
		builder.HasKey(m => new { m.FileId, m.StreamId, m.FirstVersionId });
		builder.HasOne<VersionRecord>().WithMany().HasForeignKey(m => m.FirstVersionId).OnDelete(DeleteBehavior.Restrict);
		builder.HasOne<VersionRecord>().WithMany().HasForeignKey(m => m.LastVersionId).OnDelete(DeleteBehavior.Restrict);

		builder.HasIndex(m => new { m.FileId, m.FirstVersionId, m.LastVersionId });
	}
}

public class BlobRecordConfiguration(string providerName) : IEntityTypeConfiguration<SegmentRecord>
{
	public void Configure(EntityTypeBuilder<SegmentRecord> builder)
	{
		builder.HasKey(b => new { b.Id, b.PageNumber, b.FirstVersionId });
		builder.HasOne<VersionRecord>().WithMany().HasForeignKey(b => b.FirstVersionId).OnDelete(DeleteBehavior.Restrict);
		builder.HasOne<VersionRecord>().WithMany().HasForeignKey(b => b.LastVersionId).OnDelete(DeleteBehavior.Restrict);

		builder.Property(b => b.Data).HasMaxLength(65536).IsRequired();

		// Filtered Index for Reverse Diff Optimization
		var activeIndex = builder.HasIndex(b => new { b.Id, b.PageNumber }).HasDatabaseName("IX_BlobRecord_CurrentFull");
		string? filter = providerName switch
		{
			BackupDbContext.SqlServerProviderString  => $"[{nameof(SegmentRecord.LastVersionId)}] = {ulong.MaxValue}",
			BackupDbContext.SqliteProviderString  => $"\"{nameof(SegmentRecord.LastVersionId)}\" = {ulong.MaxValue}",
			BackupDbContext.PostgreSqlProviderString => $"\"{nameof(SegmentRecord.LastVersionId)}\" = {ulong.MaxValue}",
			_ => null
		};
		if (filter != null) activeIndex.HasFilter(filter);

		// Check Constraint
		string ckName = "CK_BlobRecord_VersionSequence";
		string ckSql = providerName switch
		{
			BackupDbContext.SqlServerProviderString => $"[{nameof(SegmentRecord.LastVersionId)}] >= [{nameof(SegmentRecord.FirstVersionId)}]",
			BackupDbContext.SqliteProviderString  => $"\"{nameof(SegmentRecord.LastVersionId)}\" >= \"{nameof(SegmentRecord.FirstVersionId)}\"",
			BackupDbContext.PostgreSqlProviderString  => $"\"{nameof(SegmentRecord.LastVersionId)}\" >= \"{nameof(SegmentRecord.FirstVersionId)}\"",
			_ => ""
		};

		if (!string.IsNullOrEmpty(ckSql))
		{
			builder.ToTable(t => t.HasCheckConstraint(ckName, ckSql));
		}
	}
}

public class StreamBlobMapRecordConfiguration(string providerName) : IEntityTypeConfiguration<StreamSegmentMapRecord>
{
	public void Configure(EntityTypeBuilder<StreamSegmentMapRecord> builder)
	{
		builder.HasKey(m => new { m.StreamId, m.BlobId, m.FirstVersionId });
		builder.HasOne<VersionRecord>().WithMany().HasForeignKey(m => m.FirstVersionId).OnDelete(DeleteBehavior.Restrict);
		builder.HasOne<VersionRecord>().WithMany().HasForeignKey(m => m.LastVersionId).OnDelete(DeleteBehavior.Restrict);

		builder.HasIndex(m => new { m.StreamId, m.BlobPageNumber, m.FirstVersionId, m.LastVersionId })
			.HasDatabaseName("IX_StreamBlob_Lookup");
	}
}