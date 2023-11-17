#include "player.h"
#include "vector3.h"

Player::Player(const Vector3f& position, float rotX, float rotY) : m_position(position), m_rotX(rotX), m_rotY(rotY)
{

}

void Player::TurnLeftRight(float value)
{
	m_rotY += value;
	if (m_rotY > 360.0f) m_rotY -= 360.0f;
	if (m_rotY < 0.0f) m_rotY += 360.0f;
}

void Player::TurnTopBottom(float value)
{
	m_rotX += value;
	if (m_rotX < -90.0f) m_rotX = -90.0f; //Pour ne pas faire de tours en arrière
	if (m_rotX > 90.0f) m_rotX = 90.0f;   //Pour ne pas faire de tours en avant
}

Vector3f Player::SimulateMove(bool front, bool back, bool left, bool right, bool m_keyJump, bool m_keyFly, float elapsedTime)
{
	if (elapsedTime > 0.5f)
	{
		(elapsedTime < 0.5f);
	}
	float yrotrad = (m_rotY / 180 * 3.141592654f);
	Vector3f movement(0.0f, 0.0f, 0.0f);

	if (front)  movement += Vector3f(sin(yrotrad), 0.0f, -cos(yrotrad)) * elapsedTime;
	if (back)   movement += Vector3f(-sin(yrotrad), 0.0f, cos(yrotrad)) * elapsedTime;
	if (left)   movement += Vector3f(-cos(yrotrad), 0.0f, -sin(yrotrad)) * elapsedTime;
	if (right)  movement += Vector3f(cos(yrotrad), 0.0f, sin(yrotrad)) * elapsedTime;
	
	if (m_keyFly) {
		movement.y +=1* elapsedTime;
	}
	else
		{		

		if (BlockUnder && m_keyJump) {
			IsJumping = true;
			positiondebutY = m_position.y;
		}

		if (IsJumping) {
			movement.y += elapsedTime;

			if (m_position.y > positiondebutY + 1.3f || BlockAbove) {
				IsJumping = false;
				movement.y = 0;
			}
		}
		else {
			movement.y -= elapsedTime;
		}
	}
	return movement;
}

void Player::CheckBlockUnderROver(bool blockunder, bool blockabove)
{
	BlockUnder = blockunder;
	BlockAbove = blockabove;
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

void Player::SetRotationY(float rotY)
{
	m_rotY = rotY;
}


