#include "Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace Engine::Rendering
{
	Camera::Camera()
		: m_fov(glm::radians(75.0f))
		, m_nearFar(0.01f, 100.0f)
		, m_dimensions(100, 100)
		, m_position(0.0f, 0.0f, -5.0f)
		, m_rotation(1.0f, 0.0f, 0.0f, 0.0f)
		, m_pitchClamp(glm::radians(-90.0f), glm::radians(90.0f))
		, m_pitch(0.0f)
		, m_yaw(0.0f)
		, m_viewDirty(true)
		, m_projDirty(true)
	{
		UpdateView();
		UpdateProjection();
	}

	void Camera::UpdateProjection()
	{
		if (m_projDirty)
		{
			m_proj = glm::perspectiveFov(m_fov, static_cast<float>(m_dimensions.x), static_cast<float>(m_dimensions.y), m_nearFar.x, m_nearFar.y);
			m_proj[1][1] *= -1.0f;
			m_projDirty = false;
		}
	}

	void Camera::UpdateView()
	{
		if (m_viewDirty)
		{
			m_view = glm::toMat4(m_rotation);
			m_view = glm::translate(m_view, m_position);
			m_viewDirty = false;
		}
	}

	void Camera::ClampPitch(const glm::vec2& minMaxPitch)
	{
		m_pitchClamp = minMaxPitch;
	}

	void Camera::Update(const glm::uvec2& dimensions)
	{
		m_projDirty = m_projDirty || m_dimensions != dimensions;
		m_dimensions = dimensions;

		UpdateView();
		UpdateProjection();
	}

	void Camera::TranslateLocal(const glm::vec3& translation)
	{
		m_position += translation * m_rotation;
		m_viewDirty = true;
	}

	void Camera::RotateEuler(const glm::vec3& eulerAngles)
	{
		glm::quat rotation = glm::quat(eulerAngles);
		m_rotation *= rotation;
		m_viewDirty = true;
	}

	void Camera::Rotate(const glm::quat& rotation)
	{
		m_rotation = glm::normalize(rotation * m_rotation);
		m_viewDirty = true;
	}

	void Camera::RotateFPS(float pitch, float yaw)
	{
		m_pitch = glm::clamp(m_pitch + pitch, m_pitchClamp.x, m_pitchClamp.y);
		m_yaw = std::remainder(m_yaw + yaw, glm::two_pi<float>());

		glm::quat qPitch = glm::angleAxis(m_pitch, glm::vec3(1, 0, 0));
		glm::quat qYaw = glm::angleAxis(m_yaw, glm::vec3(0, 1, 0));

		m_rotation = glm::normalize(qPitch * qYaw);
		m_viewDirty = true;
	}

	void Camera::SetPosition(const glm::vec3& position)
	{
		m_position = position;
		m_viewDirty = true;
	}

	const glm::vec3& Camera::GetPosition() const
	{
		return m_position;
	}

	void Camera::SetRotation(const glm::quat& rotation)
	{
		m_rotation = rotation;
		m_viewDirty = true;
	}

	void Camera::SetRotationEuler(const glm::vec3& eulerAngles)
	{
		m_rotation = glm::quat(eulerAngles);
		m_viewDirty = true;
	}

	const glm::quat& Camera::GetRotation() const
	{
		return m_rotation;
	}

	glm::vec3 Camera::GetRotationEuler() const
	{
		return glm::eulerAngles(m_rotation);
	}

	void Camera::SetNearFar(const glm::vec2& nearFar)
	{
		m_nearFar = nearFar;
		m_projDirty = true;
	}

	const glm::vec2& Camera::GetNearFar() const
	{
		return m_nearFar;
	}

	void Camera::SetFOV(float fov)
	{
		m_fov = fov;
		m_projDirty = true;
	}

	float Camera::GetFOV() const
	{
		return m_fov;
	}

	void Camera::SetView(const glm::mat4& view)
	{
		m_view = view;
	}

	const glm::mat4& Camera::GetView() const
	{
		return m_view;
	}

	const glm::mat4& Camera::GetProjection() const
	{
		return m_proj;
	}
}