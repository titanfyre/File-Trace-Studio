# Validation and release status

This is an engineering preview. No driver installation, loading, volume attachment or live capture has been performed on the host or a VM for this product.

Completed locally:

- Native GUI, CLI and SDK builds with MSVC14.44, C++17, `/W4 /WX`.
- Two CTest suites in Release and AddressSanitizer configurations.
- More than 50,000 assertions: malformed/truncated/oversized records, header/version/count/frequency/PID/flag bounds, completion sequence validation, warning/error and successful-byte accounting, QPC conversion, strict PID parsing, bounded visualization history, JSON export/no-overwrite and 50,000 malformed decoder fuzz inputs.
- Concurrent epoch/stop/start semantic model: stale completions do not enter a restarted capture, maximum256 modeled contexts and complete release after stop. This tests the policy model; it is not execution or verification of the kernel implementation.
- Original private C minifilter compiled with WDK26100, `/W4 /WX` and PREfast using installed Spectre libraries. Static analysis is not runtime proof.
- CAB generation is gated on a real, explicitly confirmed Microsoft altitude allocation. No sample altitude or installable INF/CAB is provided.
- The screenshot is generated through `FileTraceStudio.exe --preview <absolute path.png>` using labelled synthetic data; this never connects to a driver.

Required before live distribution: actual altitude allocation, INF policy/catalog validation with that allocation, Microsoft signing, signed installation/removal/reboot on supported Windows10/11 builds, NTFS/ReFS coverage, process exit/PID reuse, real error and transfer-size fixtures, overflow, close/crash/restart with pending I/O, control-port malformed buffers/low-resource stress, Driver Verifier, BypassIO compatibility, and measured filesystem overhead under representative workloads.

No Windows Server, ARM64, WHQL, antivirus, ransomware protection, complete filesystem coverage or zero-overhead claim is made. The binary has no enforced timer; evaluation/commercial authorization is separate from its technical preview status.
