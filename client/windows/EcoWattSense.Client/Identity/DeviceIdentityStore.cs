#nullable enable
using System;
using System.IO;
using System.Text;

namespace EcoWattSense.Client.Identity;

/// <summary>Persists identity independently of the executable, repository, and current account.</summary>
public sealed class DeviceIdentityStore
{
    public const string FileName = "device-id.txt";

    public static string ResolveDirectory(string? directoryOverride = null)
    {
        if (directoryOverride is not null)
        {
            if (string.IsNullOrWhiteSpace(directoryOverride) || !Path.IsPathFullyQualified(directoryOverride))
                throw new ArgumentException("DeviceIdentity:Directory must be an absolute directory path.");

            return Path.GetFullPath(directoryOverride);
        }

        string commonData = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
        if (string.IsNullOrWhiteSpace(commonData) || !Path.IsPathFullyQualified(commonData))
            throw new InvalidOperationException("The Windows common application data directory is unavailable.");

        return Path.Combine(commonData, "EcoWattSense", "Client");
    }

    public DeviceIdentityResult LoadOrCreate(string directory)
    {
        string identityPath = Path.Combine(ResolveDirectory(directory), FileName);
        try
        {
            try
            {
                return new DeviceIdentityResult(Read(identityPath), false);
            }
            catch (FileNotFoundException) { }
            catch (DirectoryNotFoundException) { }

            // Only confirmed absence permits creation. Access and validation errors never do.
            Directory.CreateDirectory(Path.GetDirectoryName(identityPath)!);
            string temporaryPath = Path.Combine(Path.GetDirectoryName(identityPath)!,
                $".device-id-{Guid.NewGuid():N}.tmp");
            try
            {
                var identity = new DeviceIdentity(Guid.NewGuid());
                byte[] contents = Encoding.UTF8.GetBytes(identity.Id.ToString("D") + "\n");
                using (var stream = new FileStream(temporaryPath, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                {
                    stream.Write(contents);
                    stream.Flush(flushToDisk: true);
                }

                try
                {
                    // Same-directory rename publishes a complete file; never replace an existing UUID.
                    File.Move(temporaryPath, identityPath, overwrite: false);
                    return new DeviceIdentityResult(identity, true);
                }
                catch (IOException publicationError)
                {
                    // A concurrent creator may have won. Only a valid persisted UUID is usable.
                    // If no destination exists, preserve the original publication failure.
                    try { return new DeviceIdentityResult(Read(identityPath), false); }
                    catch (FileNotFoundException) { throw new IOException("Failed to publish device identity.", publicationError); }
                    catch (DirectoryNotFoundException) { throw new IOException("Failed to publish device identity.", publicationError); }
                }
            }
            finally
            {
                // Clean up only this attempt's private temporary file, never the identity file.
                // Cleanup must not hide a read/write error or invalidate a published identity.
                try { File.Delete(temporaryPath); }
                catch (IOException) { }
                catch (UnauthorizedAccessException) { }
            }
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or InvalidDataException)
        {
            throw new InvalidOperationException(
                $"Unable to load or persist device identity at '{identityPath}'. " +
                "Restore a valid identity file or correct access to this directory; existing identity was not replaced.",
                exception);
        }
    }

    private static DeviceIdentity Read(string path)
    {
        string text = File.ReadAllText(path).Trim();
        if (!Guid.TryParseExact(text, "D", out Guid id) || id == Guid.Empty)
            throw new InvalidDataException($"Identity file '{path}' must contain one non-empty UUID in D format.");

        return new DeviceIdentity(id);
    }
}
