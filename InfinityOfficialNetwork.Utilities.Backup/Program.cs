using InfinityOfficialNetwork.Utilities.Backup.Database.BackupTarget;
using InfinityOfficialNetwork.Utilities.Backup.Native;
using InfinityOfficialNetwork.Utilities.Backup.Services.Database;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Logging;
using System.Threading;


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
				.SetMinimumLevel(LogLevel.Debug)
				//.AddFilter("Microsoft.EntityFrameworkCore", LogLevel.Warning)
#if DEBUG
				.AddFilter("InfinityOfficialNetwork", LogLevel.Trace);
#endif
		});

		// 2. Create the logger instance
		ILogger logger = loggerFactory.CreateLogger<Program>();

		var optionsBuilder = new DbContextOptionsBuilder<BackupDbContext>();
		optionsBuilder.UseLoggerFactory(loggerFactory);

		optionsBuilder.UseSqlite("Data Source=test.db");

		await using var context = new BackupDbContext(optionsBuilder.Options);

		await context.Database.EnsureDeletedAsync();
		await context.Database.EnsureCreatedAsync();
		await context.Database.MigrateAsync();

		await context.SaveChangesAsync();

		//return 0;

		TokenPrivilegeManager.EnableBackupPrivileges();

		using VssSnapshotSession session = new(logger);

		session.AddVolume("C:\\");
		await using var snap = await session.CreateSnapshotAsync(CancellationToken.None);

		var stdout = Console.OpenStandardOutput();
		GC.SuppressFinalize(stdout);

		foreach (var property in snap.Snapshots)
		{
			using var volume = snap.OpenSnapshotVolume(property);

			using var reader = volume.OpenUsnJournalReader();

			var records = reader.ExtractAllRecords();


			Console.WriteLine("Found {0} records", records.Count);



			var hFile = volume.OpenFileById((new UInt128(0x0000_0000_0000_0000ul,0x0002_0000_0003_1a0ful), true));
			await hFile.OpenExtendedAttributes().CopyToAsync(stdout);
		}

		return 0;
	}
}