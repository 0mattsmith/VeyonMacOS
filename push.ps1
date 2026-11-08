<#
.SYNOPSIS
    Commits all changes and pushes them to your private GitHub repository (never to veyon/veyon).

.DESCRIPTION
    push.ps1 "What changed" [--bump [patch|minor|major]] [--skip-ci]

      "What changed"   Commit message (required unless there is nothing to commit)
      --bump           Increase the version in CMakeLists.txt (patch by default) and push a tag "vX.Y.Z",
                       which makes GitHub Actions build release packages (.pkg/.dmg) for it
      --skip-ci        Don't run GitHub Actions for this push (saves macOS build minutes)

    On the first run, the script signs in to GitHub (browser), creates the private repository
    <your account>/VeyonMacOS and sets the commit author. The original Veyon repository is kept as the
    fetch-only remote "upstream" - pushing to it is blocked.

.EXAMPLE
    .\push.ps1 "Fix volume lock on Windows 11"
    .\push.ps1 "Website filter presets" --bump
    .\push.ps1 "New add-on store layout" --bump minor --skip-ci
#>

$ErrorActionPreference = 'Stop'

$RepositoryName = 'VeyonMacOS'
$AuthorName = 'Matt Smith'
$UpstreamPattern = 'github\.com[:/]veyon/veyon(\.git)?$'

function Fail( [string] $message )
{
    Write-Host "error: $message" -ForegroundColor Red
    exit 1
}

# Native commands are checked with $LASTEXITCODE - their messages on stderr (e.g. git's progress output)
# must not be treated as errors by Windows PowerShell

# runs a command and stops the script if it fails
function Invoke-Checked
{
    $ErrorActionPreference = 'Continue'
    $command, $arguments = $args
    & $command @arguments
    if( $LASTEXITCODE -ne 0 ) { Fail "$command $($arguments -join ' ') failed" }
}

# runs a command and returns its output, or an empty string if it fails
function Get-Output
{
    $ErrorActionPreference = 'Continue'
    $command, $arguments = $args
    $output = & $command @arguments 2> $null
    if( $LASTEXITCODE -ne 0 ) { return '' }
    return ( $output -join "`n" ).Trim()
}

# runs a command silently and returns whether it succeeded
function Test-Success
{
    $ErrorActionPreference = 'Continue'
    $command, $arguments = $args
    & $command @arguments *> $null
    return $LASTEXITCODE -eq 0
}


# --- arguments ---------------------------------------------------------------------------------------------

$arguments = @( $args )
$message = ''
$bump = ''
$skipCi = $false
for( $i = 0; $i -lt $arguments.Count; $i++ )
{
    $argument = [string] $arguments[$i]
    switch -Regex ( $argument )
    {
        '^--?bump$' {
            $bump = 'patch'
            if( $i + 1 -lt $arguments.Count -and [string] $arguments[$i + 1] -match '^(patch|minor|major)$' ) { $bump = $arguments[++$i] }
            break
        }
        '^--?bump=(patch|minor|major)$' { $bump = $Matches[1]; break }
        '^--?skip-?ci$' { $skipCi = $true; break }
        '^(--?help|-h|/\?)$' { Get-Help $PSCommandPath -Detailed; exit 0 }
        '^-' { Fail "unknown option $argument (see .\push.ps1 --help)" }
        default {
            if( $message ) { Fail 'please put the commit message in quotes' }
            $message = $argument
        }
    }
}

Set-Location $PSScriptRoot
$branch = Get-Output git rev-parse --abbrev-ref HEAD
if( -not $branch -or $branch -eq 'HEAD' ) { Fail 'not on a branch' }


# --- GitHub sign-in ----------------------------------------------------------------------------------------

if( -not ( Get-Command gh -ErrorAction SilentlyContinue ) ) { Fail 'GitHub CLI not found - install it with "winget install GitHub.cli"' }

if( -not ( Test-Success gh auth status ) )
{
    Write-Host 'Signing in to GitHub - follow the instructions (a browser window opens) ...' -ForegroundColor Cyan
    Invoke-Checked gh auth login --hostname github.com --git-protocol https --web
}
# let git use the GitHub CLI login for pushing
Invoke-Checked gh auth setup-git

$login = Get-Output gh api user --jq .login
if( -not $login ) { Fail 'could not determine your GitHub account' }


# --- remotes: "upstream" = veyon/veyon (fetch only), "origin" = your private repository --------------------

