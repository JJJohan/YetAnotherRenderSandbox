function Compile-Shader($shader)
{
	$baseName = $shader.BaseName
	$ext = $shader.Extension.substring(1)
	& "C:/Development/VulkanSDK/1.3.243.0/Bin/glslc.exe" $shader.Name -o "${baseName}_${ext}.spv"
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