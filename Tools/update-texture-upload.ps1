$p=Join-Path (Get-Location) 'Source/racers/GolDP/src/render/nativerenderer.cpp'
$s=[IO.File]::ReadAllText($p).Replace('get(2)','get(3)')
$start=$s.IndexOf('    uint64_t id = 0;',$s.IndexOf('HRESULT GolD3DRenderDevice::DrawNative'))
$end=$s.IndexOf('    if (!m_nativeApi->draw',$start)
$block=$s.Substring($start,$end-$start).Replace('m_currentTexture','sourceTexture')
$s=$s.Substring(0,$start)+'    uint64_t id = UploadNativeTexture(m_currentTexture);'+"`n"+$s.Substring($end)
$s+="`n"+'unsigned __int64 GolD3DRenderDevice::UploadNativeTexture(GolTexture* sourceTexture)'+"`n{`n"+$block+"    return id;`n}`n"
[IO.File]::WriteAllText($p,$s)
$p=Join-Path (Get-Location) 'Source/Renderer/renderer_test.cpp'
$s=[IO.File]::ReadAllText($p).Replace('RBGetApi(2)','RBGetApi(3)');[IO.File]::WriteAllText($p,$s)
