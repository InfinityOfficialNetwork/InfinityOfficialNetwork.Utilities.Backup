using Microsoft.EntityFrameworkCore;
using Microsoft.EntityFrameworkCore.Storage.ValueConversion;
using Microsoft.Extensions.Logging;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.ComponentModel.DataAnnotations.Schema;
using System.Runtime.CompilerServices;
using System.Text;

namespace InfinityOfficialNetwork.Utilities.Backup.Shared.Database.UsnJournal;

public enum DbProvider
{
    Sqlite,
    SqlServer,
    Memory
}

public class UInt128ToBytesConverter : ValueConverter<UInt128, byte[]>
{
    public UInt128ToBytesConverter(ConverterMappingHints? mappingHints = null)
        : base(
            v => ConvertToBytes(v),
            v => ConvertFromBytes(v),
            mappingHints)
    {
    }

    private static byte[] ConvertToBytes(UInt128 v)
    {
        var bytes = new byte[16];
        BinaryPrimitives.WriteUInt128BigEndian(bytes,v);
        return bytes;
    }

    private static UInt128 ConvertFromBytes(byte[] v)
    {
        return BinaryPrimitives.ReadUInt128BigEndian(v);
    }
}


public class UsnJournalScratchDbContext : DbContext
{
    public UsnJournalScratchDbContext(DbContextOptions options) : base(options)
    {
    }

    public static async Task<UsnJournalScratchDbContext> CreateNewDbAsync(DbProvider provider, string connectionString, ILoggerFactory loggerFactory, CancellationToken cancellationToken = default)
    {
        var optionsBuilder = new DbContextOptionsBuilder<UsnJournalScratchDbContext>();
        optionsBuilder.UseLoggerFactory(loggerFactory);

        switch (provider)
        {
            case DbProvider.Sqlite:
                optionsBuilder.UseSqlite(connectionString);
                break;
        }

        _ = provider switch
        {
            DbProvider.Sqlite => optionsBuilder.UseSqlite(connectionString),
            DbProvider.SqlServer => optionsBuilder.UseSqlServer(connectionString),
            DbProvider.Memory => optionsBuilder.UseInMemoryDatabase(connectionString),
            _ => throw new InvalidOperationException()
        };

        var context = new UsnJournalScratchDbContext(optionsBuilder.Options);

#if DEBUG
        await context.Database.EnsureDeletedAsync(cancellationToken);
#endif

        await context.Database.EnsureCreatedAsync(cancellationToken);

        return context;
    }

    protected override void ConfigureConventions(ModelConfigurationBuilder configurationBuilder)
    {
        base.ConfigureConventions(configurationBuilder);

        configurationBuilder
            .Properties<UInt128>()
            .HaveConversion<UInt128ToBytesConverter>();
    }

    public DbSet<UsnRecordCommonHeaderEntity> UsnRecords => Set<UsnRecordCommonHeaderEntity>();
    public DbSet<UsnRecordV2Entity> UsnRecordsV2 => Set<UsnRecordV2Entity>();
    public DbSet<UsnRecordV3Entity> UsnRecordsV3 => Set<UsnRecordV3Entity>();
    public DbSet<UsnRecordV4Entity> UsnRecordsV4 => Set<UsnRecordV4Entity>();
}

[Index(nameof(MajorVersion))]
public record UsnRecordCommonHeaderEntity
{
    [Key]
    public ulong Usn { get; set; }

    [Range(2, 4)]
    public ushort MajorVersion { get; set; }
    public ushort MinorVersion { get; set; }

}


[Index(nameof(FileReferenceNumber))]
[Index(nameof(ParentFileReferenceNumber))]
public record UsnRecordV2Entity
{
    [Key]
    [ForeignKey(nameof(UsnRecordCommonHeaderEntity))]
    public ulong Usn { get; set; }


    public long FileReferenceNumber { get; set; }
    public long ParentFileReferenceNumber { get; set; }
    public DateTime Timestamp { get; set; }
    public uint Reason { get; set; }
    public uint SourceInfo { get; set; }

    [MaxLength(260)]
    public string? FileName { get; set; }
}

[Index(nameof(FileReferenceNumberBlob))]
[Index(nameof(ParentFileReferenceNumberBlob))]
public record UsnRecordV3Entity
{
    [Key]
    [ForeignKey(nameof(UsnRecordCommonHeaderEntity))]
    public ulong Usn { get; set; }

