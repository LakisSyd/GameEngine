$ErrorActionPreference = 'Stop'

$docPath = (Resolve-Path "$PSScriptRoot\NPC_AI_NAVMESH_CODE_GUIDE_RU.docx").Path
$qaPath = Join-Path $PSScriptRoot '.npc_ai_docx_qa'
$pdfPath = Join-Path $qaPath 'NPC_AI_NAVMESH_CODE_GUIDE_RU.pdf'
$donePath = Join-Path $qaPath 'export.done'
$errorPath = Join-Path $qaPath 'export.error.txt'

New-Item -ItemType Directory -Force $qaPath | Out-Null
Remove-Item -LiteralPath $donePath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $errorPath -Force -ErrorAction SilentlyContinue

$word = $null
$document = $null
try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $document = $word.Documents.Open($docPath, $false, $true)
    $document.ExportAsFixedFormat($pdfPath, 17)
    Set-Content -LiteralPath $donePath -Value 'ok' -Encoding ASCII
}
catch {
    Set-Content -LiteralPath $errorPath -Value $_.Exception.ToString() -Encoding UTF8
}
finally {
    if ($document) {
        $document.Close($false)
        [System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($document) | Out-Null
    }
    if ($word) {
        $word.Quit()
        [System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($word) | Out-Null
    }
}
