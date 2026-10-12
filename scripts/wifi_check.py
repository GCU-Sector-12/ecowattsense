#!/usr/bin/env python3
"""Network check for the EcoWattSense demo: will this Wi-Fi work with our Cloudflare Tunnel?

The tunnel host (the Mac that runs Docker and cloudflared) needs outbound TCP 7844 to
Cloudflare, plus HTTPS (443). Every other laptop only needs normal HTTPS (443) to the
tunnel address. Nothing is installed or changed. Python 3.8+, standard library only.

Examples
  python3 scripts/wifi_check.py                               on the Mac, on the university Wi-Fi
  python3 scripts/wifi_check.py --url https://HOST/           also test the real tunnel address
  python3 scripts/wifi_check.py --client --url https://HOST/  on a team laptop (no tunnel tests)
  python3 scripts/wifi_check.py --no-tunnel                   skip the real quick tunnel test
  python3 scripts/wifi_check.py --protocol http2              quick tunnel over TCP only

Tip: at home run `docker pull cloudflare/cloudflared:latest`, so the image is already here.
Source of the port list: Cloudflare docs, "Tunnel with firewall" and "Connectivity pre-checks".
"""
import argparse
import ipaddress
import os
import platform
import queue
import re
import secrets
import shutil
import socket
import ssl
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

EDGE_HOSTS = ["region1.v2.argotunnel.com", "region2.v2.argotunnel.com"]
EDGE_RANGE = ipaddress.ip_network("198.41.128.0/17")  # Cloudflare range of the tunnel edge
SRV_NAME = "_v2-origintunneld._tcp.argotunnel.com"
TRACE_URL = "https://www.cloudflare.com/cdn-cgi/trace"
API_URL = "https://api.cloudflare.com/client/v4/"
CONTAINER = "ews-wifi-check"
TOKEN = "ews-wifi-check-" + secrets.token_hex(4)
URL_RE = re.compile(r"https://[a-z0-9-]+\.trycloudflare\.com")
PROTO_RE = re.compile(r"protocol=(\w+)")
PUBLIC_CAS = ("cloudflare", "google", "digicert", "let's encrypt", "isrg", "sectigo",
              "globalsign", "ssl.com", "amazon", "microsoft", "comodo", "entrust", "godaddy")

results = []  # (status, side, text). side: "all" = every laptop, "server" = tunnel host only
state = {"tunnel_ok": False, "no_network": False}


def say(status, side, text):
    results.append((status, side, text))
    print("[%-4s] %s" % (status, text), flush=True)


def section(title):
    print("\n--- %s ---" % title, flush=True)


# ---------- small helpers ----------

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def http_get(url, timeout=10, follow=True):
    """Return (status, body, error). error: None, 'cert', 'dns', 'timeout' or 'other: ...'.
    Direct connection, system proxy settings are ignored (cloudflared ignores them too)."""
    handlers = [urllib.request.ProxyHandler({}),
                urllib.request.HTTPSHandler(context=ssl.create_default_context())]
    if not follow:
        handlers.append(NoRedirect())
    opener = urllib.request.build_opener(*handlers)
    req = urllib.request.Request(url, headers={"User-Agent": "ews-wifi-check/1"})
    try:
        with opener.open(req, timeout=timeout) as r:
            return r.status, r.read(2000).decode("utf-8", "replace"), None
    except urllib.error.HTTPError as e:
        return e.code, "", None
    except Exception as e:
        reason = getattr(e, "reason", e)
        if isinstance(reason, ssl.SSLCertVerificationError):
            return None, "", "cert"
        if isinstance(reason, socket.gaierror):
            return None, "", "dns"
        if isinstance(reason, (socket.timeout, TimeoutError)) or isinstance(e, (socket.timeout, TimeoutError)):
            return None, "", "timeout"
        return None, "", "other: %s" % reason


def curl_ok(url):
    curl = shutil.which("curl")
    if not curl:
        return None
    try:
        p = subprocess.run([curl, "-sS", "-m", "10", "-o", os.devnull, "-w", "%{http_code}", url],
                           capture_output=True, text=True, timeout=15)
        return p.returncode == 0 and p.stdout.strip() not in ("", "000")
    except Exception:
        return None


def addresses(host):
    try:
        infos = socket.getaddrinfo(host, 443, proto=socket.IPPROTO_TCP)
    except socket.gaierror as e:
        return [], str(e)
    out = []
    for info in infos:
        if info[4][0] not in out:
            out.append(info[4][0])
    return out, None


def odd(ip, expect=None):
    a = ipaddress.ip_address(ip.split("%")[0])
    if a.is_private or a.is_loopback or a.is_unspecified or a.is_link_local:
        return True
    return bool(expect) and a.version == 4 and a not in expect