    [NotMapped]
    public UInt128 FileReferenceNumber { get; set; }

    [Column(nameof(FileReferenceNumber))]
    [Length(16,16)]
    public byte[] FileReferenceNumberBlob
    {
        get
        {
            byte[] bytes = new byte[16];
            BinaryPrimitives.WriteUInt128LittleEndian(bytes, FileReferenceNumber);
            return bytes;
        }
        set
        {
            FileReferenceNumber = BinaryPrimitives.ReadUInt128LittleEndian(value);
        }
    }


    [NotMapped]
    public UInt128 ParentFileReferenceNumber { get; set; }

    [Column(nameof(ParentFileReferenceNumber))]
    [Length(16,16)]
    public byte[] ParentFileReferenceNumberBlob
    {
        get
        {
            byte[] bytes = new byte[16];
            BinaryPrimitives.WriteUInt128LittleEndian(bytes, ParentFileReferenceNumber);
            return bytes;
        }
        set
        {
            ParentFileReferenceNumber = BinaryPrimitives.ReadUInt128LittleEndian(value);
        }
    }

    public DateTime Timestamp { get; set; }
    public int Reason { get; set; }
    public uint SourceInfo { get; set; }
    [MaxLength(260)]
    public string? FileName { get; set; }
}

[Keyless]
public record UsnExtent
{
    public ulong Offset { get; set; }
    public ulong Length { get; set; }
};


[Index(nameof(FileReferenceNumberBlob))]
[Index(nameof(ParentFileReferenceNumberBlob))]
public record UsnRecordV4Entity
{
    [Key]
    [ForeignKey(nameof(UsnRecordCommonHeaderEntity))]
    public ulong Usn { get; set; }

    [NotMapped]
    public UInt128 FileReferenceNumber { get; set; }

    [Column(nameof(FileReferenceNumber))]
    [Length(16,16)]
    public byte[] FileReferenceNumberBlob
    {
        get
        {
            byte[] bytes = new byte[16];
            BinaryPrimitives.WriteUInt128LittleEndian(bytes, FileReferenceNumber);
            return bytes;
        }
        set
        {
            FileReferenceNumber = BinaryPrimitives.ReadUInt128LittleEndian(value);
        }
    }


    [NotMapped]
    public UInt128 ParentFileReferenceNumber { get; set; }

    [Column(nameof(ParentFileReferenceNumber))]
    [Length(16,16)]
    public byte[] ParentFileReferenceNumberBlob
    {
        get
        {
            byte[] bytes = new byte[16];
            BinaryPrimitives.WriteUInt128LittleEndian(bytes, ParentFileReferenceNumber);
            return bytes;
        }
        set
        {
            ParentFileReferenceNumber = BinaryPrimitives.ReadUInt128LittleEndian(value);
        }
    }
    public int Reason { get; set; }
    public uint SourceInfo { get; set; }

    [NotMapped]
    public List<UsnExtent> Extents { get; set; } = new();

    [Column(nameof(Extents))] // Renames the column in the DB
    public byte[] ExtentsBlob
    {
        get
        {
            if (Extents == null || Extents.Count == 0) return Array.Empty<byte>();

            byte[] bytes = new byte[Extents.Count * 16];
            Span<byte> span = bytes.AsSpan();

            for (int i = 0; i < Extents.Count; i++)
            {
                BinaryPrimitives.WriteUInt64LittleEndian(span.Slice(i * 16), Extents[i].Offset);
                BinaryPrimitives.WriteUInt64LittleEndian(span.Slice(i * 16 + 8), Extents[i].Length); 
            }
            return bytes;
        }
        set
        {
            if (value == null || value.Length == 0)
            {
                Extents = new List<UsnExtent>();
                return;
            }

            if (value.Length % 16 != 0)
                throw new InvalidDataException("Invalid buffer length");

            var newList = new List<UsnExtent>(value.Length / 16);
            ReadOnlySpan<byte> span = value.AsSpan();

            for (int i = 0; i < value.Length; i += 16)
            {
                newList.Add(new UsnExtent()
                {
                    Offset = BinaryPrimitives.ReadUInt64LittleEndian(span.Slice(i)),
                    Length = BinaryPrimitives.ReadUInt64LittleEndian(span.Slice(i + 8))
                });
            }

            this.Extents = newList;
        }
    }
}