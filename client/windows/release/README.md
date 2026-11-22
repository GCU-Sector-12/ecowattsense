# Windows client release manifest

The package script creates latest.json alongside the ZIP and web installer. Publish the signed release assets before updating any published manifest. Use the manifest generated with those exact assets.

The 0.1.8 client, the 0.1.8 web installer and the dashboard read client/spike/energy-suite/release/latest.json from the EWS-energy-suite branch. Keep that file and the branch available. After the next signed release is published and its assets are checked, update that manifest so those builds can find the release.

The copy under client/windows/release records the manifest in the integrated source tree. The published update address stays on EWS-energy-suite until the clients that use it are migrated.