def tcp_open(host, port, timeout=6):
    t0 = time.time()
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True, int((time.time() - t0) * 1000), None
    except Exception as e:
        return False, int((time.time() - t0) * 1000), e


# ---------- checks ----------

def local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("1.1.1.1", 80))  # no packet is sent, this only picks the network interface
        ip = s.getsockname()[0]
        s.close()
        return ip
    except OSError:
        return None


def dns_servers():
    try:
        with open("/etc/resolv.conf") as f:
            return [p[1] for p in (l.split() for l in f) if len(p) > 1 and p[0] == "nameserver"]
    except OSError:
        return []


def check_basics():
    ip = local_ip()
    if not ip:
        state["no_network"] = True
        say("FAIL", "all", "No network: no route to the internet. Join the Wi-Fi (and log in to any login page) first.")
        return
    else:
        say("INFO", "all", "Local address %s, DNS servers: %s" % (ip, ", ".join(dns_servers()) or "unknown"))
    proxies = urllib.request.getproxies()
    if proxies:
        say("INFO", "all", "A system proxy is configured on this computer. This script connects directly, "
            "like cloudflared does. Browsers may use the proxy.")
    st, _, err = http_get("http://connectivitycheck.gstatic.com/generate_204", timeout=8, follow=False)
    if err is None and st == 204:
        say("PASS", "all", "No captive portal (no login page in the way)")
    elif err is None:
        say("WARN", "all", "Plain HTTP got HTTP %s instead of 204. A login page (captive portal) may be in the "
            "way: open a browser, log in, run this again." % st)
    else:
        say("WARN", "all", "Plain HTTP (port 80) test failed (%s). Not needed for the tunnel, but look for a login page." % err)


def check_dns(hosts, side, level="FAIL", expect=None):
    for h in hosts:
        ips, err = addresses(h)
        if err:
            say(level, side, "DNS %s: no answer (%s). The network DNS may block it." % (h, err))
        elif any(odd(ip, expect) for ip in ips):
            say("WARN", side, "DNS %s -> %s: unexpected address, the network may change DNS answers."
                % (h, ", ".join(ips[:3])))
        else:
            say("PASS", side, "DNS %s -> %s" % (h, ", ".join(ips[:2])))


def check_https(url, side, label, level="FAIL"):
    t0 = time.time()
    st, body, err = http_get(url, timeout=12)
    ms = int((time.time() - t0) * 1000)
    if err is None:
        say("PASS", side, "HTTPS %s: HTTP %s in %d ms" % (label, st, ms))
        return True, body
    if err == "cert":
        if curl_ok(url):
            say("WARN", side, "HTTPS %s: Python does not trust the certificate, but curl does. This is a Python "
                "CA problem (python.org build on macOS: run 'Install Certificates.command'), not the network." % label)
            return True, body
        say(level, side, "HTTPS %s: certificate not trusted. Either the network inspects TLS (a middle box "
            "replaces the certificate) or this Python has no CA bundle." % label)
        return False, body
    say(level, side, "HTTPS %s: %s." % (label, "DNS lookup failed" if err == "dns" else err))
    return False, body


def check_issuer(host, side):
    try:
        with socket.create_connection((host, 443), timeout=8) as s:
            with ssl.create_default_context().wrap_socket(s, server_hostname=host) as t:
                cert = t.getpeercert()
    except Exception:
        return  # already reported by the HTTPS check
    parts = dict(kv for rdn in cert.get("issuer", ()) for kv in rdn)
    name = parts.get("organizationName") or parts.get("commonName") or "unknown"
    if any(w in (name + " " + parts.get("commonName", "")).lower() for w in PUBLIC_CAS):
        say("PASS", side, "TLS issuer for %s: %s (a normal public CA)" % (host, name))
    else:
        say("WARN", side, "TLS issuer for %s: %s. Not a known public CA, the network may inspect TLS." % (host, name))


def check_url(url):
    host = urllib.parse.urlparse(url).hostname
    if not host:
        say("FAIL", "all", "--url needs a full address, for example https://ews.example.com/")
        return
    check_dns([host], "all")
    if addresses(host)[1]:
        return
    t0 = time.time()
    st, _, err = http_get(url, timeout=15)
    ms = int((time.time() - t0) * 1000)
    if err:
        say("FAIL", "all", "Tunnel address %s: %s" % (url, err))
    elif st >= 500:
        say("WARN", "all", "Tunnel address %s answers HTTP %s in %d ms: the network is fine, but the tunnel or "
            "the backend behind it is not up." % (url, st, ms))
    else:
        say("PASS", "all", "Tunnel address %s answers HTTP %s in %d ms" % (url, st, ms))


