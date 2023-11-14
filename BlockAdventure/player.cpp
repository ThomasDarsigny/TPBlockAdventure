#include "player.h"
#include "vector3.h"

Player::Player(const Vector3f& position, float rotX, float rotY): m_position(position), m_rotX(rotX), m_rotY(rotY), m_jumpHeight(0.0f)
{
	
}

void Player::TurnLeftRight(float value)
{
	m_rotY += value;
}

void Player::TurnTopBottom(float value)
{
	m_rotX += value;
	if (m_rotX < -90.0f) m_rotX = -90.0f; //Pour pas faire de tours en arrière
	if (m_rotX > 90.0f) m_rotX = 90.0f;   //Pour pas faire de tours en avant
}

void Player::Move(bool front, bool back, bool left, bool right,bool jump , float elapsedTime)
{
	Vector3f Deplacement(0.0f, 0.0f, 0.0f);
	float yrotrad;

	if (front)
	{
		yrotrad = (m_rotY / 180 * 3.141592654f);
		m_position.x += Deplacement.x += float(sin(yrotrad)) * elapsedTime;
		m_position.z += Deplacement.z -= float(cos(yrotrad)) * elapsedTime;		
	}

	if (back)
	{		
		yrotrad = (m_rotY / 180 * 3.141592654f);
		m_position.x += Deplacement.x -= float(sin(yrotrad)) * elapsedTime;
		m_position.z += Deplacement.z += float(cos(yrotrad)) * elapsedTime;
	}

	if (left)
	{	
		yrotrad = (m_rotY / 180 * 3.141592654f);
		m_position.x += Deplacement.x -= float(cos(yrotrad)) * elapsedTime;
		m_position.z += Deplacement.z -= float(sin(yrotrad)) * elapsedTime;
	}

	if (right)
	{
		yrotrad = (m_rotY / 180 * 3.141592654f);
		m_position.x += Deplacement.x += float(cos(yrotrad)) * elapsedTime;
		m_position.z += Deplacement.z += float(sin(yrotrad)) * elapsedTime;
	}
}

void Player::ApplyTransformation(Transformation& transformation) const
{ 
	transformation.ApplyRotation(-m_rotX, 1.f, 0, 0);
	transformation.ApplyRotation(-m_rotY, 0, 1.f, 0);
	transformation.ApplyTranslation(-m_position);
}

const Vector3f& Player::GetPositon() const
{
	return m_position;
}

float Player::GetRotationX() const
{
	return m_rotX;
}

float Player::GetRotationY() const
{
	return m_rotY;
}


