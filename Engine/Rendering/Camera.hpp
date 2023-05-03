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

		EXPORT void SetNearFar(const glm::vec2& nearFar);
		EXPORT const glm::vec2& GetNearFar() const;

		EXPORT void TranslateLocal(const glm::vec3& translation);
		EXPORT void RotateEuler(const glm::vec3& eulerAngles);
		EXPORT void Rotate(const glm::quat& rotation);
		EXPORT void RotateFPS(float pitch, float yaw);

		EXPORT void SetPosition(const glm::vec3& position);
		EXPORT const glm::vec3& GetPosition() const;

		EXPORT void ClampPitch(const glm::vec2& minMaxPitch);

		EXPORT void SetRotation(const glm::quat& rotation);
		EXPORT const glm::quat& GetRotation() const;

		EXPORT void SetRotationEuler(const glm::vec3& eulerAngles);
		EXPORT glm::vec3 GetRotationEuler() const;

		EXPORT void SetFOV(float fov);
		EXPORT float GetFOV() const;

		EXPORT void SetView(const glm::mat4& view);
		EXPORT const glm::mat4& GetView() const;

		EXPORT const glm::mat4& GetProjection() const;

		void Update(const glm::uvec2& dimensions);

	private:
		void UpdateProjection();
		void UpdateView();

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