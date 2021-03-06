#include "Item_optimizer.hpp"

bool operator<(const Item_optimizer::Sim_result_t& left, const Item_optimizer::Sim_result_t& right)
{
    return left.mean_dps < right.mean_dps;
}

double Item_optimizer::get_total_qp_equivalent(const Special_stats& special_stats, const Weapon& wep1,
                                               const Weapon& wep2, const std::vector<Use_effect>& use_effects)
{
    double attack_power = special_stats.attack_power;
    int main_hand_skill = get_skill_of_type(special_stats, wep1.type);
    int off_hand_skill = get_skill_of_type(special_stats, wep2.type);
    double main_hand_ap =
        get_ap_equivalent(special_stats, main_hand_skill, wep1.swing_speed, (wep1.max_damage + wep1.min_damage) / 2);
    double off_hand_ap =
        get_ap_equivalent(special_stats, off_hand_skill, wep2.swing_speed, (wep2.max_damage + wep2.min_damage) / 2);
    double use_effects_ap = 0;
    double best_use_effects_shared_ap = 0;
    for (const auto& effect : use_effects)
    {
        double use_effect_ap =
            effect.get_special_stat_equivalent(special_stats).attack_power * std::min(effect.duration / sim_time, 1.0);
        if (effect.name == "badge_of_the_swarmguard")
        {
            use_effect_ap = 300 * std::min(effect.duration / sim_time, 1.0);
        }
        if (effect.effect_socket == Use_effect::Effect_socket::unique)
        {
            use_effects_ap += use_effect_ap;
        }
        else
        {
            if (use_effect_ap > best_use_effects_shared_ap)
            {
                best_use_effects_shared_ap = use_effect_ap;
            }
        }
    }
    double hit_effects_ap = 0;
    for (const auto& effect : wep1.hit_effects)
    {
        if (effect.type == Hit_effect::Type::damage_magic_guaranteed || effect.type == Hit_effect::Type::damage_magic ||
            effect.type == Hit_effect::Type::damage_physical)
        {
            hit_effects_ap += effect.probability * effect.damage * ap_per_coh;
        }
        else if (effect.type == Hit_effect::Type::extra_hit)
        {
            hit_effects_ap += effect.probability * crit_w;
        }
        else if (effect.type == Hit_effect::Type::stat_boost)
        {
            // Estimate empyrean demolisher as 10 DPS increase (okay since its only the filtering step)
            hit_effects_ap += 140;
        }
    }
    for (const auto& effect : wep2.hit_effects)
    {
        if (effect.type == Hit_effect::Type::damage_magic_guaranteed || effect.type == Hit_effect::Type::damage_magic ||
            effect.type == Hit_effect::Type::damage_physical)
        {
            hit_effects_ap += effect.probability * effect.damage * ap_per_coh * 0.5;
        }
        else if (effect.type == Hit_effect::Type::extra_hit)
        {
            hit_effects_ap += effect.probability * crit_w * 0.5;
        }
        else if (effect.type == Hit_effect::Type::stat_boost)
        {
            // Estimate empyrean demolisher as 10 DPS increase (okay since its only the filtering step)
            hit_effects_ap += 140 * 0.5;
        }
    }
    return attack_power + (main_hand_ap + 0.625 * off_hand_ap) / 1.625 + use_effects_ap + best_use_effects_shared_ap +
           hit_effects_ap;
}

void Item_optimizer::find_best_use_effect(const Special_stats& special_stats, std::string& debug_message)
{
    std::vector<Use_effect> use_effects;
    for (const auto& armor : legs)
    {
        if (!armor.use_effects.empty())
        {
            use_effects.push_back(armor.use_effects[0]);
        }
    }
    for (const auto& armor : trinkets)
    {
        if (!armor.use_effects.empty())
        {
            use_effects.push_back(armor.use_effects[0]);
        }
    }

    double best_ap = 0;
    for (const auto& use_effect : use_effects)
    {
        if (use_effect.effect_socket == Use_effect::Effect_socket::shared)
        {
            double ap_equiv = use_effect.get_special_stat_equivalent(special_stats).attack_power *
                              std::min(use_effect.duration, sim_time);
            debug_message +=
                "Name: " + use_effect.name + ". Estimated as: " + string_with_precision(ap_equiv, 5) + "<br>";
            std::cout << "Name: " + use_effect.name + ". Estimated as: " + string_with_precision(ap_equiv, 5) + "\n";
            if (ap_equiv > best_ap)
            {
                best_use_effect_name = use_effect.name;
                best_ap = ap_equiv;
            }
        }
    }
    debug_message += "Best shared use-effect: " + best_use_effect_name + "<br>";
    std::cout << "Best shared use-effect: " + best_use_effect_name + "\n";
}

