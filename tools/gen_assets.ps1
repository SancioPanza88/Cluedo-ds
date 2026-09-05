# Generatore asset per il porting Nintendo DS di Cluedo.
# Legge i JPG da ..\..\cluedo\assets, produce PNG 8bpp quantizzati + .grit,
# tiles/sprite come array C deterministici, icon.gif per il banner ROM.
# Eseguire da questa cartella:  powershell -ExecutionPolicy Bypass -File gen_assets.ps1
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$proj   = Split-Path $PSScriptRoot -Parent
$srcDir = 'C:\Users\david\Desktop\cluedo\assets'
$gfxDir = Join-Path $proj 'graphics'
$srcOut = Join-Path $proj 'source'
New-Item -ItemType Directory -Force -Path $gfxDir, $srcOut | Out-Null

# ---------- 1. Quantizzatore median-cut + dithering Floyd-Steinberg (C#) ----------
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
public static class MedCut {
    public static void Quantize(int[] pix, byte[] outIdx, int[] outPal) {
        int n = pix.Length;
        int[] R = new int[n], G = new int[n], B = new int[n];
        for (int i = 0; i < n; i++) { R[i] = (pix[i] >> 16) & 255; G[i] = (pix[i] >> 8) & 255; B[i] = pix[i] & 255; }
        var boxes = new List<List<int>>();
        var first = new List<int>(n);
        for (int i = 0; i < n; i++) first.Add(i);
        boxes.Add(first);
        while (boxes.Count < 256) {
            int bi = -1, best = -1;
            for (int b = 0; b < boxes.Count; b++) {
                var bx = boxes[b];
                if (bx.Count < 2) continue;
                int r0=255,r1=0,g0=255,g1=0,b0=255,b1=0;
                foreach (int i in bx) {
                    int r=R[i],g=G[i],bl=B[i];
                    if(r<r0)r0=r; if(r>r1)r1=r; if(g<g0)g0=g; if(g>g1)g1=g; if(bl<b0)b0=bl; if(bl>b1)b1=bl;
                }
                int scA = r1-r0;
                if (g1-g0 > scA) { scA = g1-g0; }
                if (b1-b0 > scA) { scA = b1-b0; }
                int s = scA * bx.Count;
                if (s > best) { best = s; bi = b; }
            }
            if (bi < 0) break;
            var bx2 = boxes[bi];
            int q0=255,q1=0,w0=255,w1=0,e0=255,e1=0;
            foreach (int i in bx2) {
                int r=R[i],g=G[i],bl=B[i];
                if(r<q0)q0=r; if(r>q1)q1=r; if(g<w0)w0=g; if(g>w1)w1=g; if(bl<e0)e0=bl; if(bl>e1)e1=bl;
            }
            int ch = 0; int sc = q1-q0;
            if (w1-w0 > sc) { sc = w1-w0; ch = 1; }
            if (e1-e0 > sc) { ch = 2; }
            if (ch == 0) bx2.Sort((a,b) => R[a].CompareTo(R[b]));
            else if (ch == 1) bx2.Sort((a,b) => G[a].CompareTo(G[b]));
            else bx2.Sort((a,b) => B[a].CompareTo(B[b]));
            int mid = bx2.Count / 2;
            var nb = bx2.GetRange(mid, bx2.Count - mid);
            bx2.RemoveRange(mid, bx2.Count - mid);
            boxes.Add(nb);
        }
        int np = boxes.Count;
        int[] pr = new int[256], pg = new int[256], pb = new int[256];
        for (int b = 0; b < np; b++) {
            long sr=0,sg=0,sb=0; var bx = boxes[b];
            foreach (int i in bx) { sr+=R[i]; sg+=G[i]; sb+=B[i]; }
            int c = Math.Max(1, bx.Count);
            pr[b]=(int)(sr/c); pg[b]=(int)(sg/c); pb[b]=(int)(sb/c);
        }
        for (int b = np; b < 256; b++) { pr[b]=0; pg[b]=0; pb[b]=0; }
        float[] fr = new float[n], fg = new float[n], fb = new float[n];
        for (int i = 0; i < n; i++) { fr[i]=R[i]; fg[i]=G[i]; fb[i]=B[i]; }
        // (w,h servono solo se dithering 2D; qui serpentine su indice lineare con larghezza dedotta)
        for (int i = 0; i < n; i++) {
            int bestI = 0, bestD = int.MaxValue;
            int or_=(int)fr[i], og=(int)fg[i], ob=(int)fb[i];
            for (int p = 0; p < np; p++) {
                int dr=or_-pr[p], dg=og-pg[p], db=ob-pb[p];
                int d = dr*dr+dg*dg+db*db;
                if (d < bestD) { bestD = d; bestI = p; }
            }
            outIdx[i] = (byte)bestI;
            float er = fr[i]-pr[bestI], eg = fg[i]-pg[bestI], eb = fb[i]-pb[bestI];
            // diffusione semplice in avanti (efficace su foto piccole)
            if (i+1 < n) { fr[i+1]+=er*0.5f; fg[i+1]+=eg*0.5f; fb[i+1]+=eb*0.5f; }
        }
        for (int p = 0; p < 256; p++) outPal[p] = (pr[p]<<16)|(pg[p]<<8)|pb[p];
    }
}
'@

