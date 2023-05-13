#include "TangentCalculator.hpp"
#include "VertexData.hpp"

namespace Engine::Rendering
{
	void TangentCalculator::CalculateTangents(const VertexData& positionBuffer, const VertexData& normalBuffer, const VertexData& uvBuffer,
		const std::vector<uint32_t>& indices, std::unique_ptr<VertexData>& tangentsResult, std::unique_ptr<VertexData>& bitangentsResult)
	{
		uint32_t vertexCount = positionBuffer.GetCount();
		std::vector<glm::vec3> tangents(vertexCount);
		std::vector<glm::vec3> bitangents(vertexCount);

		const glm::vec3* positions = static_cast<const glm::vec3*>(positionBuffer.GetData());
		const glm::vec3* normals = static_cast<const glm::vec3*>(normalBuffer.GetData());
		const glm::vec2* uvs = static_cast<const glm::vec2*>(uvBuffer.GetData());

		uint32_t vertexIndex = 0;
		size_t indexCount = indices.size();
		for (uint32_t i = 0; i < indexCount; i += 3)
		{
			uint32_t i1 = indices[i];
			uint32_t i2 = indices[i + 1];
			uint32_t i3 = indices[i + 2];

			const glm::vec3& v0 = positions[i1];
			const glm::vec3& v1 = positions[i2];
			const glm::vec3& v2 = positions[i3];

			const glm::vec2& uv0 = uvs[i1];
			const glm::vec2& uv1 = uvs[i2];
			const glm::vec2& uv2 = uvs[i3];

			glm::vec3 deltaPos1 = v1 - v0;
			glm::vec3 deltaPos2 = v2 - v0;

			glm::vec2 deltaUV1 = uv1 - uv0;
			glm::vec2 deltaUV2 = uv2 - uv0;

			float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
			glm::vec3 tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
			glm::vec3 bitangent = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r;

			tangents[i1] += tangent;
			tangents[i2] += tangent;
			tangents[i3] += tangent;

			// Same thing for binormals
			bitangents[i1] += bitangent;
			bitangents[i2] += bitangent;
			bitangents[i3] += bitangent;
		}

		for (uint32_t i = 0; i < vertexCount; i += 1)
		{
			const glm::vec3& n = normals[i];
			glm::vec3& t = tangents[i];
			glm::vec3& b = bitangents[i];

			// Gram-Schmidt orthogonalize
			t = glm::normalize(t - n * glm::dot(n, t));

			// Calculate handedness
			if (glm::dot(glm::cross(n, t), b) < 0.0f)
			{
				t = t * -1.0f;
			}
		}

		tangentsResult = std::make_unique<VertexData>(tangents);
		bitangentsResult = std::make_unique<VertexData>(bitangents);
	}
}