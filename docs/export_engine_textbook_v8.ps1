param(
    [string]$Source = (Join-Path $PSScriptRoot 'DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_8_0_EXPANDED_COURSE.docx'),
    [string]$Output = (Join-Path $PSScriptRoot 'DX_FRAMEWORK_ENGINE_TEXTBOOK_RU_8_0_EXPANDED_COURSE.pdf')
)

$ErrorActionPreference = 'Stop'
$sourcePath = (Resolve-Path -LiteralPath $Source).Path
$outputPath = [System.IO.Path]::GetFullPath($Output)
$word = $null
$document = $null
try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $document = $word.Documents.Open($sourcePath, $false, $false)
    if ($document.TablesOfContents.Count -gt 0) {
        $document.TablesOfContents.Item(1).Update()
    }
    $document.Fields.Update() | Out-Null
    $document.Save()
    $document.ExportAsFixedFormat($outputPath, 17)
    Write-Output "PDF: $outputPath"
    Write-Output "Pages: $($document.ComputeStatistics(2))"
}
finally {
    if ($null -ne $document) {
        $document.Close($false)
    }
    if ($null -ne $word) {
        $word.Quit()
        [System.Runtime.InteropServices.Marshal]::ReleaseComObject($word) | Out-Null
    }
}