function Convert-Photo([string]$inFile, [string]$outPng, [int]$tw, [int]$th, [string]$crop) {
    $src = [System.Drawing.Bitmap]::FromFile($inFile)
    try {
        if ($crop -eq 'square') { $cw = [Math]::Min($src.Width, $src.Height); $ch = $cw }
        else { # 4:3
            if ($src.Width * 3 -gt $src.Height * 4) { $ch = $src.Height; $cw = [int]($ch * 4 / 3) }
            else { $cw = $src.Width; $ch = [int]($cw * 3 / 4) }
        }
        $cx = [int](($src.Width - $cw) / 2); $cy = [int](($src.Height - $ch) / 2)
        $tmp = New-Object System.Drawing.Bitmap($tw, $th, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g = [System.Drawing.Graphics]::FromImage($tmp)
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.DrawImage($src, (New-Object System.Drawing.Rectangle(0,0,$tw,$th)),
            (New-Object System.Drawing.Rectangle($cx,$cy,$cw,$ch)),
            [System.Drawing.GraphicsUnit]::Pixel)
        $g.Dispose()
        $n = $tw * $th
        $rect = New-Object System.Drawing.Rectangle(0,0,$tw,$th)
        $bd = $tmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $bytes = New-Object byte[] ($n * 4)
        [System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0, $bytes, 0, $bytes.Length)
        $tmp.UnlockBits($bd); $tmp.Dispose()
        $pix = New-Object int[] $n
        for ($i = 0; $i -lt $n; $i++) {
            $pix[$i] = ([int]$bytes[$i*4+2] -shl 16) -bor ([int]$bytes[$i*4+1] -shl 8) -bor [int]$bytes[$i*4]
        }
        $idx = New-Object byte[] $n
        $pal = New-Object int[] 256
        [MedCut]::Quantize($pix, $idx, $pal)
        $dst = New-Object System.Drawing.Bitmap($tw, $th, [System.Drawing.Imaging.PixelFormat]::Format8bppIndexed)
        $pp = $dst.Palette
        for ($i = 0; $i -lt 256; $i++) {
            $pp.Entries[$i] = [System.Drawing.Color]::FromArgb(($pal[$i] -shr 16) -band 255, ($pal[$i] -shr 8) -band 255, $pal[$i] -band 255)
        }
        $dst.Palette = $pp
        $bd2 = $dst.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly,
            [System.Drawing.Imaging.PixelFormat]::Format8bppIndexed)
        [System.Runtime.InteropServices.Marshal]::Copy($idx, 0, $bd2.Scan0, $n)
        $dst.UnlockBits($bd2)
        $dst.Save($outPng, [System.Drawing.Imaging.ImageFormat]::Png)
        $dst.Dispose()
        # .grit gemello (obbligatorio per BlocksDS): 8bpp tiled + mappa.
        # Niente flag trasparenza: le foto sono opache e il layer e' dedicato.
        Set-Content -LiteralPath ($outPng -replace '\.png$','.grit') -Value '-gB8 -gt -m -mLs' -NoNewline
        Write-Host "OK $outPng"
    } finally { $src.Dispose() }
}

# ---------- 2. Ritratti 64x64 e stanze 128x96 ----------
$portraits = @('rosso','senape','bianca','verdi','pavone','prugna')
foreach ($p in $portraits) {
    Convert-Photo (Join-Path $srcDir "suspect-$p.jpg") (Join-Path $gfxDir "suspect_$p.png") 64 64 'square'
}
$rooms = @('cucina','ballo','serra','pranzo','biliardo','biblioteca','salotto','ingresso','studio')
foreach ($r in $rooms) {
    Convert-Photo (Join-Path $srcDir "room-$r.jpg") (Join-Path $gfxDir "room_$r.png") 128 96 '43'
}
# Titolo 256x192 dal logo (stesso quantizzatore)
Convert-Photo (Join-Path $srcDir 'logo.jpg') (Join-Path $gfxDir 'title_logo.png') 256 192 '43'

