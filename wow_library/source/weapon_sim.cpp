#include "../include/weapon_sim.hpp"

Weapon_sim::Weapon_sim(double swing_speed, double min_damage, double max_damage, Socket socket, Weapon_type skill_type,
                       Weapon_socket weapon_socket, std::vector<Hit_effect> hit_effects)
    : swing_speed{swing_speed}
    , internal_swing_timer{0.0}
    , min_damage(min_damage)
    , max_damage(max_damage)
    , average_damage{0.0}
    , socket{socket}
    , weapon_type{skill_type}
    , hit_effects(std::move(hit_effects))
{
    socket_name = (socket == Socket::main_hand) ? "main_hand" : "off_hand";
    if (weapon_socket == Weapon_socket::two_hand)
    {
        normalized_swing_speed = 3.3;
    }
    else
    {
        if (weapon_type == Weapon_type::dagger)
        {
            normalized_swing_speed = 1.7;
        }
        else
        {
            normalized_swing_speed = 2.4;
        }
    }
}

double Weapon_sim::random_swing(double attack_power)
{
    double damage = min_damage + (max_damage - min_damage) * static_cast<double>(rand()) / RAND_MAX +
                    attack_power * swing_speed / 14;
    return damage;
}

double Weapon_sim::random_normalized_swing(double attack_power)
{
    return min_damage + (max_damage - min_damage) * static_cast<double>(rand()) / RAND_MAX +
           attack_power * normalized_swing_speed / 14;
}