param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Remaining)

$outDir = $null
$inputPath = $null
$format = $null

for ($i = 0; $i -lt $Remaining.Count; $i++) {
    if ($Remaining[$i] -eq '--outdir' -and $i + 1 -lt $Remaining.Count) {
        $outDir = $Remaining[$i + 1]
        $i++
        continue
    }
    if ($Remaining[$i] -eq '--convert-to' -and $i + 1 -lt $Remaining.Count) {
        $format = $Remaining[$i + 1]
        $i++
        continue
    }
    if (-not $Remaining[$i].StartsWith('-')) {
        $inputPath = $Remaining[$i]
    }
}

if (-not $outDir -or -not $inputPath -or -not $format) {
    Write-Error "Word-backed soffice shim: missing --outdir, --convert-to, or input path."
    exit 2
}

if (-not $format.StartsWith('pdf')) {
    Write-Error "Word-backed soffice shim only supports PDF conversion."
    exit 3
}

$inputPath = [System.IO.Path]::GetFullPath($inputPath)
$outDir = [System.IO.Path]::GetFullPath($outDir)
$outputPath = Join-Path $outDir ([System.IO.Path]::GetFileNameWithoutExtension($inputPath) + '.pdf')
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$word = $null
$document = $null
try {
    Write-Output "word-shim: create Word"
    $word = New-Object -ComObject Word.Application
    Write-Output "word-shim: configure Word"
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $word.Options.CheckSpellingAsYouType = $false
    $word.Options.CheckGrammarAsYouType = $false
    $word.Options.SaveNormalPrompt = $false
    Write-Output "word-shim: open $inputPath"
    $document = $word.Documents.Open($inputPath, $false, $true, $false)
    # The DOCX requests a field refresh on normal Word open.  During automated
    # QA we intentionally export the stored layout: updating the TOC through
    # COM can force repeated full-document pagination and stall headless Word.
    Write-Output "word-shim: export $outputPath"
    $document.ExportAsFixedFormat($outputPath, 17)
    Write-Output "word-shim: close"
    $document.Close($false)
    $document = $null
    $word.Quit()
    $word = $null
    Write-Output "convert $inputPath -> $outputPath using Microsoft Word"
    exit 0
}
catch {
    Write-Error $_
    if ($document) { try { $document.Close($false) } catch { } }
    if ($word) { try { $word.Quit() } catch { } }
    exit 1
}
finally {
    if ($document) { [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($document) }
    if ($word) { [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($word) }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}