$originUrl = Get-Output git remote get-url origin
if( $originUrl -match $UpstreamPattern )
{
    if( Get-Output git remote get-url upstream ) { Fail 'remotes "origin" and "upstream" both exist - please check "git remote -v"' }
    Write-Host 'Renaming remote "origin" (veyon/veyon) to "upstream"' -ForegroundColor Cyan
    Invoke-Checked git remote rename origin upstream
    $originUrl = ''
}
if( Get-Output git remote get-url upstream )
{
    Invoke-Checked git remote set-url --push upstream 'DISABLED-do-not-push-to-upstream'
}

if( -not $originUrl )
{
    if( -not ( Test-Success gh repo view "$login/$RepositoryName" ) )
    {
        Write-Host "Creating private repository $login/$RepositoryName" -ForegroundColor Cyan
        Invoke-Checked gh repo create "$login/$RepositoryName" --private --description 'Veyon for macOS with add-on store and add-ons'
    }
    Invoke-Checked git remote add origin "https://github.com/$login/$RepositoryName.git"
    $originUrl = Get-Output git remote get-url origin
}

if( $originUrl -match $UpstreamPattern ) { Fail 'remote "origin" points to veyon/veyon - refusing to push' }


# --- commit author (GitHub's private no-reply address hides your email) ------------------------------------

if( -not ( Get-Output git config user.name ) ) { Invoke-Checked git config user.name $AuthorName }
if( -not ( Get-Output git config user.email ) )
{
    $id = Get-Output gh api user --jq .id
    if( -not $id ) { Fail 'could not determine your GitHub account' }
    Invoke-Checked git config user.email "$id+$login@users.noreply.github.com"
}


# --- version bump ------------------------------------------------------------------------------------------

$tag = ''
if( $bump )
{
    $cmakeFile = Join-Path $PSScriptRoot 'CMakeLists.txt'
    $cmake = [IO.File]::ReadAllText( $cmakeFile )
    $version = @{}
    foreach( $part in 'MAJOR', 'MINOR', 'PATCH' )
    {
        if( $cmake -notmatch "set\(VERSION_$part (\d+)\)" ) { Fail "VERSION_$part not found in CMakeLists.txt" }
        $version[$part] = [int] $Matches[1]
    }
    switch( $bump )
    {
        'major' { $version['MAJOR'] += 1; $version['MINOR'] = 0; $version['PATCH'] = 0 }
        'minor' { $version['MINOR'] += 1; $version['PATCH'] = 0 }
        'patch' { $version['PATCH'] += 1 }
    }
    foreach( $part in 'MAJOR', 'MINOR', 'PATCH' )
    {
        $cmake = $cmake -replace "set\(VERSION_$part \d+\)", "set(VERSION_$part $($version[$part]))"
    }

    $tag = "v$($version['MAJOR']).$($version['MINOR']).$($version['PATCH'])"
    if( Get-Output git tag --list $tag ) { Fail "tag $tag already exists - use --bump minor or --bump major" }

    [IO.File]::WriteAllText( $cmakeFile, $cmake )
    Write-Host "Version bumped to $($tag.Substring(1))" -ForegroundColor Cyan
}


# --- commit ------------------------------------------------------------------------------------------------

Invoke-Checked git add -A
if( -not ( Test-Success git diff --cached --quiet ) )
{
    if( -not $message ) { Fail 'please describe your changes: .\push.ps1 "What changed"' }
    if( $skipCi ) { $message += "`n`n[skip ci]" }
    $messageFile = ( New-TemporaryFile ).FullName
    [IO.File]::WriteAllText( $messageFile, $message )
    Invoke-Checked git commit --quiet --file $messageFile
    Remove-Item $messageFile
    Write-Host ( Get-Output git log -1 --oneline ) -ForegroundColor Green
}
else
{
    Write-Host 'Nothing to commit - pushing existing commits' -ForegroundColor Yellow
}

if( $tag )
{
    $tagMessage = if( $message ) { ( $message -split "`n" )[0] } else { "Veyon $($tag.Substring(1))" }
    Invoke-Checked git tag --annotate $tag --message $tagMessage
}


# --- push --------------------------------------------------------------------------------------------------

Invoke-Checked git push --set-upstream origin $branch
if( $tag ) { Invoke-Checked git push origin $tag }

$repositoryUrl = $originUrl -replace '\.git$', ''
Write-Host ''
Write-Host "Pushed $branch$(if( $tag ) { " and $tag" }) to $repositoryUrl" -ForegroundColor Green
if( -not $skipCi ) { Write-Host "Builds: $repositoryUrl/actions" }
