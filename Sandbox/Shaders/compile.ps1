function Compile-Shader($shader)
{
	$baseName = $shader.BaseName
	$ext = $shader.Extension.substring(1)
	#& "$env:VULKAN_SDK/Bin/glslc.exe" $shader.Name --target-env=vulkan1.3 -O -o "${baseName}_${ext}.spv"
	& "$env:VULKAN_SDK/Bin/glslc.exe" $shader.Name --target-env=vulkan1.3 -g -o "${baseName}_${ext}.spv" #debug info
}

$vertexShaders = dir *.vert | Select Name,BaseName,Extension
foreach ($shader in $vertexShaders)
{
	Compile-Shader $shader
}

$fragmentShaders = dir *.frag | Select Name,BaseName,Extension
foreach ($shader in $fragmentShaders)
{
	Compile-Shader $shader
}

Pause