void Item_optimizer::compute_weapon_combinations()
{
    weapon_combinations.clear();
    for (const auto& main_wep : main_hands)
    {
        for (const auto& off_wep : off_hands)
        {
            // TODO unique tag needed here!!
            if (main_wep.name != off_wep.name)
            {
                bool new_combination = true;
                for (const auto& combination : weapon_combinations)
                {
                    if (combination[0].name == main_wep.name && combination[1].name == off_wep.name)
                    {
                        new_combination = false;
                    }
                }
                if (new_combination)
                {
                    weapon_combinations.emplace_back(std::vector<Weapon>{main_wep, off_wep});
                }
            }
        }
    }
}

std::vector<std::vector<Armor>> Item_optimizer::get_combinations(const std::vector<Armor>& armors)
{
    std::vector<std::vector<Armor>> combinations;
    for (size_t i_1 = 0; i_1 < armors.size() - 1; i_1++)
    {
        for (size_t i_2 = i_1 + 1; i_2 < armors.size(); i_2++)
        {
            combinations.emplace_back(std::vector<Armor>{armors[i_1], armors[i_2]});
        }
    }
    return combinations;
}

bool is_armor_valid(const std::string& name)
{
    return !(name.substr(0, 14) == "item_not_found");
}

void Item_optimizer::extract_armors(const std::vector<std::string>& armor_vec)
{
    for (const auto& armor_name : armor_vec)
    {
        auto armor = armory.find_armor(Socket::head, armor_name);
        if (is_armor_valid(armor.name))
        {
            helmets.emplace_back(armor);
            continue;
        }
        armor = armory.find_armor(Socket::neck, armor_name);
        if (is_armor_valid(armor.name))
        {
            necks.emplace_back(armor);
            continue;
        }
        armor = armory.find_armor(Socket::shoulder, armor_name);
        if (is_armor_valid(armor.name))
        {
            shoulders.emplace_back(armor);
            continue;
        }
        armor = armory.find_armor(Socket::back, armor_name);
        if (is_armor_valid(armor.name))
        {
            backs.emplace_back(armor);
            continue;
        }
        armor = armory.find_armor(Socket::chest, armor_name);
        if (is_armor_valid(armor.name))
        {
            chests.emplace_back(armor);
            continue;
        }
        armor = armory.find_armor(Socket::wrist, armor_name);
        if (is_armor_valid(armor.name))
        {
            wrists.emplace_back(armor);
            continue;
        }
        armor = armory.find_armor(Socket::hands, armor_name);
        if (is_armor_valid(armor.name))
        {
            hands.emplace_back(armor);
            continue;
        }
        armor = armory.find_armor(Socket::belt, armor_name);
        if (is_armor_valid(armor.name))
        {
            belts.emplace_back(armor);
            continue;
        }
        armor = armory.find_armor(Socket::legs, armor_name);
        if (is_armor_valid(armor.name))
        {
            legs.emplace_back(armor);
            continue;
        }
        armor = armory.find_armor(Socket::boots, armor_name);
        if (is_armor_valid(armor.name))
        {
            boots.emplace_back(armor);
            continue;
        }
        armor = armory.find_armor(Socket::ring, armor_name);
        if (is_armor_valid(armor.name))
        {
            rings.emplace_back(armor);
            continue;
        }
        armor = armory.find_armor(Socket::trinket, armor_name);
        if (is_armor_valid(armor.name))
        {
            trinkets.emplace_back(armor);
            continue;
        }
        armor = armory.find_armor(Socket::ranged, armor_name);
        if (is_armor_valid(armor.name))
        {
            ranged.emplace_back(armor);
            continue;
        }
    }
}

