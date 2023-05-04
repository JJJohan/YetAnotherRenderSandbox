#include "GLTFLoader.hpp"
#include "Core/Logging/Logger.hpp"
#include "MeshManager.hpp"
#include "Shader.hpp"
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>

#define TINYGLTF_USE_CPP14
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

using namespace Engine::Logging;

namespace Engine::Rendering
{
	template <typename T>
	VertexData CopyBufferData(const tinygltf::Buffer& buffer, const tinygltf::BufferView& bufferView, const tinygltf::Accessor accessor)
	{
		std::vector<T> data;
		data.resize(accessor.count);
		memcpy(data.data(), static_cast<const uint8_t*>(buffer.data.data()) + bufferView.byteOffset + accessor.byteOffset, accessor.count * sizeof(std::decay_t<T>));
		return VertexData(data);
	}

	bool BindMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, MeshManager* meshManager, const Shader* shader, glm::mat4x4 transform, std::vector<uint32_t>& results)
	{
		int materialId = -1;
		for (size_t i = 0; i < mesh.primitives.size(); ++i)
		{
			tinygltf::Primitive primitive = mesh.primitives[i];

			if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
			{
				Logger::Warning("Unexpected primitive type.");
				continue;
			}

			if (primitive.material != -1)
			{
				if (materialId != -1)
				{
					Logger::Warning("Multiple materials per mesh are currently not supported.");
				}

				materialId = primitive.material;
			}

			std::vector<VertexData> vertexDataArrays;
			vertexDataArrays.resize(1);

			for (const std::pair<std::string, int32_t>& attrib : primitive.attributes)
			{
				tinygltf::Accessor accessor = model.accessors[attrib.second];
				const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

				int32_t vertexDataIndex = -1;
				if (attrib.first == "POSITION") vertexDataIndex = 0;
				//if (attrib.first == "NORMAL") vertexDataIndex = 1;
				if (attrib.first == "TEXCOORD_0")
				{
					vertexDataArrays.resize(2);
					vertexDataIndex = 1;
				}
				if (vertexDataIndex > -1)
				{
					if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
					{
						Logger::Warning("Unexpected component type.");
						continue;
					}

					if (accessor.type == TINYGLTF_TYPE_SCALAR)
					{
						vertexDataArrays[vertexDataIndex] = CopyBufferData<float>(buffer, bufferView, accessor);
					}
					else if (accessor.type == TINYGLTF_TYPE_VEC2)
					{
						vertexDataArrays[vertexDataIndex] = CopyBufferData<glm::vec2>(buffer, bufferView, accessor);
					}
					else if (accessor.type == TINYGLTF_TYPE_VEC3)
					{
						vertexDataArrays[vertexDataIndex] = CopyBufferData<glm::vec3>(buffer, bufferView, accessor);
					}
					else if (accessor.type == TINYGLTF_TYPE_VEC4)
					{
						vertexDataArrays[vertexDataIndex] = CopyBufferData<glm::vec4>(buffer, bufferView, accessor);
					}
					else
					{
						Logger::Warning("Unexpected accessor type.");
						continue;
					}
				}
				else
				{
					Logger::Warning("Vertex data type not handled: {}", attrib.first);
				}
			}

			tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];
			const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
			const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];
			int indexByteStride = indexAccessor.ByteStride(indexBufferView);

			std::vector<uint32_t> indices;
			indices.resize(indexAccessor.count);
			if (indexByteStride == 2) // UINT16_T
			{
				for (size_t i = 0; i < indexAccessor.count; ++i)
				{
					const uint16_t* index = reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(indexBuffer.data.data()) + indexBufferView.byteOffset + indexAccessor.byteOffset + i * indexByteStride);
					indices[i] = static_cast<uint32_t>(*index);
				}
			}
			else
			{
				memcpy(indices.data(), static_cast<const uint8_t*>(indexBuffer.data.data()) + indexBufferView.byteOffset + indexAccessor.byteOffset, indexAccessor.count * sizeof(uint32_t));
			}

			Colour colour;
			std::shared_ptr<Image> image = nullptr;
			if (materialId != -1 && model.textures.size() > 0)
			{
				const tinygltf::Material& material = model.materials[materialId];
				auto pbrMaterial = material.extensions.find("KHR_materials_pbrSpecularGlossiness");
				if (pbrMaterial != material.extensions.end())
				{
					const tinygltf::Value& diffuseTexture = pbrMaterial->second.Get("diffuseTexture");
					if (diffuseTexture != tinygltf::Value())
					{
						const tinygltf::Value& indexObject = diffuseTexture.Get("index");
						if (indexObject != tinygltf::Value())
						{
							int index = indexObject.GetNumberAsInt();
							if (index >= 0 && index < model.textures.size())
							{
								const tinygltf::Texture& tex = model.textures[index];
								if (tex.source > -1)
								{
									const tinygltf::Image& gltfImage = model.images[tex.source];
									image = std::make_shared<Image>(glm::uvec2(static_cast<uint32_t>(gltfImage.width), static_cast<uint32_t>(gltfImage.height)), static_cast<uint32_t>(gltfImage.component), gltfImage.image);
								}
							}
						}
						else
						{

							const tinygltf::Value& indexObject = diffuseTexture.Get("index");
						}
					}
					else
					{
						const tinygltf::Value& diffuseFactor = pbrMaterial->second.Get("diffuseFactor");
						if (diffuseFactor.IsArray() && diffuseFactor.ArrayLen() == 4)
						{
							float r = static_cast<float>(diffuseFactor.Get(0).GetNumberAsDouble());
							float g = static_cast<float>(diffuseFactor.Get(1).GetNumberAsDouble());
							float b = static_cast<float>(diffuseFactor.Get(2).GetNumberAsDouble());
							float a = static_cast<float>(diffuseFactor.Get(3).GetNumberAsDouble());
							colour = Colour(r, g, b, a);
						}
					}
				}
			}

			uint32_t id = meshManager->CreateMesh(shader, vertexDataArrays, indices, transform, colour, image);
			results.push_back(id);
		}

		return true;
	}

	// bind models
	bool BindModelNodes(const tinygltf::Model& model, const tinygltf::Node& node, MeshManager* meshManager, const Shader* shader, glm::mat4 transform, std::vector<uint32_t>& results)
	{
		if (node.matrix.size() == 16)
		{
			float floatValues[16];
			for (uint32_t i = 0; i < 16; ++i)
				floatValues[i] = static_cast<float>(node.matrix[i]);

			transform *= glm::make_mat4(floatValues);
		}
		else
		{
			if (node.translation.size() == 3 || node.rotation.size() == 4 || node.scale.size() == 3)
			{
				glm::vec3 translation(0.0f, 0.0f, 0.0f);
				glm::qua rotation(0.0f, 0.0f, 0.0f, 1.0f);
				glm::vec3 scale(1.0f, 1.0f, 1.0f);

				// Assume Trans x Rotate x Scale order
				if (node.translation.size() == 3)
				{
					translation.x = static_cast<float>(node.translation[0]);
					translation.y = static_cast<float>(node.translation[1]);
					translation.z = static_cast<float>(node.translation[2]);
				}

				if (node.rotation.size() == 4)
				{
					rotation.x = static_cast<float>(node.rotation[0]);
					rotation.y = static_cast<float>(node.rotation[1]);
					rotation.z = static_cast<float>(node.rotation[2]);
					rotation.w = static_cast<float>(node.rotation[4]);
				}

				if (node.scale.size() == 3)
				{
					scale.x = static_cast<float>(node.scale[0]);
					scale.y = static_cast<float>(node.scale[1]);
					scale.z = static_cast<float>(node.scale[2]);
				}

				glm::mat4 matrix = glm::mat4();

				matrix = glm::translate(matrix, translation);
				matrix = glm::scale(matrix, scale);
				matrix *= glm::mat4_cast(rotation);
				transform *= matrix;
			}
		}

		if (node.mesh >= 0 && node.mesh < model.meshes.size())
		{
			if (!BindMesh(model, model.meshes[node.mesh], meshManager, shader, transform, results))
			{
				return false;
			}
		}

		for (size_t i = 0; i < node.children.size(); ++i)
		{
			if (!BindModelNodes(model, model.nodes[node.children[i]], meshManager, shader, transform, results))
			{
				return false;
			}
		}

		return true;
	}

	bool BindModel(tinygltf::Model& model, MeshManager* meshManager, const Shader* shader, std::vector<uint32_t>& results)
	{
		glm::mat4 transform = glm::mat4(1.0f);

		const tinygltf::Scene& scene = model.scenes[model.defaultScene];
		for (size_t i = 0; i < scene.nodes.size(); ++i)
		{
			if (!BindModelNodes(model, model.nodes[scene.nodes[i]], meshManager, shader, transform, results))
			{
				return false;
			}
		}

		return true;
	}

	bool GLTFLoader::LoadGLTF(const std::string& filePath, MeshManager* meshManager, const Shader* shader, std::vector<uint32_t>& results)
	{
		if (!std::filesystem::exists(filePath))
		{
			Logger::Error("File at '{}' does not exist.", filePath);
			return false;
		}

		static auto parseStartTime = std::chrono::high_resolution_clock::now();

		std::string err, warn;
		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		bool parseResult = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);

		if (!warn.empty())
		{
			Logger::Warning("GLTF parser warning: {}", warn);
		}

		if (!err.empty())
		{
			Logger::Error("GLTF parser error: {}", err);
		}

		if (!parseResult)
		{
			Logger::Error("Failed to parse GLTF file '{}'.", filePath);
			return false;
		}

		static auto parseEndTime = std::chrono::high_resolution_clock::now();
		float parseDeltaTime = std::chrono::duration<float, std::chrono::seconds::period>(parseEndTime - parseStartTime).count();
		Logger::Verbose("GLTF file parsed in {}ms.", parseDeltaTime);

		static auto loadStartTime = std::chrono::high_resolution_clock::now();

		bool result = BindModel(model, meshManager, shader, results);

		static auto loadEndTime = std::chrono::high_resolution_clock::now();
		float loadDeltaTime = std::chrono::duration<float, std::chrono::seconds::period>(loadEndTime - loadStartTime).count();
		Logger::Verbose("GLTF file loaded in {}ms.", loadDeltaTime);

		return result;
	}
}