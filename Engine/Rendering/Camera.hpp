#pragma once

#include "Core/Macros.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Engine::Rendering
{
	class Camera
	{
	public:
		EXPORT Camera();

		inline void SetNearFar(const glm::vec2& nearFar)
		{
			m_nearFar = nearFar;
			m_projDirty = true;
		}

		inline const glm::vec2& GetNearFar() const { return m_nearFar; }

		EXPORT void TranslateLocal(const glm::vec3& translation);
		EXPORT void RotateEuler(const glm::vec3& eulerAngles);
		EXPORT void Rotate(const glm::quat& rotation);
		EXPORT void RotateFPS(float pitch, float yaw);

		inline void SetPosition(const glm::vec3& position)
		{
			m_position = position;
			m_viewDirty = true;
		}

		inline const glm::vec3& GetPosition() const { return m_position; }

		inline void ClampPitch(const glm::vec2& minMaxPitch) { m_pitchClamp = minMaxPitch; }

		inline void SetRotation(const glm::quat& rotation)
		{
			m_rotation = rotation;
			m_viewDirty = true;
		}

		inline const glm::quat& GetRotation() const { return m_rotation; }

		inline void SetRotationEuler(const glm::vec3& eulerAngles)
		{
			m_rotation = glm::quat(eulerAngles);
			m_viewDirty = true;
		}

		inline glm::vec3 GetRotationEuler() const { return glm::eulerAngles(m_rotation); }

		inline void SetFOV(float fov)
		{
			m_fov = fov;
			m_projDirty = true;
		}

		inline float GetFOV() const { return m_fov; }

		inline void SetView(const glm::mat4& view) { m_view = view; }
		inline const glm::mat4& GetView() const { return m_view; }

		inline const glm::mat4& GetProjection() const { return m_proj; }
		inline const glm::mat4& GetViewProjection() const { return m_viewProj; }

		void Update(const glm::uvec2& dimensions);

	private:
		void UpdateProjection();
		void UpdateView();

		glm::mat4 m_viewProj;
		glm::mat4 m_view;
		glm::vec3 m_position;
		glm::quat m_rotation;

		glm::mat4 m_proj;
		glm::uvec2 m_dimensions;
		glm::vec2 m_nearFar;
		glm::vec2 m_pitchClamp;
		float m_fov;
		float m_pitch;
		float m_yaw;

		bool m_viewDirty;
		bool m_projDirty;
	};
}