void Item_optimizer::extract_weapons(const std::vector<std::string>& weapon_vec)
{
    for (const auto& weapon_name : weapon_vec)
    {
        auto weapon = armory.find_weapon(weapon_name);
        if (is_armor_valid(weapon.name))
        {
            if (weapon.weapon_socket == Weapon_socket::one_hand)
            {
                main_hands.emplace_back(weapon);
                off_hands.emplace_back(weapon);
            }
            else if (weapon.weapon_socket == Weapon_socket::main_hand)
            {
                main_hands.emplace_back(weapon);
            }
            else if (weapon.weapon_socket == Weapon_socket::off_hand)
            {
                off_hands.emplace_back(weapon);
            }
        }
    }
}

void Item_optimizer::fill(std::vector<Armor>& vec, Socket socket, std::string name)
{
    if (socket == Socket::trinket || socket == Socket::ring)
    {
        while (vec.size() < 2)
        {
            vec.emplace_back(armory.find_armor(socket, name));
        }
    }
    else
    {
        if (vec.empty())
        {
            vec.emplace_back(armory.find_armor(socket, name));
        }
    }
}

struct Weapon_struct
{
    Weapon_struct() = default;

    Weapon_struct(size_t index, Special_stats special_stats, double average_damage, double swing_speed,
                  std::string name, Weapon_type type, Weapon_socket socket)
        : index(index)
        , special_stats(special_stats)
        , average_damage(average_damage)
        , swing_speed(swing_speed)
        , name(std::move(name))
        , type(type)
        , socket(socket)
    {
    }

    size_t index{};
    Special_stats special_stats;
    Special_stats set_special_stats;
    Special_stats hit_special_stats;
    double average_damage{};
    double swing_speed{};
    std::string name{};
    Weapon_type type{};
    Weapon_socket socket{};
    bool can_be_estimated{true};
    bool remove{false};
};

bool is_strictly_weaker_wep(const Weapon_struct& wep_struct1, const Weapon_struct& wep_struct2, Weapon_socket socket)
{
    auto special_stats1 = wep_struct1.special_stats + wep_struct1.set_special_stats + wep_struct1.hit_special_stats;
    auto special_stats2 = wep_struct2.special_stats;

    bool greater_eq = (special_stats2.hit >= special_stats1.hit) &&
                      (special_stats2.critical_strike >= special_stats1.critical_strike) &&
                      (special_stats2.attack_power >= special_stats1.attack_power) &&
                      (special_stats2.axe_skill >= special_stats1.axe_skill) &&
                      (special_stats2.sword_skill >= special_stats1.sword_skill) &&
                      (special_stats2.mace_skill >= special_stats1.mace_skill) &&
                      (special_stats2.dagger_skill >= special_stats1.dagger_skill);

    if (socket == Weapon_socket::main_hand)
    {
        greater_eq &= wep_struct2.swing_speed >= wep_struct1.swing_speed;
    }
    else
    {
        greater_eq &= wep_struct2.swing_speed <= wep_struct1.swing_speed;
    }
    greater_eq &=
        wep_struct2.average_damage / wep_struct2.swing_speed >= wep_struct1.average_damage / wep_struct1.swing_speed;

    bool greater = (special_stats2.hit > special_stats1.hit) ||
                   (special_stats2.critical_strike > special_stats1.critical_strike) ||
                   (special_stats2.attack_power > special_stats1.attack_power) ||
                   (special_stats2.axe_skill > special_stats1.axe_skill) ||
                   (special_stats2.sword_skill > special_stats1.sword_skill) ||
                   (special_stats2.mace_skill > special_stats1.mace_skill) ||
                   (special_stats2.dagger_skill > special_stats1.dagger_skill);

    if (socket == Weapon_socket::main_hand)
    {
        greater |= wep_struct2.swing_speed > wep_struct1.swing_speed;
    }
    else
    {
        greater |= wep_struct2.swing_speed < wep_struct1.swing_speed;
    }
    greater |=
        wep_struct2.average_damage / wep_struct2.swing_speed > wep_struct1.average_damage / wep_struct1.swing_speed;

    return greater_eq && greater;
}

