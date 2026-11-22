@echo off
rem Install Game Files: part of Big Red Launcher by ctrl-alt-rees (ctrl-alt-rees.com).
rem Copyright (C) 2026 ctrl-alt-rees. This is free software under the GNU General Public
rem License, version 3 or (at your option) any later version: see LICENSE.txt. It comes with
rem NO WARRANTY. Big Red Racing itself is not included, and not covered by this licence.
rem
rem Makes the Game and Music folders next to Big Red Racing.exe from your own copy of the
rem two Big Red Racing zips from myabandonware.com (the disc image and the v2.25 patch).
rem The work is done by Windows PowerShell (part of Windows), with the script below the
rem marker line: nothing else needs installing.
rem
rem Windows 95, 98 and ME run this with COMMAND.COM, which has no PowerShell, nor cmd.exe's
rem setlocal or exit /b; only Windows NT and its successors set OS to Windows_NT. So they
rem are told why first, as is a newer Windows without PowerShell (XP). COMMAND.COM can't
rem read the quoted "if exist", hence the order. Labels: 8 letters at most, for COMMAND.COM.
if not "%OS%"=="Windows_NT" goto no_ps
if exist "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" goto has_ps
:no_ps
echo.
echo This script requires Windows PowerShell and therefore only runs on modern
echo Windows. To set up the game files, either re-run this script under modern
echo Windows and copy the files over, or follow the instructions in the README.
echo.
pause
if "%OS%"=="Windows_NT" exit /b 1
rem COMMAND.COM may read on past an exit (DOSBox-X's does): the label is the file's last line
goto brr_end
:has_ps
setlocal
set "BRR_SETUP=%~f0"
cd /d "%~dp0"
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -Command "$s = [IO.File]::ReadAllText($env:BRR_SETUP); $m = '#' + '#BRR-SETUP-POWERSHELL#' + '#'; Invoke-Expression $s.Substring($s.IndexOf($m) + $m.Length)"
set "BRR_RESULT=%ERRORLEVEL%"
echo.
pause
exit /b %BRR_RESULT%
##BRR-SETUP-POWERSHELL##
# ---- Windows PowerShell 5.1 (Windows 10 and 11); also runs in PowerShell 7 ----
# Made by "python3 brr.py setup-build" from src/setup/install-game-files.bat and the facts
# in data/ (the game's files with their sizes and SHA-256 checksums, the CD's tracks).
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

#@DATA@

$Root = Split-Path -Parent $env:BRR_SETUP
$GameDir = Join-Path $Root 'Game'
$MusicDir = Join-Path $Root 'Music'
$ToolsDir = Join-Path $Root 'Tools'
$Sep = [IO.Path]::DirectorySeparatorChar
$RawSector = 2352
$MB = 1048576

# ---- talking to the player ----
function Say([string]$Text = '', [string]$Colour = '') {
    if ($Colour) { Write-Host $Text -ForegroundColor $Colour } else { Write-Host $Text }
}

function Heading([string]$Text) {
    Say ''
    Say $Text 'Cyan'
}

function Fail([string]$Text) { throw $Text }

function Ask-YesNo([string]$Question, [bool]$Default) {
    $hint = if ($Default) { '[Y/n]' } else { '[y/N]' }
    while ($true) {
        $answer = Read-Host "$Question $hint"
        if ($null -eq $answer) { return $Default }
        $answer = $answer.Trim().ToLowerInvariant()
        if ($answer -eq '') { return $Default }
        if ($answer -eq 'y' -or $answer -eq 'yes') { return $true }
        if ($answer -eq 'n' -or $answer -eq 'no') { return $false }
        Say '  Please answer Y or N.' 'Yellow'
    }
}

function Ask-Number([string]$Question, [int]$Count, [int]$Default) {
    while ($true) {
        $answer = Read-Host "$Question [1-$Count, Enter for $Default]"
        if ($null -eq $answer -or $answer.Trim() -eq '') { return $Default }
        $number = 0
        if ([int]::TryParse($answer.Trim(), [ref]$number) -and $number -ge 1 -and $number -le $Count) { return $number }
        Say "  Please type a number from 1 to $Count." 'Yellow'
    }
}

function Size-Text([double]$Bytes) {
    if ($Bytes -ge 1GB) { return ('{0:N1} GB' -f ($Bytes / 1GB)) }
    if ($Bytes -lt 10 * $MB) { return ('{0:N1} MB' -f ([math]::Ceiling($Bytes / $MB * 10) / 10)) }
    return ('{0:N0} MB' -f [math]::Ceiling($Bytes / $MB))
}

# ---- files and checksums ----
function Hex([byte[]]$Bytes) { return ([BitConverter]::ToString($Bytes) -replace '-', '').ToLowerInvariant() }

function File-Sha256([string]$Path) {
    $sha = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($Path)
    try { return (Hex ($sha.ComputeHash($stream))) } finally { $stream.Dispose(); $sha.Dispose() }
}

# a path given with / separators, inside a folder
function Local-Path([string]$Base, [string]$Relative) {
    return (Join-Path $Base ($Relative -replace '/', $Sep))
}

function IDiv([long]$A, [long]$B) { return [long][math]::Floor($A / $B) }

function Read-Exactly($Stream, [byte[]]$Buffer, [int]$Count) {
    $done = 0
    while ($done -lt $Count) {
        $read = $Stream.Read($Buffer, $done, $Count - $done)
        if ($read -le 0) { Fail 'The disc image in the zip ends early: the zip may be damaged. Download it again.' }
        $done += $read
    }
}

function Find-Zip([string]$Pattern, [string]$Name) {
    $exact = Join-Path $Root $Name
    if (Test-Path -LiteralPath $exact -PathType Leaf) { return $exact }
    $found = @(Get-ChildItem -LiteralPath $Root -Filter $Pattern -File -ErrorAction SilentlyContinue | Sort-Object Name)
    if ($found.Count -gt 0) { return $found[0].FullName }
    return $null
}

function Open-Zip([string]$Path) {
    try { return [IO.Compression.ZipFile]::OpenRead($Path) }
    catch { Fail "Can't open $(Split-Path -Leaf $Path) as a zip ($($_.Exception.Message)). Download it again." }
}

# The game's files as recorded: name, where each comes from, its size and SHA-256
function Game-Files {
    $list = @()
    foreach ($line in ($GameFileList -split "`r?`n")) {
        if (-not $line.Trim()) { continue }
        $part = $line.Split('|')
        $list += New-Object PSObject -Property @{ Name = $part[0]; Origin = $part[1]; Size = [long]$part[2]; Sha256 = $part[3] }
    }
    return $list
}

function Same-File([string]$Path, $File) {
    return ((Get-Item -LiteralPath $Path).Length -eq $File.Size -and (File-Sha256 $Path) -eq $File.Sha256)
}

# Which of the game's files are missing from the Game folder, or not the recorded file. The
# installer's settings (DEFAULT.CFG, DIG.INI, VIDEO.BAT) only need to be there: the DOS sound
# setup rewrites them. RACING.EXE may be patched by Big Red Launcher's DOS patch (or
# BRRPATCH), which keeps the original as RACING.ORI.
function Check-Game($Files) {
    $wrong = @()
    $i = 0
    foreach ($file in $Files) {
        $i++
        if ($i % 100 -eq 0) { Write-Host -NoNewline "`r  checking: $i of $($Files.Count)" }
        $path = Local-Path $GameDir $file.Name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { $wrong += $file; continue }
        if ($file.Origin -eq 'installer' -or (Same-File $path $file)) { continue }
        $original = Local-Path $GameDir 'RACING.ORI'
        if ($file.Name -eq 'RACING.EXE' -and (Test-Path -LiteralPath $original -PathType Leaf) -and (Same-File $original $file)) { continue }
        $wrong += $file
    }
    Write-Host -NoNewline "`r                              `r"
    return $wrong
}

# Write a game file, check it against the record and set its date; $true if it matches
function Write-GameFile($File, [byte[]]$Data, $When) {
    $path = Local-Path $GameDir $File.Name
    $folder = Split-Path -Parent $path
    if (-not (Test-Path -LiteralPath $folder)) { New-Item -ItemType Directory -Path $folder -Force | Out-Null }
    if (Test-Path -LiteralPath $path -PathType Leaf) { (Get-Item -LiteralPath $path).IsReadOnly = $false }
    [IO.File]::WriteAllBytes($path, $Data)
    $sha = [Security.Cryptography.SHA256]::Create()
    $digest = Hex ($sha.ComputeHash($Data))
    $sha.Dispose()
    if ($When) { (Get-Item -LiteralPath $path).LastWriteTime = $When }
    return ($Data.Length -eq $File.Size -and $digest -eq $File.Sha256)
}

# ---- the disc image: its CUE sheet and its ISO 9660 file system ----
function Read-Cue([string]$Text, [long]$TotalSectors) {
    $tracks = @()
    $files = 0
    $current = $null
    foreach ($raw in ($Text -split "`r?`n")) {
        $line = $raw.Trim()
        if (-not $line -or $line -match '^REM\b') { continue }
        if ($line -match '^FILE\s') {
            $files++
            if ($line -notmatch '\sBINARY$') { Fail 'The disc image''s CUE sheet names a file that isn''t BINARY: this isn''t the known disc image.' }
        } elseif ($line -match '^TRACK\s+(\d+)\s+(\S+)') {
            $current = New-Object PSObject -Property @{ Number = [int]$Matches[1]; Mode = $Matches[2].ToUpperInvariant(); Start = -1L; Sectors = 0L }
            $tracks += $current
        } elseif ($line -match '^INDEX\s+01\s+(\d+):(\d+):(\d+)' -and $current) {
            $current.Start = ([long]$Matches[1] * 60 + [long]$Matches[2]) * 75 + [long]$Matches[3]
        }
    }
    if ($files -ne 1 -or $tracks.Count -lt 2) { Fail 'The disc image''s CUE sheet isn''t one BIN file with several tracks: this isn''t the known disc image.' }
    for ($i = 0; $i -lt $tracks.Count; $i++) {
        $next = if ($i + 1 -lt $tracks.Count) { $tracks[$i + 1].Start } else { $TotalSectors }
        $tracks[$i].Sectors = $next - $tracks[$i].Start
        if ($tracks[$i].Start -lt 0 -or $tracks[$i].Sectors -le 0) { Fail "The disc image's CUE sheet has a track $($tracks[$i].Number) that can't be read." }
    }
    if ($tracks[0].Mode -ne 'MODE1/2352' -or $tracks[0].Start -ne 0) { Fail 'The disc image''s first track isn''t a data track (MODE1/2352) at its start.' }
    return $tracks
}

# Where an ISO sector's 2,048 data bytes are in the data track's raw 2,352-byte sectors
function Iso-Offset([long]$Sector) { return [int]($Sector * $RawSector + 16) }

function Iso-Time([byte[]]$Data, [int]$At) {
    try {
        return New-Object DateTime ([int](1900 + $Data[$At]), [int]$Data[$At + 1], [int]$Data[$At + 2],
                                    [int]$Data[$At + 3], [int]$Data[$At + 4], [int]$Data[$At + 5])
    } catch { return $null }
}

# Every file on the CD, by its upper-case path: where it starts, its size and its date
function Read-Iso([byte[]]$Data) {
    $pvd = Iso-Offset 16
    if ($Data[$pvd] -ne 1 -or [Text.Encoding]::ASCII.GetString($Data, $pvd + 1, 5) -ne 'CD001') {
        Fail 'The disc image has no ISO 9660 file system where expected: this isn''t the known disc image.'
    }
    $files = @{}
    $seen = @{}
    $pending = New-Object Collections.Stack
    $pending.Push(@('', [long][BitConverter]::ToUInt32($Data, $pvd + 158), [long][BitConverter]::ToUInt32($Data, $pvd + 166)))
    while ($pending.Count -gt 0) {
        $dir = $pending.Pop()
        $prefix = $dir[0]; $extent = $dir[1]; $size = $dir[2]
        if ($seen.ContainsKey($extent)) { Fail 'The disc image''s file system has a loop in it.' }
        $seen[$extent] = $true
        $pos = 0L
        while ($pos -lt $size) {
            $at = (Iso-Offset ($extent + (IDiv $pos 2048))) + [int]($pos % 2048)
            if ($at + 34 -gt $Data.Length) { Fail 'The disc image''s file system points past the data track.' }
            $length = [int]$Data[$at]
            if ($length -eq 0) { $pos = ((IDiv $pos 2048) + 1) * 2048; continue }
            $childExtent = [long][BitConverter]::ToUInt32($Data, $at + 2)
            $childSize = [long][BitConverter]::ToUInt32($Data, $at + 10)
            $isDir = ($Data[$at + 25] -band 2) -ne 0
            $nameLength = [int]$Data[$at + 32]
            $name = [Text.Encoding]::ASCII.GetString($Data, $at + 33, $nameLength)
            $pos += $length
            if ($nameLength -eq 1 -and ($Data[$at + 33] -eq 0 -or $Data[$at + 33] -eq 1)) { continue }
            if (-not $isDir) {
                $name = $name.Split(';')[0]
                if ($name.EndsWith('.')) { $name = $name.Substring(0, $name.Length - 1) }
            }
            if (-not $name -or $name.Contains('/') -or $name.Contains('\') -or $name -eq '.' -or $name -eq '..') {
                Fail 'The disc image has an unsafe file name on it.'
            }
            $path = if ($prefix) { "$prefix/$name" } else { $name }
            if ($isDir) { $pending.Push(@($path, $childExtent, $childSize)) }
            else { $files[$path.ToUpperInvariant()] = @{ Extent = $childExtent; Size = $childSize; When = (Iso-Time $Data ($at + 18)) } }
        }
    }
    return $files
}

# A file's bytes from the CD: the 2,048 data bytes of each of its raw sectors, joined
function Iso-Bytes([byte[]]$Data, $Entry) {
    $out = New-Object byte[] ([int]$Entry.Size)
    $done = 0
    $sector = $Entry.Extent
    while ($done -lt $Entry.Size) {
        $count = [int][math]::Min(2048, $Entry.Size - $done)
        $at = Iso-Offset $sector
        if ($at + $count -gt $Data.Length) { Fail 'A file on the disc image runs past the data track.' }
        [Buffer]::BlockCopy($Data, $at, $out, $done, $count)
        $done += $count
        $sector++
    }
    return ,$out
}

# ---- the music converters: downloaded only when needed, into Tools ----
function Tool-Ready($Tool) {
    foreach ($file in $Tool.Files) {
        $path = Local-Path $ToolsDir $file.Target
        if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (File-Sha256 $path) -ne $file.Sha256) { return $false }
    }
    return $true
}

function Download([string]$Url, [string]$Path, [long]$Size) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $request = [Net.HttpWebRequest]::Create($Url)
    $request.UserAgent = "BigRedLauncherSetup/$SetupVersion"
    $response = $request.GetResponse()
    $in = $response.GetResponseStream()
    $out = [IO.File]::Create($Path)
    try {
        $buffer = New-Object byte[] 1048576
        $total = 0L
        $shown = -1
        while (($read = $in.Read($buffer, 0, $buffer.Length)) -gt 0) {
            $out.Write($buffer, 0, $read)
            $total += $read
            $percent = [int][math]::Floor(100 * $total / [math]::Max($Size, 1))
            if ($percent -ne $shown -and $percent -le 100) { Write-Host -NoNewline "`r  downloading: $percent%"; $shown = $percent }
        }
    } finally { $out.Dispose(); $in.Dispose(); $response.Close() }
    Say "`r  downloaded $(Size-Text $total)          "
}

