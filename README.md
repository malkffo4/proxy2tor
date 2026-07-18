# Proxy2Tor Bridge

**Proxy2Tor** is a lightweight networking utility designed to bridge standard TCP connections to Tor Hidden Services (.onion). It acts as a local transparent proxy, forwarding all incoming traffic from a specified local port to a destination within the Tor network.

---

## Disclaimer

**For Educational and Authorized Security Testing Purposes Only.**
The use of this tool for attacking targets without prior mutual consent is illegal. The developer assumes no liability and is not responsible for any misuse or damage caused by this program. Users are responsible for complying with all applicable local, state, and federal laws.

---

## How It Works

Proxy2Tor sets up a local listener (Bridge). When a connection is received, it encapsulates the traffic and tunnels it through a local Tor instance to a pre-defined Onion service.

1. **Local Tool** (e.g., Telnet) -> connects to `127.0.0.1:LOCAL_PORT`
2. **Proxy2Tor** -> intercepts traffic.
3. **Tor Service** -> routes traffic via Tor nodes.
4. **Destination** -> `target.onion:PORT`

---

## Usage

### Prerequisites

You must have a running Tor instance on your machine. You can download it from the [Official Tor Project page](https://www.torproject.org/download/tor/).

### Command Line Arguments

```bash
proxy2tor.exe --bridge-addr <ADDR> --bridge-port <PORT> --tor-addr <ADDR> --tor-port <PORT> --file <ONION_LIST>

```

| Argument | Description |
| --- | --- |
| `--bridge-addr` | Local IP to bind the bridge (e.g., `127.0.0.1`) |
| `--bridge-port` | Local port to listen for incoming connections |
| `--tor-addr` | IP address of your Tor SOCKS proxy (default: `127.0.0.1`) |
| `--tor-port` | Port of your Tor SOCKS proxy (default: `9050`) |
| `--file` | Path to the configuration file containing onion addresses |

### Configuration File Format

The `--file` argument expects a text file with the following format:

```text
site1.onion:8888
site2.onion:9933

```

---

## Example: Routing Meterpreter through Tor

1. **Start Tor** on your local machine (port 9050).
2. **Run Proxy2Tor**:
```bash
proxy2tor.exe --bridge-addr 127.0.0.1 --bridge-port 4444 --tor-addr 127.0.0.1 --tor-port 9050 --file targets.txt

```


3. **Configure your Listener**: Set your LHOST to `127.0.0.1` and LPORT to `4444`. The traffic will be automatically forwarded to the onion address specified in `targets.txt`.

---

## License

This project is licensed under the MIT License - see the [LICENSE](https://www.google.com/search?q=LICENSE) file for details.