double estimate_wep_high(const Weapon_struct& wep, bool add_skill)
{
    Special_stats special_stats = wep.special_stats + wep.set_special_stats + wep.hit_special_stats;
    int max_skill = std::max(special_stats.axe_skill, 0);
    max_skill = std::max(special_stats.sword_skill, max_skill);
    max_skill = std::max(special_stats.mace_skill, max_skill);
    max_skill = std::max(special_stats.dagger_skill, max_skill);
    if (add_skill)
    {
        max_skill += 5; // Assumed to come from another source
    }
    double ap_from_skill = max_skill <= 5 ? max_skill * skill_w : 5 * skill_w + (max_skill - 5) * skill_w_soft;

    double ap_from_damage = wep.average_damage / wep.swing_speed * 14;

    double high_estimation = ap_from_damage + special_stats.attack_power + special_stats.hit * hit_w +
                             special_stats.critical_strike * crit_w + ap_from_skill;
    return high_estimation;
}

double estimate_wep_low(const Weapon_struct& wep)
{
    Special_stats special_stats = wep.special_stats;
    int max_skill = std::max(special_stats.axe_skill, 0);
    max_skill = std::max(special_stats.sword_skill, max_skill);
    max_skill = std::max(special_stats.mace_skill, max_skill);
    max_skill = std::max(special_stats.dagger_skill, max_skill);

    double ap_from_skill = max_skill * skill_w_hard;

    double ap_from_damage = wep.average_damage / wep.swing_speed * 14;

    double low_estimation = ap_from_damage + special_stats.attack_power + special_stats.hit * hit_w_cap +
                            special_stats.critical_strike * crit_w_cap + ap_from_skill;
    return low_estimation;
}

bool estimated_wep_weaker(const Weapon_struct& wep1, const Weapon_struct& wep2, bool main_hand)
{
    double wep_1_overest = estimate_wep_high(wep1, wep1.type != wep2.type);
    // Penalize fast MH and slow OH. Set 1 second to equal 100 AP.
    wep_1_overest += main_hand ? 100 * (wep1.swing_speed - 2.3) : -100 * (wep1.swing_speed - 2.3);
    double wep_2_underest = estimate_wep_low(wep2);
    wep_2_underest += main_hand ? 100 * (wep2.swing_speed - 2.3) : -100 * (wep1.swing_speed - 2.3);
    return wep_1_overest < wep_2_underest;
}

