#include "player.h"
#include "vector3.h"

Player::Player(const Vector3f& position, float rotX, float rotY): m_position(position), m_rotX(rotX), m_rotY(rotY)
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

Vector3f Player::SimulateMove(bool front, bool back, bool left, bool right, bool m_keyJump, float elapsedTime)
{
	Vector3f Deplacement(0.0f, 0.0f, 0.0f);
	float yrotrad;

	if (front)
	{
		yrotrad = (m_rotY / 180 * 3.141592654f);
		Deplacement.x += float(sin(yrotrad)) * elapsedTime;
		Deplacement.z -= float(cos(yrotrad)) * elapsedTime;
	}

	if (back)
	{
		yrotrad = (m_rotY / 180 * 3.141592654f);
		Deplacement.x -= float(sin(yrotrad)) * elapsedTime;
		Deplacement.z += float(cos(yrotrad)) * elapsedTime;
	}

	if (left)
	{
		yrotrad = (m_rotY / 180 * 3.141592654f);
		Deplacement.x -= float(cos(yrotrad)) * elapsedTime;
		Deplacement.z -= float(sin(yrotrad)) * elapsedTime;
	}

	if (right)
	{
		yrotrad = (m_rotY / 180 * 3.141592654f);
		Deplacement.x += float(cos(yrotrad)) * elapsedTime;
		Deplacement.z += float(sin(yrotrad)) * elapsedTime;
	}

	if (m_keyJump)
					{
		if (!BlockUnder)
		{
			Deplacement.y -= 0.5f * elapsedTime;
		}
		else
		{
			if (BlockUpper)
			{
				Deplacement.y += 0.5f * elapsedTime;
			}
		}
	}
	else
	{
		if (!BlockUnder)
		{
			Deplacement.y -= 1.0f * elapsedTime;
		}
	}
	return Deplacement;

}

void Player::SetIsBlockDORU(bool blockunder, bool blockupper)
{
	BlockUnder = blockunder;
	BlockUpper = blockupper;
}

void Player::SetPosition(Vector3f positionJoueur)
{
	m_position = positionJoueur;
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


