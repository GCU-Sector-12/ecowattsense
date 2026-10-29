#nullable enable
using System;
using System.IO;
using System.Text;

namespace EcoWattSense.Client.Reporting;

public static class DeviceTypeSelection
{
    public static string Resolve(string? configured, string directory, TextReader input, TextWriter output, bool interactive)
        => Resolve(configured, directory, () => Prompt(input, output, interactive));

    public static string Resolve(string? configured, string directory, Func<string> selectDeviceType)
    {
        // Explicit configuration remains available for unattended deployment and tests.
        if (configured is not null)
            return Validate(configured);

        string path = Path.Combine(directory, "device-type.txt");
        try { return Validate(File.ReadAllText(path).Trim()); }
        catch (FileNotFoundException) { }
        catch (DirectoryNotFoundException) { }
        // Invalid or unreadable existing selections are never silently overwritten.

        string selected = Validate(selectDeviceType());

        Directory.CreateDirectory(directory);
        string temporary = Path.Combine(directory, $".device-type-{Guid.NewGuid():N}.tmp");
        try
        {
            using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            {
                stream.Write(Encoding.UTF8.GetBytes(selected + "\n"));
                stream.Flush(true);
            }
            try { File.Move(temporary, path, overwrite: false); }
            catch (IOException)
            {
                // Another process may have saved a choice while this prompt was open.
                // Read it rather than overwriting it or using an unpersisted choice.
                selected = Validate(File.ReadAllText(path).Trim());
            }
        }
        finally
        {
            try { File.Delete(temporary); }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }

        return selected;
    }

    public static string Prompt(TextReader input, TextWriter output, bool interactive)
    {
        if (!interactive)
            throw new InvalidOperationException("Set Reporting:DeviceType or save a device-type.txt selection before unattended startup.");

        string selected;
        while (true)
        {
            output.WriteLine("Select device type: 1 - Laptop, 2 - Desktop.");
            output.Write("Enter 1 or 2: ");
            output.Flush();
            string? answer = input.ReadLine();
            if (answer is null)
                throw new InvalidOperationException("Device type selection was not completed. Use an interactive terminal or set Reporting:DeviceType.");
            selected = answer.Trim() switch { "1" => "laptop", "2" => "desktop", _ => "" };
            if (selected.Length != 0) break;
            output.WriteLine("Invalid choice. Enter 1 for Laptop or 2 for Desktop.");
        }

        return selected;
    }

    private static string Validate(string value)
    {
        if (!DeviceReport.IsValidDeviceType(value))
            throw new InvalidOperationException("Invalid device type in Reporting:DeviceType or device-type.txt. Use a lowercase identifier of at most 40 characters.");
        return value;
    }
}