std::vector<Weapon> Item_optimizer::remove_weaker_weapons(const Weapon_socket weapon_socket,
                                                          const std::vector<Weapon>& weapon_vec,
                                                          const Special_stats& special_stats,
                                                          std::string& debug_message)
{
    std::vector<Weapon_struct> weapon_struct_vec;
    for (size_t i = 0; i < weapon_vec.size(); ++i)
    {
        Special_stats wep_special_stats = weapon_vec[i].special_stats;
        wep_special_stats += weapon_vec[i].attributes.convert_to_special_stats(special_stats);
        Weapon_struct wep_struct{i,
                                 wep_special_stats,
                                 (weapon_vec[i].min_damage + weapon_vec[i].max_damage) / 2,
                                 weapon_vec[i].swing_speed,
                                 weapon_vec[i].name,
                                 weapon_vec[i].type,
                                 weapon_vec[i].weapon_socket};
        if (weapon_vec[i].set_name != Set::none)
        {
            double ap_equiv_max = 0;
            Special_stats best_special_stats;
            for (const auto& set_bonus : armory.set_bonuses)
            {
                if (weapon_vec[i].set_name == set_bonus.set)
                {
                    Special_stats set_special_stats = set_bonus.special_stats;
                    set_special_stats += set_bonus.attributes.convert_to_special_stats(special_stats);
                    double ap_equiv = estimate_special_stats_high(set_special_stats);
                    if (ap_equiv > ap_equiv_max)
                    {
                        ap_equiv_max = ap_equiv;
                        best_special_stats = set_special_stats;
                    }
                }
            }
            wep_struct.set_special_stats = best_special_stats;
        }
        if (!weapon_vec[i].hit_effects.empty())
        {
            for (const auto& effect : weapon_vec[i].hit_effects)
            {
                switch (effect.type)
                {
                case Hit_effect::Type::damage_magic_guaranteed:
                case Hit_effect::Type::damage_magic:
                case Hit_effect::Type::damage_physical:
                    wep_struct.hit_special_stats.attack_power += effect.probability * effect.damage * ap_per_coh;
                    break;
                case Hit_effect::Type::extra_hit:
                {
                    double factor = weapon_socket == Weapon_socket::main_hand ? 1.0 : 0.5;
                    wep_struct.hit_special_stats.critical_strike += effect.probability * factor;
                    break;
                }
                default:
                    wep_struct.can_be_estimated = false;
                }
            }
        }
        weapon_struct_vec.push_back(wep_struct);
    }

    std::string wep_socket = weapon_socket == Weapon_socket::main_hand ? "main-hands" : "off-hands";
    for (auto& wep1 : weapon_struct_vec)
    {
        if (wep1.can_be_estimated)
        {
            bool found_one_stronger = false;
            for (const auto& wep2 : weapon_struct_vec)
            {
                if (wep1.index != wep2.index)
                {
                    if (wep1.type == wep2.type)
                    {
                        if (is_strictly_weaker_wep(wep1, wep2, weapon_socket))
                        {
                            if (found_one_stronger)
                            {
                                wep1.remove = true;
                                debug_message += "<b>REMOVED: " + wep1.name + "</b> from" + wep_socket + " since " +
                                                 wep2.name + " is strictly better.<br>";
                                std::cout << "<b>REMOVED: " + wep1.name + "</b> from " + wep_socket + " since " +
                                                 wep2.name + " is strictly better.\n";
                                break;
                            }
                            found_one_stronger = true;
                            debug_message += "1/2: " + wep1.name + " from" + wep_socket + " since " + wep2.name +
                                             " is strictly better.<br>";
                            continue;
                        }
                        if (estimated_wep_weaker(wep1, wep2, weapon_socket == Weapon_socket::main_hand))
                        {
                            if (found_one_stronger)
                            {
                                wep1.remove = true;
                                debug_message +=
                                    "<b>REMOVED: " + wep1.name + "</b> from " + wep_socket + ". High-est as: " +
                                    string_with_precision(estimate_wep_high(wep1, wep1.type != wep2.type), 3) +
                                    "AP. Eliminated by: ";
                                debug_message += "<b>" + wep2.name + "</b> low-est as: " +
                                                 string_with_precision(estimate_wep_low(wep2), 3) + "AP.<br>";
                                break;
                            }
                            found_one_stronger = true;
                            debug_message += "1/2: " + wep1.name + " from" + wep_socket + " since " + wep2.name +
                                             " is strictly better.<br>";
                        }
                    }
                }
            }
        }
    }

    debug_message += "Weapons left:<br>";
    std::vector<Weapon> filtered_weapons;
    for (size_t i = 0; i < weapon_vec.size(); ++i)
    {
        if (!weapon_struct_vec[i].remove)
        {
            filtered_weapons.push_back(weapon_vec[i]);
            debug_message += "<b>" + weapon_vec[i].name + "</b><br>";
        }
    }

    return filtered_weapons;
}

struct Armor_struct
{
    Armor_struct() = default;

    Armor_struct(size_t index, Special_stats special_stats, std::string name)
        : index(index), special_stats(special_stats), name(std::move(name))
    {
    }

    size_t index{};
    Special_stats special_stats;
    Special_stats set_special_stats;
    Special_stats use_special_stats;
    Special_stats hit_special_stats;
    std::string name;
    bool can_be_estimated{true};
    bool remove{false};
};

