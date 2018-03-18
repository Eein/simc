#include "simulationcraft.hpp"
#include "sc_warlock.hpp"

namespace warlock {
    namespace pets {
        namespace felguard {
            struct legion_strike_t : public warlock_pet_melee_attack_t {
                legion_strike_t(warlock_pet_t* p) : warlock_pet_melee_attack_t(p, "Legion Strike") {
                    aoe = -1;
                    weapon = &(p->main_hand_weapon);
                }
                virtual bool ready() override {
                    if (p()->special_action->get_dot()->is_ticking()) return false;
                    return warlock_pet_melee_attack_t::ready();
                }
            };
            struct axe_toss_t : public warlock_pet_spell_t {
                axe_toss_t(warlock_pet_t* p) : warlock_pet_spell_t("Axe Toss", p, p -> find_spell(89766)) {
                }

                void execute() override {
                    warlock_pet_spell_t::execute();
                    p()->trigger_sephuzs_secret(execute_state, MECHANIC_STUN);
                }
            };
            struct felstorm_tick_t : public warlock_pet_melee_attack_t {
                felstorm_tick_t(warlock_pet_t* p, const spell_data_t& s) : warlock_pet_melee_attack_t("felstorm_tick", p, s.effectN(1).trigger()) {
                    aoe = -1;
                    background = true;
                    weapon = &(p->main_hand_weapon);
                }
            };
            struct felstorm_t : public warlock_pet_melee_attack_t {
                felstorm_t(warlock_pet_t* p) : warlock_pet_melee_attack_t("felstorm", p, p -> find_spell(89751)) {
                    tick_zero = true;
                    hasted_ticks = false;
                    may_miss = false;
                    may_crit = false;
                    weapon_multiplier = 0;

                    dynamic_tick_action = true;
                    tick_action = new felstorm_tick_t(p, data());
                }

                virtual void cancel() override {
                    warlock_pet_melee_attack_t::cancel();
                    get_dot()->cancel();
                }

                virtual void execute() override {
                    warlock_pet_melee_attack_t::execute();
                    p()->melee_attack->cancel();
                }

                virtual void last_tick(dot_t* d) override {
                    warlock_pet_melee_attack_t::last_tick(d);
                    if (!p()->is_sleeping() && !p()->melee_attack->target->is_sleeping())
                        p()->melee_attack->execute();
                }
            };

            felguard_pet_t::felguard_pet_t(sim_t* sim, warlock_t* owner, const std::string& name) :
                warlock_pet_t(sim, owner, name, PET_FELGUARD, name != "felguard") {
                action_list_str += "/felstorm";
                action_list_str += "/legion_strike,if=cooldown.felstorm.remains";
                owner_coeff.ap_from_sp = 1.1; // HOTFIX
                owner_coeff.ap_from_sp *= 1.2; // PTR
            }

            void felguard_pet_t::init_base_stats() {
                warlock_pet_t::init_base_stats();

                melee_attack = new warlock_pet_melee_t(this);
                special_action = new felstorm_t(this);
                special_action_two = new axe_toss_t(this);
            }

            action_t* felguard_pet_t::create_action(const std::string& name, const std::string& options_str) {
                if (name == "legion_strike") return new legion_strike_t(this);
                if (name == "felstorm") return new felstorm_t(this);
                if (name == "axe_toss") return new axe_toss_t(this);

                return warlock_pet_t::create_action(name, options_str);
            }
        }
        namespace dreadstalker {
          struct dreadbite_t : public warlock_pet_melee_attack_t
          {
            timespan_t dreadstalker_duration;
            double t21_4pc_increase;

            dreadbite_t(warlock_pet_t* p) :
              warlock_pet_melee_attack_t("Dreadbite", p, p -> find_spell(205196))
            {
              weapon = &(p->main_hand_weapon);
              dreadstalker_duration = p->find_spell(193332)->duration() +
                (p->o()->sets->has_set_bonus(WARLOCK_DEMONOLOGY, T19, B4)
                  ? p->o()->sets->set(WARLOCK_DEMONOLOGY, T19, B4)->effectN(1).time_value()
                  : timespan_t::zero());
              t21_4pc_increase = p->o()->sets->set(WARLOCK_DEMONOLOGY, T21, B4)->effectN(1).percent();
            }