function Get-Tool($Tool) {
    if (Tool-Ready $Tool) { Say "  $($Tool.Label) is already in the Tools folder (checked)."; return }
    if (-not (Test-Path -LiteralPath $ToolsDir)) { New-Item -ItemType Directory -Path $ToolsDir | Out-Null }
    $zipPath = Join-Path $ToolsDir $Tool.Zip
    if ((Test-Path -LiteralPath $zipPath -PathType Leaf) -and (File-Sha256 $zipPath) -eq $Tool.ZipSha256) {
        Say "  using $($Tool.Zip), already in the Tools folder (checked)"
    } else {
        Say "  downloading $($Tool.Label) ($(Size-Text $Tool.ZipSize)), $($Tool.Source):"
        Say "  $($Tool.Url)"
        try { Download $Tool.Url $zipPath $Tool.ZipSize }
        catch {
            if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
            Fail ("The download failed ($($_.Exception.Message)). Check the internet connection and run this again, " +
                  "or download that file yourself, put it in a folder called Tools next to this file, and run this again.")
        }
        if ((File-Sha256 $zipPath) -ne $Tool.ZipSha256) {
            Remove-Item -LiteralPath $zipPath -Force
            Fail "The downloaded $($Tool.Zip) isn't the expected file (its SHA-256 differs), so it was deleted. Try again later."
        }
        Say "  checked: SHA-256 $($Tool.ZipSha256)"
    }
    $zip = Open-Zip $zipPath
    try {
        foreach ($file in $Tool.Files) {
            $entry = $zip.GetEntry($file.Entry)
            if (-not $entry) { Fail "$($Tool.Zip) has no $($file.Entry)." }
            $target = Local-Path $ToolsDir $file.Target
            $folder = Split-Path -Parent $target
            if (-not (Test-Path -LiteralPath $folder)) { New-Item -ItemType Directory -Path $folder -Force | Out-Null }
            $in = $entry.Open()
            $out = [IO.File]::Create($target)
            try { $in.CopyTo($out) } finally { $out.Dispose(); $in.Dispose() }
        }
    } finally { $zip.Dispose() }
    Remove-Item -LiteralPath $zipPath -Force
    if (-not (Tool-Ready $Tool)) { Fail "$($Tool.Label) didn't unpack as expected." }
}