std::vector<Armor> Item_optimizer::remove_weaker_items(const std::vector<Armor>& armors,
                                                       const Special_stats& special_stats, std::string& debug_message)
{
    std::vector<Armor_struct> armors_special_stats;
    for (size_t i = 0; i < armors.size(); ++i)
    {
        Special_stats armor_special_stats = armors[i].special_stats;
        armor_special_stats += armors[i].attributes.convert_to_special_stats(special_stats);
        Armor_struct armor_equiv{i, armor_special_stats, armors[i].name};
        if (armors[i].set_name != Set::none)
        {
            double ap_equiv_max = 0;
            Special_stats best_special_stats;
            for (const auto& set_bonus : armory.set_bonuses)
            {
                if (armors[i].set_name == set_bonus.set)
                {
                    Special_stats set_special_stats = set_bonus.special_stats;
                    set_special_stats += set_bonus.attributes.convert_to_special_stats(special_stats);
                    double ap_equiv = estimate_special_stats_high(set_special_stats);
                    if (ap_equiv > ap_equiv_max)
                    {
                        ap_equiv_max = ap_equiv;
                        best_special_stats = set_special_stats;
                    }
                }
            }
            armor_equiv.set_special_stats = best_special_stats;
        }
        if (!armors[i].use_effects.empty())
        {
            if (armors[i].use_effects[0].effect_socket == Use_effect::Effect_socket::shared)
            {
                if (armors[i].use_effects[0].name == best_use_effect_name)
                {
                    Special_stats use_sp = armors[i].use_effects[0].get_special_stat_equivalent(special_stats);
                    use_sp.attack_power *= armors[i].use_effects[0].duration / sim_time;
                    armor_equiv.use_special_stats = use_sp;
                }
            }
            else
            {
                armor_equiv.can_be_estimated = false;
            }
        }
        if (!armors[i].hit_effects.empty())
        {
            for (const auto& effect : armors[i].hit_effects)
            {
                switch (effect.type)
                {
                case Hit_effect::Type::damage_magic_guaranteed:
                case Hit_effect::Type::damage_magic:
                case Hit_effect::Type::damage_physical:
                    armor_equiv.hit_special_stats.attack_power += effect.probability * effect.damage * ap_per_coh;
                    break;
                case Hit_effect::Type::extra_hit:
                    //  the factor 0.5 since extra hit resets main hand attack
                    armor_equiv.hit_special_stats.critical_strike += effect.probability * 0.5;
                    break;
                default:
                    armor_equiv.can_be_estimated = false;
                }
            }
        }
        armors_special_stats.push_back(armor_equiv);
    }

    for (auto& armor1 : armors_special_stats)
    {
        bool found_one_stronger = true;
        if (armors[0].socket == Socket::ring || armors[0].socket == Socket::trinket)
        {
            found_one_stronger = false;
        }
        if (armor1.can_be_estimated)
        {
            for (const auto& armor2 : armors_special_stats)
            {
                if (armor1.index != armor2.index)
                {
                    if (is_strictly_weaker(armor1.special_stats + armor1.set_special_stats + armor1.use_special_stats +
                                               armor1.hit_special_stats,
                                           armor2.special_stats + armor2.hit_special_stats))
                    {
                        if (found_one_stronger)
                        {
                            armor1.remove = true;
                            debug_message += "<b>REMOVED: " + armor1.name + "</b> since <b>" + armor2.name +
                                             "</b> is strictly better.<br>";
                            break;
                        }
                        debug_message +=
                            "<b>1/2: " + armor1.name + "</b> since <b>" + armor2.name + "</b> is strictly better.<br>";
                        found_one_stronger = true;
                        continue;
                    }
                    if (estimated_weaker(armor1.special_stats + armor1.set_special_stats + armor1.use_special_stats +
                                             armor1.hit_special_stats,
                                         armor2.special_stats + armor2.use_special_stats + armor2.hit_special_stats))
                    {
                        if (found_one_stronger)
                        {
                            armor1.remove = true;
                            debug_message +=
                                "<b>REMOVED: " + armor1.name + "</b>. High-est as: " +
                                string_with_precision(estimate_special_stats_high(armor1.special_stats) +
                                                          estimate_special_stats_high(armor1.set_special_stats),
                                                      3) +
                                "AP. Eliminated by: ";
                            debug_message +=
                                "<b>" + armor2.name + "</b> low-est as: " +
                                string_with_precision(estimate_special_stats_low(armor2.special_stats), 3) + "AP.<br>";
                            break;
                        }
                        debug_message += "<b>1/2: " + armor1.name + "</b>." + armor2.name + " is estimated better.<br>";
                        found_one_stronger = true;
                    }
                }
            }
        }
    }

    debug_message += "Armors left:<br>";
    std::vector<Armor> filtered_armors;
    for (size_t i = 0; i < armors.size(); ++i)
    {
        if (!armors_special_stats[i].remove)
        {
            filtered_armors.push_back(armors[i]);
            debug_message += "<b>" + armors[i].name + "</b><br>";
        }
    }

    if (armors[0].socket == Socket::ring || armors[0].socket == Socket::trinket)
    {
        if (filtered_armors.size() == 1)
        {
            debug_message += "Removed to many ring/trinkets, need atleast 2! Adding back the second best one<br>";
            double best_ap = -1;
            size_t best_index = 0;
            for (size_t i = 0; i < armors.size(); ++i)
            {
                if (armors_special_stats[i].remove)
                {
                    double ap = estimate_special_stats_high(armors_special_stats[i].special_stats);
                    if (ap > best_ap)
                    {
                        best_ap = ap;
                        best_index = i;
                    }
                }
            }
            filtered_armors.push_back(armors[best_index]);
        }
    }
    return filtered_armors;
}