            virtual bool ready() override
            {
              if (p()->dreadbite_executes <= 0)
                return false;

              return warlock_pet_melee_attack_t::ready();
            }

            virtual double action_multiplier() const override
            {
              double m = warlock_pet_melee_attack_t::action_multiplier();

              if (p()->o()->sets->has_set_bonus(WARLOCK_DEMONOLOGY, T21, B4) && p()->bites_executed == 1)
                m *= 1.0 + t21_4pc_increase;

              return m;
            }

            void execute() override
            {
              warlock_pet_melee_attack_t::execute();

              p()->dreadbite_executes--;
            }

            void impact(action_state_t* s) override
            {
              warlock_pet_melee_attack_t::impact(s);

              p()->bites_executed++;
            }
          };

          dreadstalker_t::dreadstalker_t(sim_t* sim, warlock_t* owner) : warlock_pet_t(sim, owner, "dreadstalker", PET_DREADSTALKER)
          {
            action_list_str = "travel/dreadbite";
            regen_type = REGEN_DISABLED;
            owner_coeff.health = 0.4;
            owner_coeff.ap_from_sp = 1.1; // HOTFIX
          }

          void dreadstalker_t::init_base_stats()
          {
            warlock_pet_t::init_base_stats();
            resources.base[RESOURCE_ENERGY] = 0;
            base_energy_regen_per_second = 0;
            melee_attack = new warlock_pet_melee_t(this);
          }

          void dreadstalker_t::arise()
          {
            warlock_pet_t::arise();

            dreadbite_executes = 1;
            bites_executed = 0;

            if (o()->sets->has_set_bonus(WARLOCK_DEMONOLOGY, T21, B4))
              t21_4pc_reset = false;
          }

          void dreadstalker_t::demise() {
            warlock_pet_t::demise();

            if (rng().roll(find_spell(267102)->effectN(2).percent()))
            {
              o()->buffs.demonic_core->trigger();
            }
          }

          action_t* dreadstalker_t::create_action(const std::string& name, const std::string& options_str) 
          {
            if (name == "dreadbite") return new dreadbite_t(this);

            return warlock_pet_t::create_action(name, options_str);
          }
        }
        namespace wild_imp {
          struct fel_firebolt_t : public warlock_pet_spell_t
          {
            fel_firebolt_t(warlock_pet_t* p) : warlock_pet_spell_t("fel_firebolt", p, p -> find_spell(104318))
            {
            }

            virtual bool ready() override
            {
              if (!p()->resource_available(p()->primary_resource(), 20))
                p()->demise();

              return spell_t::ready();
            }

            void execute() override
            {
              warlock_pet_spell_t::execute();
            }

            virtual void impact(action_state_t* s) override
            {
              warlock_pet_spell_t::impact(s);
            }

            virtual double composite_target_multiplier(player_t* target) const override
            {
              double m = warlock_pet_spell_t::composite_target_multiplier(target);

              return m;
            }
          };

          wild_imp_pet_t::wild_imp_pet_t(sim_t* sim, warlock_t* owner) : warlock_pet_t(sim, owner, "wild_imp", PET_WILD_IMP)
          {

          }

          void wild_imp_pet_t::init_base_stats()
          {
            warlock_pet_t::init_base_stats();

            action_list_str = "fel_firebolt";

            resources.base[RESOURCE_ENERGY] = 100;
            base_energy_regen_per_second = 0;
          }

          void wild_imp_pet_t::dismiss(bool expired)
          {
            pet_t::dismiss(expired);
          }

