# Manual device type selection

When Reporting:DeviceType is absent, the interactive console loads device-type.txt
from the same directory as device-id.txt. If it is missing, startup asks:
1 = laptop or 2 = desktop. Other console input is rejected and prompts again.
The choice is saved outside the repository; there is no automatic hardware detection.

The default directory is CommonApplicationData/EcoWattSense/Client. An explicit
DeviceIdentity:Directory override also locates the saved device type there. Directory
permissions are not changed. A save failure stops startup rather than silently losing
the choice. Invalid/unreadable saved files are not overwritten. To change the persistent
choice, edit device-type.txt while the client is stopped; do not change device-id.txt.

Explicit Reporting:DeviceType configuration takes priority without changing the saved
file. This supports manual command-line overrides and unattended deployments. Services
must have either explicit configuration or an existing saved choice; they do not prompt.
End-of-input cancels selection with a clear error. In Rider, use an interactive console
or run dotnet run in the integrated terminal if its Run console cannot accept input.

The launch profile no longer hard-codes laptop. Reporting model validation and all
monitoring behaviour remain unchanged.