void Item_optimizer::filter_weaker_items(const Special_stats& special_stats, std::string& debug_message)
{
    debug_message += "<br>Filtering <b>Shared cooldown use-effects: </b><br>";
    find_best_use_effect(special_stats, debug_message);

    debug_message += "<br>Filtering <b> Helmets: </b><br>";
    helmets = remove_weaker_items(helmets, special_stats, debug_message);
    debug_message += "<br>Filtering <b> necks: </b><br>";
    necks = remove_weaker_items(necks, special_stats, debug_message);
    debug_message += "<br>Filtering <b> shoulders: </b><br>";
    shoulders = remove_weaker_items(shoulders, special_stats, debug_message);
    debug_message += "<br>Filtering <b> backs: </b><br>";
    backs = remove_weaker_items(backs, special_stats, debug_message);
    debug_message += "<br>Filtering <b> chests: </b><br>";
    chests = remove_weaker_items(chests, special_stats, debug_message);
    debug_message += "<br>Filtering <b> wrists: </b><br>";
    wrists = remove_weaker_items(wrists, special_stats, debug_message);
    debug_message += "<br>Filtering <b> hands: </b><br>";
    hands = remove_weaker_items(hands, special_stats, debug_message);
    debug_message += "<br>Filtering <b> belts: </b><br>";
    belts = remove_weaker_items(belts, special_stats, debug_message);
    debug_message += "<br>Filtering <b> legs: </b><br>";
    legs = remove_weaker_items(legs, special_stats, debug_message);
    debug_message += "<br>Filtering <b> boots: </b><br>";
    boots = remove_weaker_items(boots, special_stats, debug_message);
    debug_message += "<br>Filtering <b> ranged: </b><br>";
    ranged = remove_weaker_items(ranged, special_stats, debug_message);
    debug_message += "<br>Filtering <b> rings: </b><br>";
    rings = remove_weaker_items(rings, special_stats, debug_message);
    debug_message += "<br>Filtering <b> trinkets: </b><br>";
    trinkets = remove_weaker_items(trinkets, special_stats, debug_message);

    debug_message += "<br>Filtering <b> weapons: </b><br>";
    main_hands = remove_weaker_weapons(Weapon_socket::main_hand, main_hands, special_stats, debug_message);
    off_hands = remove_weaker_weapons(Weapon_socket::off_hand, off_hands, special_stats, debug_message);
}