          action_t* wild_imp_pet_t::create_action(const std::string& name,const std::string& options_str)
          {
            if (name == "fel_firebolt")
            {
              firebolt = new fel_firebolt_t(this);
              return firebolt;
            }

            return warlock_pet_t::create_action(name, options_str);
          }

          void wild_imp_pet_t::arise()
          {
            warlock_pet_t::arise();

            if (isnotdoge)
            {
              firebolt->cooldown->start(timespan_t::from_millis(rng().range(500, 1500)));
            }
          }

          void wild_imp_pet_t::demise() {
            warlock_pet_t::demise();

            if (rng().roll(find_spell(267102)->effectN(1).percent()))
            {
              o()->buffs.demonic_core->trigger();
            }
          }
        }
    }

    namespace actions_demonology {
      using namespace actions;

      struct shadow_bolt_t : public warlock_spell_t
      {
        shadow_bolt_t(warlock_t* p, const std::string& options_str) :
          warlock_spell_t(p, "Shadow Bolt", p->specialization())
        {
          parse_options(options_str);
          energize_type = ENERGIZE_ON_CAST;
          energize_resource = RESOURCE_SOUL_SHARD;
          energize_amount = 1;
        }

        void execute() override
        {
          warlock_spell_t::execute();
          if (p()->talents.demonic_calling->ok() && rng().roll(p()->talents.demonic_calling->proc_chance()))
            p()->buffs.demonic_calling->trigger();
        }
      };

      struct hand_of_guldan_t : public warlock_spell_t {
            int shards_used;

            hand_of_guldan_t(warlock_t* p, const std::string& options_str) : warlock_spell_t(p, "Hand of Gul'dan") {
                parse_options(options_str);
                aoe = -1;
                shards_used = 0;

                parse_effect_data(p->find_spell(86040)->effectN(1));
            }

            virtual timespan_t travel_time() const override {
                return timespan_t::from_millis(700);
            }

            virtual bool ready() override {
                bool r = warlock_spell_t::ready();
                if (p()->resources.current[RESOURCE_SOUL_SHARD] == 0.0)
                    r = false;

                return r;
            }

            virtual double action_multiplier() const override
            {
              double m = warlock_spell_t::action_multiplier();

              m *= last_resource_cost;

              return m;
            }

            void consume_resource() override {
                warlock_spell_t::consume_resource();

                shards_used = last_resource_cost;

                if (last_resource_cost == 1.0)
                    p()->procs.one_shard_hog->occur();
                if (last_resource_cost == 2.0)
                    p()->procs.two_shard_hog->occur();
                if (last_resource_cost == 3.0)
                    p()->procs.three_shard_hog->occur();
            }

            virtual void impact(action_state_t* s) override {
                warlock_spell_t::impact(s);

                if (result_is_hit(s->result)) {
                  int j = 0;

                  if (p()->talents.riders->ok()) {
                    if (rng().roll(p()->talents.riders->effectN(1).percent()*shards_used))
                    {
                      ++shards_used;
                    }
                  }

                  for (auto imp : p()->warlock_pet_list.wild_imps)
                  {
                    if (imp->is_sleeping())
                    {
                      imp->summon();
                      if (++j == shards_used) break;
                    }
                  }

                  if (p()->sets->has_set_bonus(WARLOCK_DEMONOLOGY, T21, B2)) {
                      for (int i = 0; i < shards_used; i++) {
                          p()->buffs.rage_of_guldan->trigger();
                      }
                  }
                }
            }
        };

      struct demonbolt_t : public warlock_spell_t {
        demonbolt_t(warlock_t* p, const std::string& options_str) : warlock_spell_t(p, "Demonbolt") {
          parse_options(options_str);
          energize_type = ENERGIZE_ON_CAST;
          energize_resource = RESOURCE_SOUL_SHARD;
          energize_amount = 2;
        }

        virtual timespan_t execute_time() const override
        {
          if ( p()->buffs.demonic_core->check() )
          {
            return timespan_t::zero();
          }

          return warlock_spell_t::execute_time();
        }

