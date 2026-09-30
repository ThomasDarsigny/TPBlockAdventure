#ifndef PLAYER_H__
#define PLAYER_H__

#include "vector3.h"
#include "transformation.h"

class World;

// ---------------------------------------------------------------------------
//  Joueur: boite de collision alignee sur les axes, physique a la Minecraft
//  (gravite, saut, sprint, accroupissement, nage, vol en creatif), vie,
//  faim et degats de chute.
// ---------------------------------------------------------------------------
class Player
{
public:
    static const float WIDTH;      // 0.6
    static const float HEIGHT;     // 1.8
    static const float EYE;        // 1.62

    Player();

    void Reset(const Vector3f& feetPosition);

    // --- camera ---
    void TurnLeftRight(float value);
    void TurnTopBottom(float value);
    void ApplyTransformation(Transformation& transformation) const;

    Vector3f Direction() const;
    Vector3f EyePosition() const;

    const Vector3f& GetPosition() const { return m_position; }
    void SetPosition(const Vector3f& p) { m_position = p; }

    float GetRotationX() const { return m_rotX; }
    float GetRotationY() const { return m_rotY; }
    void  SetRotation(float rx, float ry) { m_rotX = rx; m_rotY = ry; }

    // --- entrees ---
    void SetMoveInput(bool forward, bool back, bool left, bool right);
    void SetJump(bool v)   { m_jump = v; }
    void SetSneak(bool v)  { m_sneak = v; }
    void SetSprint(bool v) { m_sprint = v; }

    // --- simulation ---
    void Update(World& world, float elapsed);

    // --- etats ---
    bool OnGround() const { return m_onGround; }
    bool InWater() const  { return m_inWater; }
    bool IsFlying() const { return m_flying; }
    bool IsCreative() const { return m_creative; }
    bool IsSneaking() const { return m_sneak; }
    bool IsSprinting() const { return m_sprint && m_forward && !m_sneak; }

    void ToggleFly();
    void SetCreative(bool v);
    void ToggleCreative() { SetCreative(!m_creative); }

    float Health() const { return m_health; }
    float MaxHealth() const { return 20.0f; }
    float Hunger() const { return m_hunger; }
    float Oxygen() const { return m_oxygen; }
    float MaxOxygen() const { return 10.0f; }

    // Remonte le joueur s'il se retrouve coince dans la matiere (chargement
    // d'une sauvegarde, terrain modifie...).
    void  Unstuck(const World& world);

    void  ApplyDamage(float amount);
    void  Heal(float amount);
    void  Feed(float amount) { m_hunger = (m_hunger + amount > 20.0f) ? 20.0f : m_hunger + amount; }
    bool  IsDead() const { return m_health <= 0.0f; }

    float DamageFlash() const { return m_damageFlash; }
    float EyeHeight() const;

    const Vector3f& Velocity() const { return m_velocity; }

private:
    bool  Collides(const World& world, const Vector3f& p) const;
    float MoveAxis(World& world, int axis, float delta);
    void  UpdateEnvironment(const World& world);

private:
    Vector3f m_position;      // pieds, centre de la boite
    Vector3f m_velocity;

    float m_rotX, m_rotY;

    bool m_forward, m_back, m_left, m_right;
    bool m_jump, m_sneak, m_sprint;

    bool m_onGround;
    bool m_inWater;
    bool m_headInWater;
    bool m_inLava;
    bool m_flying;
    bool m_creative;

    float m_health;
    float m_hunger;
    float m_oxygen;
    float m_damageFlash;
    float m_damageCooldown;
    float m_fallStartY;
    bool  m_falling;
    float m_lavaBurn;
};

#endif // PLAYER_H__