void Item_optimizer::fill_empty_armor()
{
    fill(helmets, Socket::head, "helmet");
    fill(necks, Socket::neck, "neck");
    fill(shoulders, Socket::shoulder, "shoulder");
    fill(backs, Socket::back, "back");
    fill(chests, Socket::chest, "chest");
    fill(wrists, Socket::wrist, "wrists");
    fill(hands, Socket::hands, "hands");
    fill(belts, Socket::belt, "belt");
    fill(legs, Socket::legs, "legs");
    fill(boots, Socket::boots, "boots");
    fill(ranged, Socket::ranged, "ranged");
    fill(rings, Socket::ring, "ring");
    fill(trinkets, Socket::trinket, "trinket");
}

void Item_optimizer::fill_empty_weapons()
{
    if (main_hands.empty())
    {
        main_hands.emplace_back(armory.find_weapon("none"));
    }
    if (off_hands.empty())
    {
        off_hands.emplace_back(armory.find_weapon("none"));
    }
}

void Item_optimizer::item_setup(const std::vector<std::string>& armor_vec, const std::vector<std::string>& weapons_vec)
{
    extract_armors(armor_vec);
    fill_empty_armor();
    extract_weapons(weapons_vec);
    fill_empty_weapons();
}

void Item_optimizer::compute_combinations()
{
    compute_weapon_combinations();
    ring_combinations = get_combinations(rings);
    trinket_combinations = get_combinations(trinkets);

    combination_vector.clear();
    combination_vector.push_back(helmets.size());
    combination_vector.push_back(necks.size());
    combination_vector.push_back(shoulders.size());
    combination_vector.push_back(backs.size());
    combination_vector.push_back(chests.size());
    combination_vector.push_back(wrists.size());
    combination_vector.push_back(hands.size());
    combination_vector.push_back(belts.size());
    combination_vector.push_back(legs.size());
    combination_vector.push_back(boots.size());
    combination_vector.push_back(ranged.size());
    combination_vector.push_back(ring_combinations.size());
    combination_vector.push_back(trinket_combinations.size());
    combination_vector.push_back(weapon_combinations.size());

    cum_combination_vector.clear();
    total_combinations = combination_vector[0];
    cum_combination_vector.push_back(combination_vector[0]);
    for (size_t i = 1; i < combination_vector.size(); i++)
    {
        cum_combination_vector.push_back(cum_combination_vector.back() * combination_vector[i]);
        total_combinations *= combination_vector[i];
    }
}

std::vector<size_t> Item_optimizer::get_item_ids(size_t index)
{
    std::vector<size_t> item_ids;
    item_ids.reserve(14);
    item_ids.push_back(index % combination_vector[0]);
    for (size_t i = 1; i < 14; i++)
    {
        item_ids.push_back(index / cum_combination_vector[i - 1] % combination_vector[i]);
    }
    return item_ids;
}

Character Item_optimizer::generate_character(const std::vector<size_t>& item_ids)
{
    Character character{race, 60};
    character.equip_armor(helmets[item_ids[0]]);
    character.equip_armor(necks[item_ids[1]]);
    character.equip_armor(shoulders[item_ids[2]]);
    character.equip_armor(backs[item_ids[3]]);
    character.equip_armor(chests[item_ids[4]]);
    character.equip_armor(wrists[item_ids[5]]);
    character.equip_armor(hands[item_ids[6]]);
    character.equip_armor(belts[item_ids[7]]);
    character.equip_armor(legs[item_ids[8]]);
    character.equip_armor(boots[item_ids[9]]);
    character.equip_armor(ranged[item_ids[10]]);
    character.equip_armor(ring_combinations[item_ids[11]][0]);
    character.equip_armor(ring_combinations[item_ids[11]][1]);
    character.equip_armor(trinket_combinations[item_ids[12]][0]);
    character.equip_armor(trinket_combinations[item_ids[12]][1]);
    character.equip_weapon(weapon_combinations[item_ids[13]][0], weapon_combinations[item_ids[13]][1]);
    return character;
}

Character Item_optimizer::construct(size_t index)
{
    Character character = generate_character(get_item_ids(index));

    armory.add_enchants_to_character(character, ench_vec);

    armory.add_buffs_to_character(character, buffs_vec);

    armory.compute_total_stats(character);

    return character;
}