        void execute() override
        {
          warlock_spell_t::execute();
          if ( p()->buffs.demonic_core->check() )
            p()->buffs.demonic_core->decrement();
          if (p()->talents.demonic_calling->ok() && rng().roll(p()->talents.demonic_calling->proc_chance()))
            p()->buffs.demonic_calling->trigger();
        }
      };

      struct call_dreadstalkers_t : public warlock_spell_t {
        timespan_t dreadstalker_duration;
        int dreadstalker_count;

        call_dreadstalkers_t(warlock_t* p, const std::string& options_str) : warlock_spell_t(p, "Call Dreadstalkers") {
          parse_options(options_str);
          may_crit = false;
          dreadstalker_duration = p->find_spell(193332)->duration() + (p->sets->has_set_bonus(WARLOCK_DEMONOLOGY, T19, B4) ? p->sets->set(WARLOCK_DEMONOLOGY, T19, B4)->effectN(1).time_value() : timespan_t::zero());
          dreadstalker_count = data().effectN(1).base_value();
        }

        double cost() const override
        {
          double c = warlock_spell_t::cost();

          if (p()->buffs.demonic_calling->check())
          {
            return 0;
          }

          return c;
        }

        virtual timespan_t execute_time() const override
        {
          if (p()->buffs.demonic_calling->check())
          {
            return timespan_t::zero();
          }

          return warlock_spell_t::execute_time();
        }

        virtual void execute() override
        {
          warlock_spell_t::execute();

          int j = 0;

          for (size_t i = 0; i < p()->warlock_pet_list.dreadstalkers.size(); i++)
          {
            if (p() -> warlock_pet_list.dreadstalkers[i]->is_sleeping())
            {
              p()->warlock_pet_list.dreadstalkers[i]->summon(dreadstalker_duration);
              p()->procs.dreadstalker_debug->occur();

              if (p()->sets->has_set_bonus(WARLOCK_DEMONOLOGY, T21, B2))
              {
                p()->warlock_pet_list.dreadstalkers[i]->buffs.rage_of_guldan->set_duration(dreadstalker_duration);
                p()->warlock_pet_list.dreadstalkers[i]->buffs.rage_of_guldan->set_default_value(p()->buffs.rage_of_guldan->stack_value());
                p()->warlock_pet_list.dreadstalkers[i]->buffs.rage_of_guldan->trigger();
              }
              
              /*
              if (p()->legendary.wilfreds_sigil_of_superior_summoning_flag && !p()->talents.grimoire_of_supremacy->ok())
              {
                p()->cooldowns.doomguard->adjust(p()->legendary.wilfreds_sigil_of_superior_summoning);
                p()->cooldowns.infernal->adjust(p()->legendary.wilfreds_sigil_of_superior_summoning);
                p()->procs.wilfreds_dog->occur();
              }
              */
              if (++j == dreadstalker_count) break;
            }
          }

          p()->buffs.demonic_calling->expire();
          p()->buffs.rage_of_guldan->expire();

          if (p()->sets->has_set_bonus(WARLOCK_DEMONOLOGY, T20, B4))
          {
            p()->buffs.dreaded_haste->trigger();
          }
        }
      };

      struct implosion_t : public warlock_spell_t
      {
        struct implosion_aoe_t : public warlock_spell_t
        {

          implosion_aoe_t(warlock_t* p) :
            warlock_spell_t("implosion_aoe", p, p -> find_spell(196278))
          {
            aoe = -1;
            dual = true;
            background = true;
            callbacks = false;

            p->spells.implosion_aoe = this;
          }
        };

        implosion_aoe_t* explosion;

        implosion_t(warlock_t* p, const std::string& options_str) : warlock_spell_t("implosion", p),explosion(new implosion_aoe_t(p))
        {
          parse_options(options_str);
          aoe = -1;
          add_child(explosion);
        }

        virtual bool ready() override
        {
          bool r = warlock_spell_t::ready();

          if (r)
          {
            for (auto imp : p()->warlock_pet_list.wild_imps)
            {
              if (!imp->is_sleeping())
                return true;
            }
          }
          return false;
        }

