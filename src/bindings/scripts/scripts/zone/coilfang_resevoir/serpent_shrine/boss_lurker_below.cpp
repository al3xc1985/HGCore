/* Copyright (C) 2006 - 2009 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "precompiled.h"
#include "def_serpent_shrine.h"
#include "../../../creature/simple_ai.h"
#include "Spell.h"

#define EMOTE_SPOUT "takes a deep breath."

enum lurkerAdds
{
    MOB_COILFANG_GUARDIAN = 21873,
    MOB_COILFANG_AMBUSHER = 21865
};

float addPos[9][4] =
{
    { MOB_COILFANG_AMBUSHER, 2.8553810, -459.823914, -19.182686},   //MOVE_AMBUSHER_1 X, Y, Z
    { MOB_COILFANG_AMBUSHER, 12.400000, -466.042267, -19.182686},   //MOVE_AMBUSHER_2 X, Y, Z
    { MOB_COILFANG_AMBUSHER, 51.366653, -460.836060, -19.182686},   //MOVE_AMBUSHER_3 X, Y, Z
    { MOB_COILFANG_AMBUSHER, 62.597980, -457.433044, -19.182686},   //MOVE_AMBUSHER_4 X, Y, Z
    { MOB_COILFANG_AMBUSHER, 77.607452, -384.302765, -19.182686},   //MOVE_AMBUSHER_5 X, Y, Z
    { MOB_COILFANG_AMBUSHER, 63.897900, -378.984924, -19.182686},   //MOVE_AMBUSHER_6 X, Y, Z
    { MOB_COILFANG_GUARDIAN, 34.447250, -387.333618, -19.182686},   //MOVE_GUARDIAN_1 X, Y, Z
    { MOB_COILFANG_GUARDIAN, 14.388216, -423.468018, -19.625271},   //MOVE_GUARDIAN_2 X, Y, Z
    { MOB_COILFANG_GUARDIAN, 42.471519, -445.115295, -19.769423}    //MOVE_GUARDIAN_3 X, Y, Z
};

enum lurkerSpells
{
    SPELL_SPOUT      = 37433,
    SPELL_GEYSER     = 37478,
    SPELL_WHIRL      = 37363,
    SPELL_WATERBOLT  = 37138,
    SPELL_SUBMERGE   = 37550,
    SPELL_SPOUT_VIS  = 42835
};

#define SPOUT_WIDTH 1.0f

struct TRINITY_DLL_DECL boss_the_lurker_belowAI : public Scripted_NoMovementAI
{
    boss_the_lurker_belowAI(Creature *c) : Scripted_NoMovementAI(c), m_summons(m_creature)
    {
        pInstance = (ScriptedInstance*)c->GetInstanceData();
    }

    ScriptedInstance* pInstance;
    SummonList m_summons;

    bool m_rotating;
    bool m_submerged;

    uint32 m_rotateTimer;
    uint32 m_phaseTimer;
    uint32 m_whirlTimer;
    uint32 m_geyserTimer;
    uint32 m_checkTimer;

    std::map<uint64, uint8> m_immunemap;

    void Reset()
    {
        if(pInstance)
        {
            pInstance->SetData(DATA_THELURKERBELOWEVENT, NOT_STARTED);
            pInstance->SetData(DATA_STRANGE_POOL, NOT_STARTED);
        }
        
        // Apply imunities: move to DB !
        m_creature->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_HASTE_SPELLS, true);
        m_creature->ApplySpellImmune(1, IMMUNITY_EFFECT, SPELL_EFFECT_INTERRUPT_CAST, true);
        
        // Do not fall to the ground ;]
        me->AddUnitMovementFlag(MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_LEVITATING);

        // Set reactstate to: Aggresive
        me->SetReactState(REACT_AGGRESSIVE);
        me->SetVisibility(VISIBILITY_OFF);

        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_ATTACKABLE_2);

        // Timers
        m_rotateTimer = 45000;
        m_phaseTimer = 5000;
        m_whirlTimer = 17000;
        m_geyserTimer = 0;
        m_checkTimer = 3000;

        // Bools
        m_rotating = false;
        m_submerged = true;

        m_summons.DespawnAll();
        ForceSpellCast(me, SPELL_SUBMERGE, INTERRUPT_AND_CAST_INSTANTLY);
    }

    void EnterCombat(Unit *who)
    {
        if (pInstance)
            pInstance->SetData(DATA_THELURKERBELOWEVENT, IN_PROGRESS);

        AttackStart(who);
    }
    
    void AttackStart(Unit *pWho)
    {
        if(me->HasReactState(REACT_PASSIVE))
            return;

        Scripted_NoMovementAI::AttackStart(pWho);
    }

    void JustDied(Unit* Killer)
    {
        if (pInstance)
            pInstance->SetData(DATA_THELURKERBELOWEVENT, DONE);

        m_summons.DespawnAll();
    }
    
    void MovementInform(uint32 type, uint32 data)
    {
        if (type == ROTATE_MOTION_TYPE)
        {
            if (data == 1) //Rotate movegen update
            {
                me->SetSelection(0);

                WorldLocation wLoc;
                me->GetClosePoint(wLoc.x, wLoc.y, wLoc.z, 0, 5.0f, 0);
               
                // Just visual to realize current orientation ;]
                if (Unit *vBunny = me->SummonCreature(721, wLoc.x, wLoc.y, wLoc.z +1.0f, 0, TEMPSUMMON_TIMED_DESPAWN, 200))
                    vBunny->CastSpell(vBunny, 39989, false);
       
                Map *pMap = me->GetMap();
                Map::PlayerList const& players = pMap->GetPlayers();
                for(Map::PlayerList::const_iterator i = players.begin(); i != players.end(); ++i)
                {
                    Player *pPlayer = i->getSource();
                    // Check for players that recently has been taken as spout targets( now they are in the air ? )
                    if (uint8 count = m_immunemap[pPlayer->GetGUID()])
                    {
                        if (count >= 5) // Reset counter so player again can be considered as spout target
                            m_immunemap[pPlayer->GetGUID()] = 0;
                        else
                        {
                            m_immunemap[pPlayer->GetGUID()]++;
                            continue;
                        }
                    }

                    if (!me->IsWithinDistInMap(pPlayer, 100.0f))
                        continue;

                    if (IsInLineAboveWater(pPlayer))
                    {
                        ForceSpellCast(pPlayer, SPELL_SPOUT, INTERRUPT_AND_CAST_INSTANTLY);
                        m_immunemap[pPlayer->GetGUID()] = 1;
                    }

                }
            }
            else // data == 0 finalize
            {
                me->SetReactState(REACT_AGGRESSIVE);
                m_rotating = false;
            }
        }
    }

    bool IsInLineAboveWater(Player *pWho)
    {
        if (pWho->IsInWater() || !me->HasInArc(M_PI, pWho))
            return false;

        float wLevel = pWho->GetMap()->GetWaterLevel(pWho->GetPositionX(), pWho->GetPositionY());
        if(pWho->GetPositionZ() < wLevel)
            return false;

        return (abs(sin(me->GetAngle(pWho) - me->GetOrientation())) * me->GetExactDistance2d(pWho->GetPositionX(), pWho->GetPositionY()) < SPOUT_WIDTH);
    }

    void MoveInLineOfSight(Unit *who)
    {
        if (me->HasReactState(REACT_PASSIVE))
            return;
    }

    void DoMeleeAttackIfReady()
    {
        if(me->hasUnitState(UNIT_STAT_CASTING))
            return;

        //Make sure our attack is ready and we aren't currently casting before checking distance
        if (me->isAttackReady())
        {
            //If we are within range melee the target
            if (me->IsWithinMeleeRange(me->getVictim()))
            {
                me->AttackerStateUpdate(me->getVictim());
                me->resetAttackTimer();
            }
            else
            {
                if (Unit *pTarget = me->SelectNearestTarget(5.0f))
                {
                    me->AttackerStateUpdate(pTarget);
                    me->resetAttackTimer();
                }
                else
                {
                    if (Unit *pTarget = SelectUnit(SELECT_TARGET_RANDOM, 0, 100.0f, true, 0, 5.0f))
                        AddSpellToCast(pTarget, SPELL_WATERBOLT);
                }
            }
        }
    }

    void SummonAdds()
    {
        for (uint8 i = 0; i < 9; i++)
        {
            Creature *pSummon = me->SummonCreature(addPos[i][0], addPos[i][1], addPos[i][2], addPos[i][3], 0, TEMPSUMMON_DEAD_DESPAWN, 2000);
            m_summons.Summon(pSummon);
        }
    }

    void UpdateAI(const uint32 diff)
    {
        if (pInstance->GetData(DATA_STRANGE_POOL) != IN_PROGRESS)
            return;

        if (me->GetVisibility() == VISIBILITY_OFF)
        {
            if (m_phaseTimer < diff)
            {
                me->SetVisibility(VISIBILITY_ON);
                me->RemoveAurasDueToSpell(SPELL_SUBMERGE);
                
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_ATTACKABLE_2);
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                
                m_submerged = false;
                m_phaseTimer = 120000;
            }
            else
                m_phaseTimer -= diff;

            return;
        }

        if (m_phaseTimer < diff)
        {
            if (m_submerged)
            {
                me->RemoveAurasDueToSpell(SPELL_SUBMERGE);
                
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_ATTACKABLE_2);
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                
                m_submerged = false;
                m_rotateTimer = 3000;
                m_phaseTimer = 120000;
            }
            else
            {
                ForceSpellCast(me, SPELL_SUBMERGE, INTERRUPT_AND_CAST_INSTANTLY);

                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_ATTACKABLE_2);

                SummonAdds();
                m_submerged = true;
                m_phaseTimer = 60000;
            }
        }
        else
            m_phaseTimer -= diff;

        if (m_checkTimer < diff)
        {
             if (!me->isInCombat())
                 return;

             if (m_rotating)
                 DoCast(me, SPELL_SPOUT_VIS);
                    
             DoZoneInCombat(80.0f);
             m_checkTimer = 2500;
        }
        else
            m_checkTimer -= diff;

        if (m_submerged || m_rotating || !UpdateVictim())
            return;
        
        if (m_rotateTimer < diff)
        {
            me->SetReactState(REACT_PASSIVE);
            
            m_immunemap.clear();
            me->MonsterTextEmote(EMOTE_SPOUT, 0, true);
            me->GetMotionMaster()->MoveRotate(20000, RAND(ROTATE_DIRECTION_LEFT, ROTATE_DIRECTION_RIGHT));

            DoCast(me, SPELL_SPOUT_VIS);

            m_rotating = true;
            m_rotateTimer = 40000;
            m_whirlTimer = 2000;
        }
        else
            m_rotateTimer -= diff;

        if (m_whirlTimer < diff)
        {
            AddSpellToCast(me, SPELL_WHIRL);
            m_whirlTimer = urand(15000, 30000);
        }
        else
            m_whirlTimer -= diff;

        if (m_geyserTimer < diff)
        {
            if (Unit *pTarget = SelectUnit(SELECT_TARGET_RANDOM, 0, 100.0f, true, me->getVictimGUID()))
            {
                AddSpellToCast(pTarget, SPELL_GEYSER);
                m_geyserTimer = urand(20000, 30000);
            }
        }
        else
            m_geyserTimer -= diff;

        CastNextSpellIfAnyAndReady();
        DoMeleeAttackIfReady();
    }
 };

struct TRINITY_DLL_DECL mob_coilfang_ambusherAI : public Scripted_NoMovementAI
{
    mob_coilfang_ambusherAI(Creature *c) : Scripted_NoMovementAI(c)
    {
    }

    uint32 MultiShotTimer;
    uint32 ShootBowTimer;

    void Reset()
    {
        MultiShotTimer = 10000;
        ShootBowTimer = 4000;
    }

    void EnterCombat(Unit *who)
    {
    }

    void MoveInLineOfSight(Unit *who)
    {
    }

    void UpdateAI(const uint32 diff)
    {
        if(MultiShotTimer < diff)
        {
        }
        else
            MultiShotTimer -= diff;

        if(ShootBowTimer < diff)
        {
        }
        else
            ShootBowTimer -= diff;
    }
};

CreatureAI* GetAI_mob_coilfang_ambusher(Creature* pCreature)
{
    return new mob_coilfang_ambusherAI (pCreature);
}

CreatureAI* GetAI_boss_the_lurker_below(Creature* pCreature)
{
    return new boss_the_lurker_belowAI (pCreature);
}

void AddSC_boss_the_lurker_below()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name="boss_the_lurker_below";
    newscript->GetAI = &GetAI_boss_the_lurker_below;
    newscript->RegisterSelf();

    /*newscript = new Script;
    newscript->Name="mob_coilfang_guardian";
    newscript->GetAI = &GetAI_mob_coilfang_guardian;
    newscript->RegisterSelf();*/

    newscript = new Script;
    newscript->Name="mob_coilfang_ambusher";
    newscript->GetAI = &GetAI_mob_coilfang_ambusher;
    newscript->RegisterSelf();
}


