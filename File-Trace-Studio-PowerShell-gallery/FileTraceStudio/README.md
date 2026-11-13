# File-Trace-Studio PowerShell SDK

PowerShell7.4+ Windows x64. `Install-Module FileTraceStudio -Scope CurrentUser`.

```powershell
$client=Connect-FileTraceStudioSDK
try { Invoke-FileTraceStudioSDK -Client $client -Operation status } finally { Disconnect-FileTraceStudioSDK -Client $client }
```

Use -Arguments with a hashtable matching [schema](docs/API.md). Native errors retain their Windows code through the inner exception. Matching driver installed separately.