# ---------- 3. Tiles tabellone (array C deterministici, niente grit) ----------
function RGB15([int]$r,[int]$g,[int]$b) { return ($r -shr 3) -bor (($g -shr 3) -shl 5) -bor (($b -shr 3) -shl 10) }
$roomCols = @(@(122,74,43),@(138,74,99),@(46,107,70),@(110,42,51),@(31,92,77),@(93,68,38),@(51,80,122),@(74,74,85),@(90,58,114))
$pal = @(0) * 256
$pal[0]  = RGB15 0 0 0
$pal[1]  = RGB15 74 59 40    # corridoio
$pal[2]  = RGB15 46 36 25    # corridoio scuro
$pal[3]  = RGB15 212 175 55  # porta oro
$pal[4]  = RGB15 138 109 31  # porta scura
$pal[5]  = RGB15 243 214 124 # evidenziazione
$pal[6]  = RGB15 248 244 230 # bianco
for ($i = 0; $i -lt 9; $i++) {
    $c = $roomCols[$i]
    $pal[10 + 2*$i] = RGB15 $c[0] $c[1] $c[2]
    $pal[11 + 2*$i] = RGB15 ([int]($c[0]*0.55)) ([int]($c[1]*0.55)) ([int]($c[2]*0.55))
}
function New-Tile([scriptblock]$fn) {
    $t = @(0) * 64
    for ($y = 0; $y -lt 8; $y++) { for ($x = 0; $x -lt 8; $x++) { $t[$y*8+$x] = & $fn $x $y } }
    return $t
}
$tiles = @()
$tiles += ,(New-Tile { param($x,$y) if ((($x + $y) % 2) -eq 0) { 1 } else { 2 } })                 # 0 corridoio
$tiles += ,(New-Tile { param($x,$y) if ($x -eq 0 -or $y -eq 0 -or $x -eq 7 -or $y -eq 7) { 4 } else { 3 } }) # 1 porta
$tiles += ,(New-Tile { param($x,$y) if ($x -ge 2 -and $x -le 5 -and $y -ge 2 -and $y -le 5) { 5 } else { 0 } }) # 2 highlight
for ($i = 0; $i -lt 9; $i++) {
    $f = 10 + 2*$i; $e = 11 + 2*$i
    $tiles += ,(New-Tile { param($x,$y) if ((($x*7+$y*3+$i) % 11) -eq 0) { $e } else { $f } }.GetNewClosure())  # floor
    $tiles += ,(New-Tile { param($x,$y) if ($x -eq 0 -or $y -eq 0 -or $x -eq 7 -or $y -eq 7) { $e } else { $f } }.GetNewClosure()) # edge
}
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('// Generato da tools/gen_assets.ps1 - NON modificare a mano')
[void]$sb.AppendLine('#pragma once')
[void]$sb.AppendLine('static const unsigned short boardPal[256] = {')
for ($i = 0; $i -lt 256; $i += 8) {
    [void]$sb.AppendLine('    ' + (($pal[$i..($i+7)] | ForEach-Object { '{0}' -f $_ }) -join ', ') + ',')
}
[void]$sb.AppendLine('};')
[void]$sb.AppendLine('static const unsigned char boardTiles[] = {')
foreach ($t in $tiles) { [void]$sb.AppendLine('    ' + ($t -join ', ') + ',') }
[void]$sb.AppendLine('};')
[void]$sb.AppendLine('#define TILE_CORR 0')
[void]$sb.AppendLine('#define TILE_DOOR 1')
[void]$sb.AppendLine('#define TILE_HL 2')
[void]$sb.AppendLine('#define TILE_ROOM_FLOOR(r) (3 + (r) * 2)')
[void]$sb.AppendLine('#define TILE_ROOM_EDGE(r) (4 + (r) * 2)')
[void]$sb.AppendLine('#define BOARD_TILE_COUNT ' + $tiles.Count)
Set-Content -LiteralPath (Join-Path $srcOut 'tiles_data.h') -Value $sb.ToString() -Encoding ASCII
Write-Host 'OK tiles_data.h'

