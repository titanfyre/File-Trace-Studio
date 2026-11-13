$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'lib/FileTraceStudioJsonSDKPowerShell.dll')
function Connect-FileTraceStudioSDK { <# .SYNOPSIS Opens an inert product SDK session. #> [CmdletBinding()]param() [TitanCSA.Products.FileTraceStudio.PowerShell.Client]::new() }
function Invoke-FileTraceStudioSDK { <# .SYNOPSIS Calls a documented product operation with an argument object. #> [CmdletBinding()]param([Parameter(Mandatory)]$Client,[Parameter(Mandatory)][string]$Operation,[hashtable]$Arguments=@{}) $Client.Call($Operation,($Arguments|ConvertTo-Json -Depth 100 -Compress)) | ConvertFrom-Json -Depth 100 }
function Disconnect-FileTraceStudioSDK { <# .SYNOPSIS Closes the SDK session and owned resources. #> [CmdletBinding()]param([Parameter(Mandatory)]$Client) $Client.Dispose() }
Export-ModuleMember -Function Connect-FileTraceStudioSDK,Invoke-FileTraceStudioSDK,Disconnect-FileTraceStudioSDK