# Start a converter reading raw CD audio from its standard input. BRR_SETUP_RUNNER (only for
# the project's own tests under Linux) runs it through another program, such as wine.
function Start-Converter([string]$Exe, [string]$Arguments) {
    $info = New-Object Diagnostics.ProcessStartInfo
    if ($env:BRR_SETUP_RUNNER) {
        $info.FileName = $env:BRR_SETUP_RUNNER
        $info.Arguments = "`"$Exe`" $Arguments"
    } else {
        $info.FileName = $Exe
        $info.Arguments = $Arguments
    }
    $info.UseShellExecute = $false
    $info.RedirectStandardInput = $true
    return [Diagnostics.Process]::Start($info)
}

function Wav-Header([long]$Bytes) {
    $header = New-Object IO.MemoryStream
    $writer = New-Object IO.BinaryWriter $header
    $writer.Write([Text.Encoding]::ASCII.GetBytes('RIFF')); $writer.Write([uint32](36 + $Bytes))
    $writer.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt ')); $writer.Write([uint32]16)
    $writer.Write([uint16]1); $writer.Write([uint16]2); $writer.Write([uint32]44100)
    $writer.Write([uint32]176400); $writer.Write([uint16]4); $writer.Write([uint16]16)
    $writer.Write([Text.Encoding]::ASCII.GetBytes('data')); $writer.Write([uint32]$Bytes)
    $writer.Flush()
    return ,$header.ToArray()
}

# A FLAC file's STREAMINFO: its total samples, and the MD5 of its audio
function Flac-Info([string]$Path) {
    $head = New-Object byte[] 42
    $in = [IO.File]::OpenRead($Path)
    try { $read = $in.Read($head, 0, 42) } finally { $in.Dispose() }
    if ($read -lt 42 -or [Text.Encoding]::ASCII.GetString($head, 0, 4) -ne 'fLaC') { return $null }
    $samples = (([long]($head[21] -band 15)) -shl 32) + ([long]$head[22] -shl 24) + ([long]$head[23] -shl 16) +
               ([long]$head[24] -shl 8) + [long]$head[25]
    $md5 = New-Object byte[] 16
    [Array]::Copy($head, 26, $md5, 0, 16)
    return @{ Samples = $samples; Md5 = (Hex $md5) }
}

# One audio track from the disc image's stream into the Music folder; its raw audio's
# SHA-256, checked against the CD's
function Convert-Track($Stream, $Track, [string]$Format, $Tool, [byte[]]$Buffer) {
    $name = 'track{0:D2}.{1}' -f $Track.Number, $Format
    $path = Join-Path $MusicDir $name
    $bytes = [long]$Track.Sectors * $RawSector
    $sha = [Security.Cryptography.SHA256]::Create()
    $md5 = $null
    try { $md5 = [Security.Cryptography.MD5]::Create() } catch { $md5 = $null }
    $process = $null
    if ($Format -eq 'wav') {
        $out = [IO.File]::Create($path)
        $header = Wav-Header $bytes
        $out.Write($header, 0, $header.Length)
    } elseif ($Format -eq 'flac') {
        $process = Start-Converter (Local-Path $ToolsDir $Tool.Exe) ("--silent --force-raw-format --endian=little --sign=signed " +
            "--channels=2 --bps=16 --sample-rate=44100 --tag=ALBUM=`"Big Red Racing`" --tag=TRACKNUMBER=$($Track.Number) " +
            "-o `"$path`" -")
        $out = $process.StandardInput.BaseStream
    } else {
        $process = Start-Converter (Local-Path $ToolsDir $Tool.Exe) ("-hide_banner -loglevel error -f s16le -ar 44100 -ac 2 " +
            "-i pipe:0 -c:a libvorbis -q:a 6 -metadata album=`"Big Red Racing`" -metadata track=$($Track.Number) -y `"$path`"")
        $out = $process.StandardInput.BaseStream
    }
    $left = $bytes
    try {
        while ($left -gt 0) {
            $count = [int][math]::Min($Buffer.Length, $left)
            Read-Exactly $Stream $Buffer $count
            $out.Write($Buffer, 0, $count)
            $sha.TransformBlock($Buffer, 0, $count, $null, 0) | Out-Null
            if ($md5) { $md5.TransformBlock($Buffer, 0, $count, $null, 0) | Out-Null }
            $left -= $count
        }
    } finally { $out.Dispose() }
    $sha.TransformFinalBlock((New-Object byte[] 0), 0, 0) | Out-Null
    $digest = Hex $sha.Hash
    $sha.Dispose()
    $md5Hex = $null
    if ($md5) { $md5.TransformFinalBlock((New-Object byte[] 0), 0, 0) | Out-Null; $md5Hex = Hex $md5.Hash; $md5.Dispose() }
    if ($process) {
        $process.WaitForExit()
        if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $path)) {
            Fail "The converter stopped with an error on $name (exit code $($process.ExitCode))."
        }
    }
    if ($Format -eq 'flac') {
        $info = Flac-Info $path
        if (-not $info -or $info.Samples -ne $Track.Sectors * 588 -or ($md5Hex -and $info.Md5 -ne $md5Hex)) {
            Fail "$name doesn't hold exactly the CD's audio (its FLAC checksum differs)."
        }
    }
    return $digest
}

