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
	void Move(bool front, bool back, bool left, bool right, float elapsedTime);
	void ApplyTransformation(Transformation& transformation) const;
	
	const Vector3f& GetPositon() const;
	float GetRotationX() const;
	float GetRotationY() const;

private:
	Vector3f m_position;
	float m_rotX;
	float m_rotY;

};


#endif 
