#ifndef PLAYER_H__
#define PLAYER_H__
#include "vector3.h"
#include "transformation.h"

class Player 
{
public:
	Player(const Vector3f& position, float rotX = 0, float rotY = 0);
	void TurnLeftRight(float value);
	void TurnTopBottom(float value);
	Vector3f SimulateMove(bool m_keyW, bool m_keyS, bool m_keyA, bool m_keyD, bool m_keyJump, float elapsedTime);
	void CheckBlockUnderROver(bool blockunder, bool blockabove);
	void SetPosition(Vector3f positionJoueur);
	void ApplyTransformation(Transformation& transformation) const;
	
	const Vector3f& GetPositon() const;
	float GetRotationX() const;
	float GetRotationY() const;

private:
	Vector3f m_position;
	float m_rotX;
	float m_rotY;

	bool BlockUnder = false;
	bool BlockAbove = false;
	bool IsJumping = false;
	float positiondebutY = 0;

};


#endif 
