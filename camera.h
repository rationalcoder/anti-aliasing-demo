struct Camera
{
    v3 up = v3(0, 0, 1);
    v3 direction = v3(0, 1, 0);
    v3 position = v3(0, 0, 0);

    v3 fpsUp = v3(0, 0, 1);
    bool fps = true;
    float fov = 65;

    void translate(float x, float y, float z);
    void translate(const v3& offset);
    void up_by(float units);
    void forward_by(float units);
    void right_by(float units);

    // @Slow. All of these are currently crap, but it probably doesn't matter.
    void yaw_by(float thetaRad);
    void pitch_by(float thetaRad);
    void roll_by(float thetaRad);
    void rotate(float pRad, float yRad, float rRad);

    void look_at(const v3& eye, const v3& target);
    void look_at(const v3& target);
    mat4 view_matrix();
};

inline void Camera::
translate(float x, float y, float z)
{
    v3 temp = position;
    temp.x += x;
    temp.y += y;
    temp.z += z;

    position = temp;
}

inline void Camera::
translate(const v3& offset)
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

    v3 axis = up;
    v3 temp((1 - c) * axis);
    mat3 rot(glm::uninitialize);

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
        const float dotDown = glm::dot(direction, v3(0, 0, -1));
        // .996 is ~5 degrees, so any rotation starting greater than .97 is likely to go over.
        if (dotDown >= .97) return;

        const float thetaLeft = glm::acos(dotDown);
        if (-thetaRad >= thetaLeft)
            thetaRad = -(thetaLeft - .087f); // ~5 degrees
    }
    else if (thetaRad > 0.0f) {
        const float dotUp = glm::dot(direction, v3(0, 0, 1));
        if (dotUp >= .97) return;

        const float thetaLeft = glm::acos(glm::dot(direction, v3(0, 0, 1)));
        if (thetaRad >= thetaLeft) {
            thetaRad = thetaLeft - .087f;
        }
    }

    const float a = thetaRad;
    const float c = glm::cos(a);
    const float s = glm::sin(a);

    v3 axis = glm::normalize(glm::cross(direction, up));
    v3 temp((1 - c) * axis);
    mat3 rot(glm::uninitialize);

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

    v3 axis = direction;
    v3 temp((1 - c) * axis);
    mat3 rot(glm::uninitialize);

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
look_at(const v3& eye, const v3& target)
{
    v3 newDirection = glm::normalize(target-eye);
    quat xform = glm::rotation(direction, newDirection);

    up        = xform * up;
    direction = newDirection;
    position  = eye;
}

inline void Camera::
look_at(const v3& target)
{
    v3 newDirection = glm::normalize(target-position);
    quat xform = glm::rotation(direction, newDirection);

    up        = xform * up;
    direction = newDirection;
}

inline mat4 Camera::
view_matrix()
{
    if (fps) up = fpsUp;

    return glm::lookAt(position, position + direction, up);
}