def check_srv():
    if shutil.which("dig"):
        cmd = ["dig", "+short", "+time=3", "+tries=1", "SRV", SRV_NAME]
    elif shutil.which("nslookup"):
        cmd = ["nslookup", "-type=SRV", SRV_NAME]
    else:
        say("SKIP", "server", "SRV lookup skipped (no dig or nslookup)")
        return
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=15).stdout
    except Exception as e:
        say("SKIP", "server", "SRV lookup skipped (%s)" % e)
        return
    if "7844" in out and "argotunnel.com" in out:
        say("PASS", "server", "DNS SRV %s answers (cloudflared uses it to find the edge)" % SRV_NAME)
    else:
        say("WARN", "server", "DNS SRV %s gave no answer. cloudflared may still work, the tunnel test decides." % SRV_NAME)


def check_edge():
    up, dns_fail = 0, 0
    for h in EDGE_HOSTS:
        ok, ms, err = tcp_open(h, 7844)
        if ok:
            up += 1
            say("PASS", "server", "TCP %s:7844 open (%d ms)" % (h, ms))
        elif isinstance(err, socket.gaierror):
            dns_fail += 1
            say("WARN", "server", "TCP %s:7844 not tried, the name does not resolve" % h)
        else:
            kind = "timeout, probably dropped by a firewall" if isinstance(err, socket.timeout) else str(err)
            say("WARN", "server", "TCP %s:7844 failed after %d ms (%s)" % (h, ms, kind))
    if up == 0 and dns_fail == len(EDGE_HOSTS):
        say("FAIL", "server", "The Cloudflare tunnel hosts do not resolve (DNS). A tunnel cannot start on this network.")
    elif up == 0:
        say("FAIL", "server", "TCP 7844 is blocked to both Cloudflare regions. A tunnel cannot start on this network.")


# ---------- real quick tunnel ----------

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        body = (TOKEN + "\n").encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


def find_runner():
    """Docker first (it is what we will use for the demo), then a local cloudflared."""
    dk = shutil.which("docker")
    if dk:
        try:
            p = subprocess.run([dk, "version", "--format", "{{.Server.Version}}"],
                               capture_output=True, text=True, timeout=15)
            if p.returncode == 0 and p.stdout.strip():
                return "docker", dk
        except Exception:
            pass
    cf = shutil.which("cloudflared")
    return ("cloudflared", cf) if cf else (None, None)


def tunnel_command(kind, path, port, protocol):
    flags = ["tunnel", "--no-autoupdate"] + (["--protocol", protocol] if protocol != "auto" else [])
    if kind == "docker":
        cmd = [path, "run", "--rm", "--name", CONTAINER]
        if sys.platform.startswith("linux"):
            cmd += ["--add-host", "host.docker.internal:host-gateway"]
        return cmd + ["cloudflare/cloudflared:latest"] + flags + ["--url", "http://host.docker.internal:%d" % port]
    return [path] + flags + ["--url", "http://127.0.0.1:%d" % port]


