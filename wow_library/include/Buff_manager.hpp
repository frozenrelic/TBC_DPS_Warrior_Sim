#ifndef WOW_SIMULATOR_BUFF_MANAGER_HPP
#define WOW_SIMULATOR_BUFF_MANAGER_HPP

#include "Attributes.hpp"
#include "Item.hpp"

#include <cmath>
#include <map>
#include <unordered_map>
#include <vector>

struct Over_time_buff
{
    Over_time_buff(std::string id, Special_stats special_stats, int init_time, double rage_gain, double damage,
                   int interval, int duration)
        : id(std::move(id))
        , special_stats(special_stats)
        , init_time(init_time)
        , total_ticks(duration / interval)
        , current_ticks(1)
        , rage_gain(rage_gain)
        , damage(damage)
        , interval(interval){};

    std::string id;
    Special_stats special_stats;
    int init_time;
    int total_ticks;
    int current_ticks;
    double rage_gain;
    double damage;
    int interval;
};

struct Combat_buff
{
    Combat_buff() = default;
    ~Combat_buff() = default;

    Combat_buff(Special_stats special_stats, double duration_left)
        : special_stats(special_stats), duration_left(duration_left){};

    Special_stats special_stats;
    double duration_left;
};

struct Hit_buff
{
    Hit_buff(std::string id, double duration_left) : id(std::move(id)), duration_left(duration_left){};

    std::string id;
    double duration_left;
};

class Buff_manager
{
public:
    void initialize(Special_stats& special_stats, const std::vector<Use_effect>& use_effects_input,
                    std::vector<Hit_effect>& hit_effects_mh_input, std::vector<Hit_effect>& hit_effects_oh_input,
                    bool performance_mode_in)
    {
        performance_mode = performance_mode_in;
        stat_gains.clear();
        hit_gains.clear();
        over_time_buffs.clear();
        simulation_special_stats = &special_stats;
        hit_effects_mh = &hit_effects_mh_input;
        hit_effects_oh = &hit_effects_oh_input;
        use_effects = use_effects_input;
        deep_wounds_damage = 0.0;
        rage_spent_executing = 0.0;
        deep_wounds_timestamps.clear();
        need_to_recompute_hittables = true;
    };

    double get_dt(double sim_time)
    {
        double dt = 1e10;
        dt = std::min(dt, next_event);
        double interval_dt = double(min_interval) - std::fmod(sim_time, double(min_interval));
        dt = std::min(dt, interval_dt);
        return dt;
    }

    std::vector<std::string> increment(double dt, double current_time, double time_left, double& rage,
                                       double& rage_lost_stance, double& rage_lost_exec, double& global_cooldown,
                                       bool debug)
    {
        size_t i = 0;
        next_event = 10;
        std::vector<std::string> debug_msg{};
        for (auto it = stat_gains.begin(); it != stat_gains.end();)
        {
            if (!performance_mode)
            {
                aura_uptime[it->first] += dt;
            }
            it->second.duration_left -= dt;
            if (it->second.duration_left < 0.0)
            {
                if (debug)
                {
                    debug_msg.emplace_back(it->first + " fades.");
                }
                if (it->first == "battle_stance")
                {
                    if (rage > 25.0)
                    {
                        rage_lost_stance += rage - 25;
                        rage = 25;
                    }
                }
                else if (it->first == "execute_rage_batch")
                {
                    if (rage > rage_before_execute)
                    {
                        rage_lost_exec += rage - rage_before_execute;
                    }
                    rage_spent_executing += rage;
                    rage = 0;
                    debug_msg.emplace_back("Current rage: 0");
                }
                if (it->second.special_stats.hit > 0.0 || it->second.special_stats.critical_strike > 0.0)
                {
                    need_to_recompute_hittables = true;
                }
                (*simulation_special_stats) -= it->second.special_stats;
                it = stat_gains.erase(it);
            }
            else
            {
                next_event = std::min(next_event, it->second.duration_left);
                ++it;
            }
        }
        i = 0;
        while (i < hit_gains.size())
        {
            if (!performance_mode)
            {
                aura_uptime[hit_gains[i].id] += dt;
            }
            hit_gains[i].duration_left -= dt;
            if (hit_gains[i].duration_left < 0.0)
            {
                if (debug)
                {
                    debug_msg.emplace_back(hit_gains[i].id + " fades.");
                }
                for (size_t j = 0; j < (*hit_effects_mh).size(); j++)
                {
                    if (hit_gains[i].id == (*hit_effects_mh)[j].name)
                    {
                        (*hit_effects_mh).erase((*hit_effects_mh).begin() + j);
                    }
                }
                for (size_t j = 0; j < (*hit_effects_oh).size(); j++)
                {
                    if (hit_gains[i].id == (*hit_effects_oh)[j].name)
                    {
                        (*hit_effects_oh).erase((*hit_effects_oh).begin() + i);
                    }
                }
                hit_gains.erase(hit_gains.begin() + i);
            }
            else
            {
                next_event = std::min(next_event, hit_gains[i].duration_left);
                ++i;
            }
        }
        i = 0;
        while (i < use_effects.size())
        {
            if (time_left - use_effects[i].duration - 1.5 < 0.0 && global_cooldown < 0.0 &&
                rage >= -use_effects[i].rage_boost)
            {
                if (debug)
                {
                    debug_msg.emplace_back("Activating: " + use_effects[i].name);
                }
                if (!use_effects[i].hit_effects.empty())
                {
                    add_hit_effect(use_effects[i].name, use_effects[i].hit_effects[0],
                                   use_effects[i].hit_effects[0].duration);
                }
                else if (!use_effects[i].over_time_effects.empty())
                {
                    add_over_time_effect(use_effects[i].over_time_effects[0], int(current_time + 1));
                }
                else
                {
                    add(use_effects[i].name, use_effects[i].get_special_stat_equivalent(*simulation_special_stats),
                        use_effects[i].duration);
                }
                rage += use_effects[i].rage_boost;
                rage = std::min(100.0, rage);
                use_effects.erase(use_effects.begin() + i);
                if (use_effects[i].triggers_gcd)
                {
                    global_cooldown = 1.5;
                }
            }
            else
            {
                ++i;
            }
        }
        i = 0;
        while (i < over_time_buffs.size())
        {
            if ((int(current_time) - over_time_buffs[i].init_time) / over_time_buffs[i].interval >=
                over_time_buffs[i].current_ticks)
            {
                rage += over_time_buffs[i].rage_gain;
                (*simulation_special_stats) += over_time_buffs[i].special_stats;
                rage = std::min(100.0, rage);
                over_time_buffs[i].current_ticks++;
                if (over_time_buffs[i].damage > 0.0)
                {
                    deep_wounds_damage += over_time_buffs[i].damage;
                    deep_wounds_timestamps.push_back(current_time);
                }
                if (debug)
                {
                    if (over_time_buffs[i].rage_gain > 0)
                    {
                        debug_msg.emplace_back("Over time effect: " + over_time_buffs[i].id +
                                               " tick. Current rage: " + std::to_string(int(rage)));
                    }
                    else if (over_time_buffs[i].damage > 0)
                    {
                        debug_msg.emplace_back("Over time effect: " + over_time_buffs[i].id +
                                               " tick. Damage: " + std::to_string(int(over_time_buffs[i].damage)));
                    }
                    else
                    {
                        debug_msg.emplace_back("Over time effect: " + over_time_buffs[i].id + " tick.");
                    }
                }
            }
            if (over_time_buffs[i].current_ticks == over_time_buffs[i].total_ticks)
            {
                if (debug)
                {
                    debug_msg.emplace_back("Over time effect: " + over_time_buffs[i].id + " fades.");
                }
                over_time_buffs.erase(over_time_buffs.begin() + i);
                if (over_time_buffs.empty())
                {
                    min_interval = 10;
                }
                else
                {
                    for (const auto& over_time_buff : over_time_buffs)
                    {
                        min_interval = std::min(over_time_buff.interval, min_interval);
                    }
                }
            }
            else
            {
                i++;
            }
        }
        return debug_msg;
    }

