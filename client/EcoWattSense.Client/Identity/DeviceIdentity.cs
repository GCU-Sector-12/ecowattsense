using System;

namespace EcoWattSense.Client.Identity;

public sealed class DeviceIdentity
{
    public Guid Id { get; }

    public DeviceIdentity(Guid id)
    {
        if (id == Guid.Empty)
            throw new ArgumentException("Device identity must not be empty.", nameof(id));

        Id = id;
    }
}

public sealed record DeviceIdentityResult(DeviceIdentity Identity, bool Created);
