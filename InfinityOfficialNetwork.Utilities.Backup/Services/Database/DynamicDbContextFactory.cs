using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Data.Common;
using System.Text;

namespace InfinityOfficialNetwork.Utilities.Backup.Services.Database;

public enum DbProvider
{
    Sqlite,
    SqlServer,
    Memory
}

internal class DynamicDbContextFactory
{
    public static async Task<TContext> CreateNewDbContextAsync<TContext>(DbProvider provider, string connectionString, ILoggerFactory loggerFactory, CancellationToken cancellationToken = default)
        where TContext : DbContext
    {
        var optionsBuilder = new DbContextOptionsBuilder<TContext>();
        optionsBuilder.UseLoggerFactory(loggerFactory);

        _ = provider switch
        {
            DbProvider.Sqlite => optionsBuilder.UseSqlite(connectionString),
            DbProvider.SqlServer => optionsBuilder.UseSqlServer(connectionString),
            DbProvider.Memory => optionsBuilder.UseInMemoryDatabase(connectionString),
            _ => throw new InvalidOperationException()
        };

        var context = (TContext?)Activator.CreateInstance(typeof(TContext), optionsBuilder.Options) ?? throw new InvalidOperationException("Constructor that takes options not found");

        await context.Database.EnsureDeletedAsync(cancellationToken);
        await context.Database.EnsureCreatedAsync(cancellationToken);
        await context.Database.MigrateAsync(cancellationToken);
        return context;
    }
}
