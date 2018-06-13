#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

struct Camera
{
    glm::vec3 up = glm::vec3(0, 0, 1);
    glm::vec3 direction = glm::vec3(0, 1, 0);
    glm::vec3 position = glm::vec3(0, 0, 0);
    float fov = 60;

    void translate(float x, float y, float z);
    void translate(const glm::vec3& offset);
    void up_by(float units);
    void forward_by(float units);
    void right_by(float units);

    void yaw_by(float thetaRad);
    void pitch_by(float thetaRad);
    void roll_by(float thetaRad);
    void rotate(float pRad, float yRad, float rRad);

    void look_at(const glm::vec3& eye, const glm::vec3& target);
    void look_at(const glm::vec3& target);
    glm::mat4 view_matrix();
};

inline void Camera::
translate(float x, float y, float z)
{
    glm::vec3 temp = position;
    temp.x += x;
    temp.y += y;
    temp.z += z;

    position = temp;
}

inline void Camera::
translate(const glm::vec3& offset)
{
    position += offset;
}

inline void Camera::
up_by(float units)
{
    position += up * units;
}

inline void Camera::
forward_by(float units)
{
    position += direction * units;
}

inline void Camera::
right_by(float units)
{
    position += glm::normalize(glm::cross(direction, up)) * units;
}

// NOTE: yep, all of these are crap.

inline void Camera::
yaw_by(float thetaRad)
{
    const float a = thetaRad;
    const float c = glm::cos(a);
    const float s = glm::sin(a);

    glm::vec3 axis = up;
    glm::vec3 temp((1 - c) * axis);
    glm::mat3 rot(glm::uninitialize);

    rot[0][0] = c + temp[0] * axis[0];
    rot[0][1] =     temp[0] * axis[1] + s * axis[2];
    rot[0][2] =     temp[0] * axis[2] - s * axis[1];

    rot[1][0] =     temp[1] * axis[0] - s * axis[2];
    rot[1][1] = c + temp[1] * axis[1];
    rot[1][2] =     temp[1] * axis[2] + s * axis[0];

    rot[2][0] =     temp[2] * axis[0] + s * axis[1];
    rot[2][1] =     temp[2] * axis[1] - s * axis[0];
    rot[2][2] = c + temp[2] * axis[2];

    direction = rot * direction;
}

inline void Camera::
pitch_by(float thetaRad)
{
    // FPS-style clamping

    if (thetaRad < 0.0f) {
        const float dotDown = glm::dot(direction, glm::vec3(0, 0, -1));
        // .996 is ~5 degrees, so any rotation starting greater than .97 is likely to go over.
        if (dotDown >= .97) return;

        const float thetaLeft = glm::acos(dotDown);
        if (-thetaRad >= thetaLeft)
            thetaRad = -(thetaLeft - .087f); // ~5 degrees
    }
    else if (thetaRad > 0.0f) {
        const float dotUp = glm::dot(direction, glm::vec3(0, 0, 1));
        if (dotUp >= .97) return;

        const float thetaLeft = glm::acos(glm::dot(direction, glm::vec3(0, 0, 1)));
        if (thetaRad >= thetaLeft) {
            thetaRad = thetaLeft - .087f;
        }
    }

    const float a = thetaRad;
    const float c = glm::cos(a);
    const float s = glm::sin(a);

    glm::vec3 axis = glm::normalize(glm::cross(direction, up));
    glm::vec3 temp((1 - c) * axis);
    glm::mat3 rot(glm::uninitialize);

    rot[0][0] = c + temp[0] * axis[0];
    rot[0][1] =     temp[0] * axis[1] + s * axis[2];
    rot[0][2] =     temp[0] * axis[2] - s * axis[1];

    rot[1][0] =     temp[1] * axis[0] - s * axis[2];
    rot[1][1] = c + temp[1] * axis[1];
    rot[1][2] =     temp[1] * axis[2] + s * axis[0];

    rot[2][0] =     temp[2] * axis[0] + s * axis[1];
    rot[2][1] =     temp[2] * axis[1] - s * axis[0];
    rot[2][2] = c + temp[2] * axis[2];

    up        = rot * up;
    direction = rot * direction;
}

inline void Camera::
roll_by(float thetaRad)
{
    const float a = thetaRad;
    const float c = glm::cos(a);
    const float s = glm::sin(a);

    glm::vec3 axis = direction;
    glm::vec3 temp((1 - c) * axis);
    glm::mat3 rot(glm::uninitialize);

    rot[0][0] = c + temp[0] * axis[0];
    rot[0][1] =     temp[0] * axis[1] + s * axis[2];
    rot[0][2] =     temp[0] * axis[2] - s * axis[1];

    rot[1][0] =     temp[1] * axis[0] - s * axis[2];
    rot[1][1] = c + temp[1] * axis[1];
    rot[1][2] =     temp[1] * axis[2] + s * axis[0];

    rot[2][0] =     temp[2] * axis[0] + s * axis[1];
    rot[2][1] =     temp[2] * axis[1] - s * axis[0];
    rot[2][2] = c + temp[2] * axis[2];

    up = rot * up;
}

inline void Camera::
rotate(float pitchRad, float yawRad, float rollRad)
{
    pitch_by(pitchRad);
    yaw_by(yawRad);
    roll_by(rollRad);
}

inline void Camera::
look_at(const glm::vec3& eye, const glm::vec3& target)
{
    glm::vec3 newDirection = glm::normalize(target-eye);
    glm::quat xform = glm::rotation(direction, newDirection);

    up        = xform * up;
    direction = newDirection;
    position  = eye;
}

inline void Camera::
look_at(const glm::vec3& target)
{
    glm::vec3 newDirection = glm::normalize(target-position);
    glm::quat xform = glm::rotation(direction, newDirection);

    up        = xform * up;
    direction = newDirection;
}

inline glm::mat4 Camera::
view_matrix()
{
    up = glm::vec3(0, 0, 1);
    return glm::lookAt(position, position + direction, up);
}