    void add(const std::string& name, const Special_stats& special_stats, double duration_left)
    {
        auto it = stat_gains.find(name);
        if (it != stat_gains.end())
        {
            it->second.duration_left = duration_left;
            return;
        }
        (*simulation_special_stats) += special_stats;
        if (special_stats.hit > 0.0 || special_stats.critical_strike > 0.0)
        {
            need_to_recompute_hittables = true;
        }
        stat_gains[name] = {special_stats, duration_left};
    }

    void add_hit_effect(const std::string& name, const Hit_effect& hit_effect, double duration_left)
    {
        (*hit_effects_mh).emplace_back(hit_effect);
        (*hit_effects_oh).emplace_back(hit_effect);
        hit_gains.emplace_back(name, duration_left);
    }

    void add_over_time_effect(const Over_time_effect& over_time_effect, int init_time)
    {
        if (over_time_effect.name == "Deep_wounds")
        {
            for (auto& over_time_buff : over_time_buffs)
            {
                if (over_time_buff.id == "Deep_wounds")
                {
                    over_time_buff.damage = std::max(over_time_effect.damage, over_time_buff.damage);
                    over_time_buff.total_ticks =
                        (over_time_effect.duration + init_time - over_time_buff.init_time) / over_time_buff.interval;
                    return;
                }
            }
        }
        min_interval = std::min(over_time_effect.interval, min_interval);
        over_time_buffs.emplace_back(over_time_effect.name, over_time_effect.special_stats, init_time,
                                     over_time_effect.rage_gain, over_time_effect.damage, over_time_effect.interval,
                                     over_time_effect.duration);
    }

    bool can_do_overpower()
    {
        auto it = stat_gains.find("overpower_aura");
        return it != stat_gains.end();
    }

    bool need_to_recompute_hittables{false};
    bool performance_mode{false};
    Special_stats* simulation_special_stats;
    std::map<std::string, Combat_buff> stat_gains;
    std::vector<Hit_buff> hit_gains;
    std::vector<Over_time_buff> over_time_buffs;
    std::vector<Hit_effect>* hit_effects_mh;
    std::vector<Hit_effect>* hit_effects_oh;
    std::vector<Use_effect> use_effects;
    double next_event = 10;
    double rage_before_execute{};
    double rage_spent_executing{};
    int min_interval = 10;
    std::map<std::string, double> aura_uptime;
    double deep_wounds_damage{};
    std::vector<double> deep_wounds_timestamps{};
};

#endif // WOW_SIMULATOR_BUFF_MANAGER_HPP