# ---------- 4. Sprite pedine + cursore (array C deterministici) ----------
$pcols = @(@(192,57,43),@(217,162,27),@(185,194,204),@(46,158,91),@(47,111,208),@(142,68,173))
$spal = @(0) * 256
$spal[0] = RGB15 0 0 0
$spal[1] = RGB15 248 244 230
for ($i = 0; $i -lt 6; $i++) {
    $c = $pcols[$i]
    $spal[2 + 2*$i] = RGB15 $c[0] $c[1] $c[2]
    $spal[3 + 2*$i] = RGB15 ([int]($c[0]*0.5)) ([int]($c[1]*0.5)) ([int]($c[2]*0.5))
}
$spal[14] = RGB15 212 175 55
$spal[15] = RGB15 12 12 12
function New-Sprite([int]$base, [bool]$cursor) {
    $s = @(0) * 256
    for ($y = 0; $y -lt 16; $y++) { for ($x = 0; $x -lt 16; $x++) {
        $dx = $x - 7.5; $dy = $y - 7.5; $d = [Math]::Sqrt($dx*$dx + $dy*$dy)
        if ($cursor) {
            $edge = ($x -lt 3 -or $x -gt 12) -and ($y -lt 3 -or $y -gt 12)
            if (($x -eq 0 -or $x -eq 15 -or $y -eq 0 -or $y -eq 15) -and ($x -lt 4 -or $x -gt 11 -or $y -lt 4 -or $y -gt 11)) { $s[$y*16+$x] = 1 }
            elseif ($edge -and ($x -eq 1 -or $x -eq 14 -or $y -eq 1 -or $y -eq 14)) { $s[$y*16+$x] = 14 }
        } else {
            if ($d -gt 7.4) { continue }
            elseif ($d -gt 6.4) { $s[$y*16+$x] = 15 }
            elseif ($x -le 6 -and $y -le 6 -and $d -lt 5.5) { $s[$y*16+$x] = 1 }
            elseif ($y -gt 9) { $s[$y*16+$x] = $base + 1 }
            else { $s[$y*16+$x] = $base }
        }
    } }
    return $s
}
$sb2 = New-Object System.Text.StringBuilder
[void]$sb2.AppendLine('// Generato da tools/gen_assets.ps1 - NON modificare a mano')
[void]$sb2.AppendLine('#pragma once')
[void]$sb2.AppendLine('static const unsigned short sprPal[256] = {')
for ($i = 0; $i -lt 256; $i += 8) {
    [void]$sb2.AppendLine('    ' + (($spal[$i..($i+7)] | ForEach-Object { '{0}' -f $_ }) -join ', ') + ',')
}
[void]$sb2.AppendLine('};')
[void]$sb2.AppendLine('static const unsigned char sprPawn[6][256] = {')
for ($i = 0; $i -lt 6; $i++) {
    $s = New-Sprite (2 + 2*$i) $false
    [void]$sb2.AppendLine('  {')
    for ($r = 0; $r -lt 16; $r++) { [void]$sb2.AppendLine('    ' + ($s[($r*16)..($r*16+15)] -join ', ') + ',') }
    [void]$sb2.AppendLine('  },')
}
[void]$sb2.AppendLine('};')
$c = New-Sprite 0 $true
[void]$sb2.AppendLine('static const unsigned char sprCursor[256] = {')
for ($r = 0; $r -lt 16; $r++) { [void]$sb2.AppendLine('    ' + ($c[($r*16)..($r*16+15)] -join ', ') + ',') }
[void]$sb2.AppendLine('};')
Set-Content -LiteralPath (Join-Path $srcOut 'sprites_data.h') -Value $sb2.ToString() -Encoding ASCII
Write-Host 'OK sprites_data.h'

# ---------- 5. icon.gif 32x32 per il banner ROM ----------
$ico = New-Object System.Drawing.Bitmap(32, 32)
$g2 = [System.Drawing.Graphics]::FromImage($ico)
$g2.Clear([System.Drawing.Color]::FromArgb(20, 40, 30))
$br = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(212,175,55))
$g2.FillEllipse($br, 4, 4, 24, 24)
$br2 = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(20, 40, 30))
$g2.FillEllipse($br2, 9, 9, 14, 14)
$g2.Dispose()
$ico.Save((Join-Path $proj 'icon.gif'), [System.Drawing.Imaging.ImageFormat]::Gif)
$ico.Dispose()
Write-Host 'OK icon.gif'
Write-Host 'FINE - controlla graphics/ e source/'
