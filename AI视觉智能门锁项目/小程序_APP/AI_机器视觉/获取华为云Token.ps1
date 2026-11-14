param(
    [string]$ProjectId = '019e61b85cf57ce39d924a0022346239'
)

$ErrorActionPreference = 'Stop'
$tokenUri = 'https://iam.myhuaweicloud.com/v3/auth/tokens?nocatalog=true'

Write-Host ''
Write-Host 'Huawei Cloud IoTDA Token Helper' -ForegroundColor Cyan
Write-Host 'Your password is used only for this request and is not saved.' -ForegroundColor DarkGray
Write-Host ''

$accountName = (Read-Host 'Enter Huawei Cloud ACCOUNT name (not phone/email, not IAM user name)').Trim()
$iamUserName = (Read-Host 'Enter IAM USER name (IAM > Users)').Trim()
$securePassword = Read-Host 'Enter IAM USER password' -AsSecureString

Write-Host ''
Write-Host "Account name: $accountName" -ForegroundColor DarkGray
Write-Host "IAM user name: $iamUserName" -ForegroundColor DarkGray
Write-Host "Project ID: $ProjectId" -ForegroundColor DarkGray
Write-Host ''

if ([string]::IsNullOrWhiteSpace($accountName)) { throw 'Account name cannot be empty.' }
if ([string]::IsNullOrWhiteSpace($iamUserName)) { throw 'IAM user name cannot be empty.' }
if ([string]::IsNullOrWhiteSpace($ProjectId)) { throw 'Project ID cannot be empty.' }

$bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($securePassword)
try {
    $plainPassword = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
} finally {
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
}

$bodyObject = @{
    auth = @{
        identity = @{
            methods = @('password')
            password = @{
                user = @{
                    name = $iamUserName
                    password = $plainPassword
                    domain = @{
                        name = $accountName
                    }
                }
            }
        }
        scope = @{
            project = @{
                id = $ProjectId
            }
        }
    }
}

$json = $bodyObject | ConvertTo-Json -Depth 10 -Compress
$bodyBytes = [Text.Encoding]::UTF8.GetBytes($json)

try {
    $response = Invoke-WebRequest `
        -Uri $tokenUri `
        -Method Post `
        -ContentType 'application/json; charset=utf-8' `
        -Body $bodyBytes `
        -UseBasicParsing

    $token = $response.Headers['X-Subject-Token']
    if ($token -is [array]) { $token = $token[0] }
    if ([string]::IsNullOrWhiteSpace($token)) {
        throw 'Request succeeded, but X-Subject-Token was not found in the response headers.'
    }

    Set-Clipboard -Value $token

    Write-Host ''
    Write-Host 'SUCCESS: Token copied to clipboard.' -ForegroundColor Green
    Write-Host "Token length: $($token.Length)" -ForegroundColor DarkGray
    Write-Host ''
    Write-Host "Open utils/huaweiCloud.js and replace token: '' with the copied value." -ForegroundColor Yellow
} catch {
    Write-Host ''
    Write-Host 'ERROR: Failed to get token.' -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red

    if ($_.ErrorDetails -and $_.ErrorDetails.Message) {
        Write-Host 'Server response:' -ForegroundColor DarkYellow
        Write-Host $_.ErrorDetails.Message -ForegroundColor DarkRed
    }

    if ($_.Exception.Response) {
        try {
            $stream = $_.Exception.Response.GetResponseStream()
            if ($stream) {
                $reader = New-Object IO.StreamReader($stream)
                $errorBody = $reader.ReadToEnd()
                $reader.Dispose()
                if ($errorBody) { Write-Host $errorBody -ForegroundColor DarkRed }
            }
        } catch {
            # Ignore errors while reading the HTTP error response.
        }
    }

    exit 1
} finally {
    $plainPassword = $null
}