        virtual void execute() override
        {
          warlock_spell_t::execute();
          for (auto imp : p()->warlock_pet_list.wild_imps)
          {
            if (!imp->is_sleeping())
            {
              explosion->execute();
              imp->dismiss(true);
            }
          }
        }
      };

      // Talents
      struct power_siphon_t : public warlock_spell_t {
        power_siphon_t(warlock_t* p, const std::string& options_str) : warlock_spell_t("power_siphon", p, p -> talents.power_siphon) {
          parse_options(options_str);
          harmful = false;
          ignore_false_positive = true;
        }

        virtual void execute() override
        {
          warlock_spell_t::execute();

          int i = 0;
          for (auto imp : p()->warlock_pet_list.wild_imps)
          {
            if (!imp->is_sleeping())
            {
              p()->buffs.demonic_core->trigger();
              imp->dismiss(true);
              if (++i == p()->talents.power_siphon->effectN(1).base_value()) break;
            }
          }
        }
      };

      struct doom_t : public warlock_spell_t {
        doom_t(warlock_t* p, const std::string& options_str) : warlock_spell_t("doom", p, p -> talents.doom) {
            parse_options(options_str);

            base_tick_time = p->find_spell(265412)->duration();
            dot_duration = p->find_spell(265412)->duration();
            spell_power_mod.tick = p->find_spell(265469)->effectN(1).sp_coeff();

            may_crit = true;
            hasted_ticks = true;
        }

        timespan_t composite_dot_duration(const action_state_t* s) const override {
            timespan_t duration = warlock_spell_t::composite_dot_duration(s);
            return duration * p()->cache.spell_haste();
        }

        virtual double action_multiplier()const override {
            double m = warlock_spell_t::action_multiplier();
            return m;
        }

        virtual void tick(dot_t* d) override {
            warlock_spell_t::tick(d);

            if (d->state->result == RESULT_HIT || result_is_hit(d->state->result)) {
                if (p()->sets->has_set_bonus(WARLOCK_DEMONOLOGY, T19, B2) && rng().roll(p()->sets->set(WARLOCK_DEMONOLOGY, T19, B2)->effectN(1).percent()))
                    p()->resource_gain(RESOURCE_SOUL_SHARD, 1, p()->gains.t19_2pc_demonology);
            }
        }

        virtual void execute() override
        {
          warlock_spell_t::execute();
        }
      };

      struct grimoire_of_service_t : public summon_pet_t {
            grimoire_of_service_t(warlock_t* p, const std::string& pet_name, const std::string& options_str) :
                summon_pet_t("service_" + pet_name, p, p -> talents.grimoire_of_service -> ok() ? p -> find_class_spell("Grimoire: " + pet_name) : spell_data_t::not_found()) {
                parse_options(options_str);
                cooldown = p->get_cooldown("grimoire_of_service");
                cooldown->duration = data().cooldown();
                summoning_duration = data().duration() + timespan_t::from_millis(1);
            }
            virtual void execute() override {
                pet->is_grimoire_of_service = true;
                summon_pet_t::execute();
            }
            bool init_finished() override {
                if (pet) {
                    pet->summon_stats = stats;
                }
                return summon_pet_t::init_finished();
            }
        };
    } // end actions namespace
    namespace buffs {
    } // end buffs namespace