def check_quick_tunnel(protocol, wait, fetch=http_get):
    kind, path = find_runner()
    if not kind:
        say("SKIP", "server", "Quick tunnel test skipped: no running Docker and no cloudflared found "
            "(start Docker Desktop, or: brew install cloudflared).")
        return
    bind = "0.0.0.0" if (kind == "docker" and sys.platform.startswith("linux")) else "127.0.0.1"
    server = ThreadingHTTPServer((bind, 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    cmd = tunnel_command(kind, path, server.server_address[1], protocol)
    say("INFO", "server", "Starting a real quick tunnel with %s (waiting up to %d s) ..." % (kind, wait))
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
    lines, log = queue.Queue(), []
    seen = {"url": None, "proto": None, "registered": False}

    def pump():
        for line in proc.stdout:
            lines.put(line.rstrip())
        lines.put(None)

    def drain(block):
        try:
            line = lines.get(timeout=1) if block else lines.get_nowait()
        except queue.Empty:
            return True
        if line is None:
            return False
        log.append(line)
        m = URL_RE.search(line)
        if m and not seen["url"] and "api.trycloudflare.com" not in m.group(0):
            seen["url"] = m.group(0)
        p = PROTO_RE.search(line)
        if p:
            seen["proto"] = p.group(1)
        if "Registered tunnel connection" in line:
            seen["registered"] = True
        return True

    threading.Thread(target=pump, daemon=True).start()
    deadline = time.time() + wait
    last_err = None
    try:
        alive = True
        while alive and time.time() < deadline and not seen["url"]:
            alive = drain(True)
        if not seen["url"]:
            say("FAIL", "server", "No quick tunnel address appeared. Last log lines:\n         | "
                + "\n         | ".join(log[-6:] or ["(no output)"]))
            return
        say("INFO", "server", "Tunnel address created: %s. Waiting for the tunnel to connect ..." % seen["url"])
        while alive and time.time() < deadline and not seen["registered"]:
            alive = drain(True)
        while time.time() < deadline:
            while drain(False) and not lines.empty():
                pass
            st, body, err = fetch(seen["url"] + "/", timeout=8)
            if err is None and TOKEN in body:
                state["tunnel_ok"] = True
                say("PASS", "server", "Quick tunnel works end to end: %s (cloudflared protocol: %s)"
                    % (seen["url"], seen["proto"] or "unknown"))
                if seen["proto"] == "http2" and protocol == "auto":
                    say("INFO", "server", "QUIC (UDP) did not work here, cloudflared fell back to HTTP/2 (TCP). That is fine.")
                return
            last_err = err or "HTTP %s" % st
            time.sleep(3)
        hint = (" The new name may not be known to this network's DNS yet, try again in a minute."
                if last_err == "dns" else "")
        say("FAIL", "server", "Tunnel address %s does not answer (%s).%s Last log lines:\n         | %s"
            % (seen["url"], last_err, hint, "\n         | ".join(log[-6:] or ["(no output)"])))
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=8)
        except Exception:
            proc.kill()
        if kind == "docker":
            subprocess.run([path, "rm", "-f", CONTAINER], capture_output=True)
        server.shutdown()


# ---------- main ----------

def summary(args):
    print("\n==== Summary ====")
    count = dict((s, sum(1 for r in results if r[0] == s)) for s in ("PASS", "WARN", "FAIL", "SKIP"))
    print("PASS %(PASS)d, WARN %(WARN)d, FAIL %(FAIL)d, SKIP %(SKIP)d" % count)
    if state["no_network"]:
        print("No network. Join the Wi-Fi (and log in to any login page), then run this again.")
        return
    all_fail = [r for r in results if r[0] == "FAIL" and r[1] == "all"]
    srv_fail = [r for r in results if r[0] == "FAIL" and r[1] == "server"]
    print("Every laptop (HTTPS to Cloudflare): %s" % ("PROBLEM, see the FAIL lines" if all_fail else "OK"))
    if all_fail:
        print("Plan B for a laptop: use the phone hotspot, or ask university IT to allow HTTPS to Cloudflare.")
    if args.client:
        return
    if srv_fail or all_fail:
        print("This computer as tunnel host:       PROBLEM, see the FAIL lines")
        print("Plan B: put this Mac on the phone hotspot. The other laptops can stay on the university Wi-Fi")
        print("if their own check is OK (they only need HTTPS, port 443).")
    elif state["tunnel_ok"]:
        print("This computer as tunnel host:       OK (a real quick tunnel worked)")
    else:
        print("This computer as tunnel host:       LOOKS OK, but the real tunnel test did not run or was skipped")


def main():
    ap = argparse.ArgumentParser(description="Will this network work with our Cloudflare Tunnel?")
    ap.add_argument("--client", action="store_true", help="test only what every laptop needs (HTTPS to Cloudflare)")
    ap.add_argument("--url", help="also test the real tunnel address, for example https://ews.example.com/")
    ap.add_argument("--no-tunnel", action="store_true", help="do not start a real quick tunnel")
    ap.add_argument("--protocol", choices=["auto", "http2", "quic"], default="auto",
                    help="cloudflared protocol for the quick tunnel (default: auto)")
    ap.add_argument("--wait", type=int, default=90, help="seconds to wait for the quick tunnel (default: 90)")
    args = ap.parse_args()
    print("EcoWattSense network check, %s, %s, Python %s"
          % (time.strftime("%Y-%m-%d %H:%M:%S"), platform.platform(), platform.python_version()))
    try:
        section("Basics (every laptop)")
        check_basics()
        if state["no_network"]:
            summary(args)
            return 1
        check_dns(["www.cloudflare.com"], "all")
        ok, body = check_https(TRACE_URL, "all", "www.cloudflare.com")
        info = dict(l.split("=", 1) for l in body.splitlines() if "=" in l)
        if ok and info.get("ip"):
            say("INFO", "all", "Public address %s, Cloudflare location %s, country %s, WARP %s"
                % (info.get("ip"), info.get("colo", "?"), info.get("loc", "?"), info.get("warp", "?")))
        check_issuer("www.cloudflare.com", "all")
        if args.url:
            check_url(args.url)
        if not args.client:
            section("Tunnel host (this computer runs cloudflared)")
            check_dns(EDGE_HOSTS, "server", level="WARN", expect=EDGE_RANGE)
            check_srv()
            check_edge()
            check_https(API_URL, "server", "api.cloudflare.com")
            check_https("https://api.trycloudflare.com/", "server", "api.trycloudflare.com (quick tunnels only)",
                        level="WARN")
            if not args.no_tunnel:
                check_quick_tunnel(args.protocol, args.wait)
    except KeyboardInterrupt:
        print("\nStopped.")
    summary(args)
    return 1 if any(r[0] == "FAIL" for r in results) else 0


if __name__ == "__main__":
    sys.exit(main())
