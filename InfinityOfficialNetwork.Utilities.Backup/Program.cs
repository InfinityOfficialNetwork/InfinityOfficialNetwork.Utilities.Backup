using InfinityOfficialNetwork.Utilities.Backup.Native;
using InfinityOfficialNetwork.Utilities.Backup.Shared.Database.UsnJournal;
using Microsoft.Extensions.Logging;


namespace InfinityOfficialNetwork.Utilities.Backup;


internal class Program
{
    private static async Task<int> Main(string[] args)
    {
        using var loggerFactory = LoggerFactory.Create(builder =>
        {
            builder
                .AddSimpleConsole(options =>
                {
                    options.IncludeScopes = true;
                    options.SingleLine = true; // Optional: makes output cleaner
                    options.TimestampFormat = "HH:mm:ss "; // Optional: adds time
                })
                .SetMinimumLevel(LogLevel.Debug);
        });

        Core.DisposeLoggerFactory = loggerFactory;

        // 2. Create the logger instance
        ILogger logger = loggerFactory.CreateLogger<Program>();


        using VssSnapshotSession session = new(logger);

        session.AddVolume("K:\\");
        await using var snap = await session.CreateSnapshotAsync(CancellationToken.None);

        foreach (var property in snap.Snapshots)
        {
            using var h = snap.OpenSnapshotVolume(property);

            using var reader = h.OpenUsnJournalReader();

            using var ctx = await UsnJournalScratchDbContext.CreateNewDbAsync(DbProvider.Sqlite, "Data Source=\\\\ion-vmhst-0001.infinityofficial.net\\f$\\test\\test.db", loggerFactory);

            reader.ExtractAllRecords(ctx);

            Console.WriteLine("Found {0} records", ctx.UsnRecords.Count());

            foreach (var record in ctx.UsnRecords.Take(1000))
            {
                Console.WriteLine(record.ToString());
            }
        }

        return 0;
    }
}