function Make-Shortcut {
    $exe = Join-Path $Root 'Big Red Racing.exe'
    if (-not (Test-Path -LiteralPath $exe)) { Say '  No desktop shortcut: Big Red Racing.exe isn''t in this folder.' 'Yellow'; return }
    try {
        $shell = New-Object -ComObject WScript.Shell
        $link = $shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) 'Big Red Racing.lnk'))
        $link.TargetPath = $exe
        $link.WorkingDirectory = $Root
        $link.IconLocation = "$exe,0"
        $link.Description = 'Big Red Racing, with Big Red Launcher'
        $link.Save()
        Say '  desktop shortcut: Big Red Racing' 'Green'
    } catch { Say "  Couldn't make the desktop shortcut ($($_.Exception.Message))." 'Yellow' }
}

# ---- the steps ----
function Main {
    Say "Install Game Files, for Big Red Launcher $SetupVersion by ctrl-alt-rees" 'Cyan'
    Say 'Makes the Game and Music folders from your own copy of the two Big Red Racing zips.'
    Say "Folder: $Root"
    if (-not (Test-Path -LiteralPath (Join-Path $Root 'Big Red Racing.exe'))) {
        Say 'Note: Big Red Racing.exe (Big Red Launcher) isn''t in this folder. This file belongs in the folder' 'Yellow'
        Say 'Big Red Launcher was unzipped into, so that Game and Music end up next to it.' 'Yellow'
    }
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem

    Heading 'The zips'
    $discZip = Find-Zip $DiscZipInfo.Pattern $DiscZipInfo.Name
    $patchZip = Find-Zip $PatchZipInfo.Pattern $PatchZipInfo.Name
    if (-not $discZip -or -not $patchZip) {
        Say 'These two files from myabandonware.com (search there for Big Red Racing) need to be in this folder:' 'Red'
        foreach ($pair in @(@($DiscZipInfo, $discZip), @($PatchZipInfo, $patchZip))) {
            $mark = if ($pair[1]) { 'found  ' } else { 'MISSING' }
            Say ('  {0}  {1}  ({2}, {3})' -f $mark, $pair[0].Name, $pair[0].What, (Size-Text $pair[0].Size)) 'Red'
        }
        Say 'Download them, put them next to this file (keep their names) and run it again.' 'Red'
        return 1
    }
    Say "  found $(Split-Path -Leaf $discZip) and $(Split-Path -Leaf $patchZip)"

    # What's there already, and every question now, so that the rest runs by itself
    Heading 'Questions'
    $files = Game-Files
    $todo = $files
    $gameNote = ''
    if (Test-Path -LiteralPath (Join-Path $GameDir 'WINRACE.EXE')) {
        Say '  There is a Game folder already: checking it against the game''s known files...'
        $todo = @(Check-Game $files)
        if ($todo.Count -eq 0) {
            Say "  All $($files.Count) of the game's files are there and correct." 'Green'
            $gameNote = "all $($files.Count) of the game's files already there and correct"
        } else {
            Say "  $($todo.Count) of the game's $($files.Count) files are missing or different:" 'Yellow'
            $todo | Select-Object -First 8 | ForEach-Object { Say "    $($_.Name)" 'Yellow' }
            if ($todo.Count -gt 8) { Say "    ... and $($todo.Count - 8) more" 'Yellow' }
            if (-not (Ask-YesNo '  Put the original files back from the zips? (Nothing else in the Game folder is touched.)' $true)) {
                $todo = @()
                $gameNote = 'left as it was, with the differences above'
            }
        }
    }

    $existing = @()
    if (Test-Path -LiteralPath $MusicDir) {
        $existing = @(Get-ChildItem -LiteralPath $MusicDir -File | Where-Object { $_.Name -match '^track\d\d\.(flac|ogg|wav)$' })
    }
    Say ''
    Say '  The game''s music is on the CD as audio tracks. Converted into the Music folder, they play'
    Say '  in the game through Big Red Launcher.'
    if ($existing.Count -ge $AudioTrackCount) {
        $music = Ask-YesNo "  The Music folder already has $($existing.Count) tracks. Convert the music again?" $false
    } else {
        $music = Ask-YesNo '  Convert the CD music? Without it there will be no music in the game.' $true
        if (-not $music) { Say '  No music, then: the game will be silent apart from its sound effects (run this again to add it).' 'Yellow' }
    }
    $format = $null
    $win64 = [Environment]::Is64BitOperatingSystem
    if ($music) {
        Say ''
        Say '  Music format:'
        Say "    1. FLAC: exactly the CD's sound, about 240 MB (downloads the official FLAC tool, $(Size-Text $FlacTool.ZipSize))"
        if ($win64) { Say "    2. Ogg Vorbis: close to the CD's sound, about 60 MB (downloads FFmpeg, $(Size-Text $FfmpegTool.ZipSize))" }
        else { Say '    2. Ogg Vorbis: not on 32-bit Windows (FFmpeg is 64-bit only)' }
        Say '    3. WAV: exactly the CD''s sound, about 520 MB, nothing to download'
        while ($true) {
            $choice = Ask-Number '  Which?' 3 1
            if ($choice -eq 2 -and -not $win64) { Say '  Ogg Vorbis needs 64-bit Windows: please choose 1 or 3.' 'Yellow'; continue }
            break
        }
        $format = @('flac', 'ogg', 'wav')[$choice - 1]
    }
    Say ''
    $shortcut = Ask-YesNo '  Make a desktop shortcut to Big Red Racing.exe?' $true

    $tool = $null
    if ($format -eq 'flac') { $tool = if ($win64) { $FlacTool } else { $FlacTool32 } }
    if ($format -eq 'ogg') { $tool = $FfmpegTool }

    # Room on the disk
    $need = 0L
    foreach ($file in $todo) { $need += $file.Size }
    if ($format -eq 'flac') { $need += 255 * $MB }
    if ($format -eq 'ogg') { $need += 300 * $MB }
    if ($format -eq 'wav') { $need += 520 * $MB }
    $free = -1L
    try { $free = (New-Object IO.DriveInfo ([IO.Path]::GetPathRoot($Root))).AvailableFreeSpace } catch { $free = -1L }
    if ($free -ge 0 -and $free -lt $need + 20 * $MB) {
        Fail "This needs about $(Size-Text $need) of free disk space here, and there's $(Size-Text $free)."
    }

    Heading 'Checking the zips'
    $warnings = @()
    foreach ($pair in @(@($discZip, $DiscZipInfo), @($patchZip, $PatchZipInfo))) {
        Write-Host -NoNewline "  $(Split-Path -Leaf $pair[0]): "
        if ((File-Sha256 $pair[0]) -eq $pair[1].Sha256) { Say 'the known copy (SHA-256 matches)' 'Green' }
        else {
            Say 'not byte-identical to the known copy; its files are checked one by one below' 'Yellow'
            $warnings += "$(Split-Path -Leaf $pair[0]) isn't the known copy"
        }
    }

    if ($tool) {
        Heading "The music converter: $($tool.Label)"
        Get-Tool $tool
    }

    # The game's files and the music, in one pass through the disc image (its data track
    # first, then the audio tracks in order), straight out of the zip
    Heading 'The game and the music'
    if ($todo.Count -eq 0 -and -not $music) { Say '  nothing to do' }
    $installed = 0
    $bad = @()
    $tracksDone = 0
    $fromCd = @($todo | Where-Object { $_.Origin -like 'cd:*' })
    if ($todo.Count -gt 0) {
        foreach ($folder in $GameFolders) {
            $path = Local-Path $GameDir $folder
            if (-not (Test-Path -LiteralPath $path)) { New-Item -ItemType Directory -Path $path -Force | Out-Null }
        }
    }
    $zip = $null
    $stream = $null
    try {
        $tracks = $null
        if ($fromCd.Count -gt 0 -or $music) {
            $zip = Open-Zip $discZip
            $bin = @($zip.Entries | Where-Object { $_.Name -match '\.bin$' })
            $cue = @($zip.Entries | Where-Object { $_.Name -match '\.cue$' })
            if ($bin.Count -ne 1 -or $cue.Count -ne 1) { Fail "$(Split-Path -Leaf $discZip) should hold one .bin and one .cue file." }
            if ($bin[0].Length % $RawSector -ne 0) { Fail 'The disc image isn''t made of whole 2,352-byte CD sectors.' }
            $reader = New-Object IO.StreamReader ($cue[0].Open(), [Text.Encoding]::GetEncoding(28591))
            try { $cueText = $reader.ReadToEnd() } finally { $reader.Dispose() }
            $tracks = @(Read-Cue $cueText ([long]($bin[0].Length / $RawSector)))
            $known = ($tracks.Count -eq $AudioTrackCount + 1)
            foreach ($t in $tracks) {
                if ($t.Number -lt 2) { continue }
                $expected = $AudioTracks[$t.Number]
                if (-not $expected -or $expected.Start -ne $t.Start -or $expected.Sectors -ne $t.Sectors) { $known = $false }
            }
            if (-not $known) {
                Say '  The disc image''s track layout isn''t the known one.' 'Yellow'
                $warnings += 'the disc image''s track layout isn''t the known one'
            }
            $stream = $bin[0].Open()
            Say "  reading the CD's data track ($(Size-Text ($tracks[0].Sectors * $RawSector)))..."
            $data = New-Object byte[] ([int]($tracks[0].Sectors * $RawSector))
            Read-Exactly $stream $data $data.Length
            if ($fromCd.Count -gt 0) {
                $cd = Read-Iso $data
                foreach ($file in $fromCd) {
                    $entry = $cd[$file.Origin.Substring(3).ToUpperInvariant()]
                    if (-not $entry) { $bad += "$($file.Name) (not on the disc image)"; continue }
                    if (Write-GameFile $file (Iso-Bytes $data $entry) $entry.When) { $installed++ } else { $bad += $file.Name }
                    if ($installed % 50 -eq 0) { Write-Host -NoNewline "`r  game files from the CD: $installed" }
                }
                Say "`r  game files from the CD: $installed      "
            }
            $data = $null
        }

        $fromPatch = @($todo | Where-Object { $_.Origin -like 'patch:*' })
        if ($fromPatch.Count -gt 0) {
            $patch = Open-Zip $patchZip
            try {
                $count = 0
                foreach ($file in $fromPatch) {
                    $wanted = $file.Origin.Substring(6)
                    $entry = @($patch.Entries | Where-Object { $_.Name -ieq $wanted })
                    if ($entry.Count -ne 1) { $bad += "$($file.Name) (not in the patch zip)"; continue }
                    $bytes = New-Object byte[] ([int]$entry[0].Length)
                    $in = $entry[0].Open()
                    try { Read-Exactly $in $bytes $bytes.Length } finally { $in.Dispose() }
                    if (Write-GameFile $file $bytes $entry[0].LastWriteTime.DateTime) { $installed++; $count++ } else { $bad += $file.Name }
                }
                Say "  game files from the v2.25 patch: $count"
            } finally { $patch.Dispose() }
        }
        foreach ($file in @($todo | Where-Object { $_.Origin -eq 'installer' })) {
            if (Write-GameFile $file ([Convert]::FromBase64String($InstallerFiles[$file.Name])) $InstallerTime) { $installed++ }
            else { $bad += $file.Name }
        }
        if ($todo.Count -gt 0) {
            if ($bad.Count -eq 0) { Say "  Game folder: $installed file(s) written, every one checked (SHA-256)." 'Green' }
            else {
                Say "  $($bad.Count) file(s) don't match the known v2.25 game:" 'Yellow'
                $bad | Select-Object -First 8 | ForEach-Object { Say "    $_" 'Yellow' }
                $warnings += "$($bad.Count) game file(s) don't match the known v2.25 game"
            }
        }

        if ($music) {
            if (-not (Test-Path -LiteralPath $MusicDir)) { New-Item -ItemType Directory -Path $MusicDir | Out-Null }
            foreach ($old in @(Get-ChildItem -LiteralPath $MusicDir -File | Where-Object { $_.Name -match '^track\d\d\.(flac|ogg|wav)$' })) {
                Remove-Item -LiteralPath $old.FullName -Force
            }
            $buffer = New-Object byte[] ($RawSector * 450)
            $audio = @($tracks | Where-Object { $_.Number -ge 2 })
            foreach ($t in $audio) {
                Write-Host -NoNewline ("`r  music: track{0:D2}.{1} ({2} of {3})   " -f $t.Number, $format, ($tracksDone + 1), $audio.Count)
                $digest = Convert-Track $stream $t $format $tool $buffer
                $expected = $AudioTracks[$t.Number]
                if ($expected -and $digest -ne $expected.Sha256) { $warnings += "track $($t.Number) isn't the known CD audio" }
                $tracksDone++
            }
            Say ("`r  music: {0} tracks in the Music folder ({1}), each checked against the CD's audio.   " -f $tracksDone, $format.ToUpperInvariant()) 'Green'
        }
    } finally {
        if ($stream) { $stream.Dispose() }
        if ($zip) { $zip.Dispose() }
    }

    if ($shortcut) { Make-Shortcut }

    Heading 'Tidying up'
    if ($warnings.Count -eq 0 -and $bad.Count -eq 0) {
        $size = (Get-Item -LiteralPath $discZip).Length + (Get-Item -LiteralPath $patchZip).Length
        if (Ask-YesNo "  Delete the two zips to save $(Size-Text $size)? (You'd need them again to run this again.)" $false) {
            Remove-Item -LiteralPath $discZip, $patchZip -Force
            Say '  zips deleted'
        }
    } else { Say '  The zips are kept, as something didn''t match (see above).' }
    if (Test-Path -LiteralPath $ToolsDir) {
        $mine = @(@($FlacTool, $FlacTool32, $FfmpegTool) | ForEach-Object { $_.Folder; $_.Zip } | Select-Object -Unique)
        $toolSize = 0L
        foreach ($item in $mine) {
            $path = Join-Path $ToolsDir $item
            if (Test-Path -LiteralPath $path) { Get-ChildItem -LiteralPath $path -Recurse -File | ForEach-Object { $toolSize += $_.Length } }
        }
        if ($toolSize -gt 0 -and (Ask-YesNo "  Delete the music converter downloaded into the Tools folder ($(Size-Text $toolSize))? It's only needed to convert the music again." $true)) {
            foreach ($item in $mine) {
                $path = Join-Path $ToolsDir $item
                if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
            }
            Say '  converter deleted'
        }
        if (@(Get-ChildItem -LiteralPath $ToolsDir -Force).Count -eq 0) { Remove-Item -LiteralPath $ToolsDir -Force }
    }

    Heading 'Done'
    if ($gameNote) { Say "  Game folder: $gameNote." }
    elseif ($bad.Count -eq 0) { Say '  Game folder: Big Red Racing, patched to v2.25, checked file by file.' 'Green' }
    if ($music) { Say ("  Music folder: {0} tracks ({1})." -f $tracksDone, $format.ToUpperInvariant()) 'Green' }
    elseif ($existing.Count -ge $AudioTrackCount) { Say "  Music folder: $($existing.Count) tracks, as before." }
    else { Say '  No music: the game will have none.' 'Yellow' }
    foreach ($warning in $warnings) { Say "  Warning: $warning" 'Yellow' }
    Say ''
    Say '  Start Big Red Racing.exe to play.' 'Cyan'
    if ($warnings.Count -gt 0) { return 2 }
    return 0
}

try {
    $code = Main
    exit $code
} catch {
    Say ''
    Say "Stopped: $($_.Exception.Message)" 'Red'
    exit 1
}

# Where the batch part sends Windows 95, 98 and ME ("goto brr_end", after its message): to
# COMMAND.COM a label on the file's last line, so it stops reading here; to PowerShell a loop
# that never runs (and is never reached: the script exits above).
:brr_end while ($false) {}