      // add actions
    action_t* warlock_t::create_action_demonology(const std::string& action_name, const std::string& options_str) {
        using namespace actions_demonology;

        if (action_name == "shadow_bolt") return new          shadow_bolt_t(this, options_str);
        if (action_name == "demonbolt") return new            demonbolt_t(this, options_str);
        if (action_name == "hand_of_guldan") return new       hand_of_guldan_t(this, options_str);
        if (action_name == "implosion") return new            implosion_t(this, options_str);

        if (action_name == "doom")          return new        doom_t(this, options_str);
        if (action_name == "power_siphon") return new         power_siphon_t(this, options_str);

        if (action_name == "call_dreadstalkers") return new   call_dreadstalkers_t(this, options_str);
        if (action_name == "summon_felguard") return new      summon_main_pet_t("felguard", this);
        if (action_name == "service_felguard") return new     grimoire_of_service_t(this, "felguard", options_str);
        if (action_name == "service_felhunter") return new    grimoire_of_service_t(this, "felhunter", options_str);
        if (action_name == "service_imp") return new          grimoire_of_service_t(this, "imp", options_str);
        if (action_name == "service_succubus") return new     grimoire_of_service_t(this, "succubus", options_str);
        if (action_name == "service_voidwalker") return new   grimoire_of_service_t(this, "voidwalker", options_str);

        return nullptr;
    }

    void warlock_t::create_buffs_demonology() {
      buffs.demonic_core = make_buff(this, "demonic_core", find_spell(264173))
        ->set_refresh_behavior(buff_refresh_behavior::DURATION);
      //Talents
      buffs.demonic_calling = make_buff(this, "demonic_calling", talents.demonic_calling->effectN(1).trigger());
      //Tier
      buffs.rage_of_guldan = make_buff(this, "rage_of_guldan", sets->set(WARLOCK_DEMONOLOGY, T21, B2)->effectN(1).trigger())
        ->set_duration(find_spell(257926)->duration())
        ->set_max_stack(find_spell(257926)->max_stacks())
        ->set_default_value(find_spell(257926)->effectN(1).base_value())
        ->set_refresh_behavior(buff_refresh_behavior::DURATION);
      buffs.dreaded_haste = make_buff<haste_buff_t>(this, "dreaded_haste", sets->set(WARLOCK_DEMONOLOGY, T20, B4)->effectN(1).trigger())
        ->set_default_value(sets->set(WARLOCK_DEMONOLOGY, T20, B4)->effectN(1).trigger()->effectN(1).percent());
    }

    void warlock_t::init_spells_demonology() {
        spec.demonology                         = find_specialization_spell(137044);
        mastery_spells.master_demonologist      = find_mastery_spell(WARLOCK_DEMONOLOGY);
        // spells
        // Talents
        talents.demonic_strength                = find_talent_spell("Demonic Strength");
        talents.demonic_calling                 = find_talent_spell("Demonic Calling");
        talents.doom                            = find_talent_spell("Doom");
        talents.riders                          = find_talent_spell("Riders");
        talents.power_siphon                    = find_talent_spell("Power Siphon");
        talents.summon_vilefiend                = find_talent_spell("Summon Vilefiend");
        talents.demonic_strength                = find_talent_spell("Demonic Strength");
        talents.biliescourge_bombers            = find_talent_spell("Biliescourge Bombers");
        talents.demonic_consumption             = find_talent_spell("Demonic Consumption");
        talents.grimoire_of_service             = find_talent_spell("Grimoire of Service");
        talents.inner_demons                    = find_talent_spell("Inner Demons");
        talents.nether_portal                   = find_talent_spell("Nether Portal");
    }

    void warlock_t::init_gains_demonology() {

    }

    void warlock_t::init_rng_demonology() {
    }

    void warlock_t::init_procs_demonology() {
      procs.dreadstalker_debug = get_proc("dreadstalker_debug");
    }

    void warlock_t::create_options_demonology() {
    }

    void warlock_t::create_apl_demonology() {
        action_priority_list_t* def = get_action_priority_list("default");
       
        def -> add_action("power_siphon,if=talent.power_siphon.enabled");
        def -> add_action("doom,if=talent.doom.enabled&refreshable");
        def -> add_action("call_dreadstalkers");
        def -> add_action("hand_of_guldan,if=soul_shard>=3");
        def -> add_action("demonbolt,if=buff.demonic_core.stack>0");
        def -> add_action("shadow_bolt");
    }

    using namespace unique_gear;
    using namespace actions;

    void warlock_t::legendaries_demonology() {

    }
}