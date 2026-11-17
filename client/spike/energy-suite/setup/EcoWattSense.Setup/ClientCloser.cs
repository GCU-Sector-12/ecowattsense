// ClientCloser.cs: the step "Closing the client" (SPEC section 10), shared by the install and the uninstall.
// Who uses it: InstallEngine (before the programs are copied) and MaintenanceEngine (before they are deleted).
// A running client or calibrator keeps its exe open, so a copy or a delete over it fails. The step asks every such
// process of any user to close, and the real operation kills the ones that do not close in time.
namespace EcoWattSense.Setup;

/// <summary>Closes the running client and calibrator processes and logs how many were closed.</summary>
public static class ClientCloser
{
    /// <summary>Closes every running client and calibrator process, then writes the line `close_client N`.</summary>
    /// <param name="services">The operations; the process operation does the closing.</param>
    /// <param name="log">The install log.</param>
    /// <returns>0 when all closed (or none ran), otherwise the error code of the process that could not be closed.</returns>
    public static int Run(SetupServices services, SetupLog log)
    {
        int code = services.Processes.CloseClients(out int closed);
        log.Line("close_client", closed.ToString(), 0);
        return code;
    }
}
