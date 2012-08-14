/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
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

#include "OutdoorPvPNA.h"
#include "WorldPacket.h"
#include "../World.h"
#include "../ObjectMgr.h"
#include "../Object.h"
#include "../Creature.h"
#include "../GameObject.h"
#include "../Player.h"

OutdoorPvPNA::OutdoorPvPNA() : OutdoorPvP(),
    m_zoneMapState(WORLD_STATE_NA_HALAA_NEUTRAL),
    m_zoneWorldState(0),
    m_zoneOwner(TEAM_NONE),
    m_guardsLeft(0)
{
}

void OutdoorPvPNA::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    if (m_zoneOwner != TEAM_NONE)
    {
        FillInitialWorldState(data, count, m_zoneWorldState, WORLD_STATE_ADD);

        // map states
        for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
            FillInitialWorldState(data, count, m_roostWorldState[i], WORLD_STATE_ADD);
    }

    FillInitialWorldState(data, count, m_zoneMapState, WORLD_STATE_ADD);
    FillInitialWorldState(data, count, WORLD_STATE_NA_GUARDS_MAX, MAX_NA_GUARDS);
    FillInitialWorldState(data, count, WORLD_STATE_NA_GUARDS_LEFT, m_guardsLeft);
}

void OutdoorPvPNA::SendRemoveWorldStates(Player* player)
{
    player->SendUpdateWorldState(m_zoneWorldState, WORLD_STATE_REMOVE);
    player->SendUpdateWorldState(m_zoneMapState, WORLD_STATE_REMOVE);

    for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
        player->SendUpdateWorldState(m_roostWorldState[i], WORLD_STATE_REMOVE);
}

void OutdoorPvPNA::HandlePlayerEnterZone(Player* player, bool isMainZone)
{
    OutdoorPvP::HandlePlayerEnterZone(player, isMainZone);

    // remove the buff from the player first because there are some issues at relog
    player->RemoveAurasDueToSpell(SPELL_STRENGTH_HALAANI);

    // buff the player if same team is controlling the zone
    if (player->GetTeam() == m_zoneOwner)
        player->CastSpell(player, SPELL_STRENGTH_HALAANI, true);
}

void OutdoorPvPNA::HandlePlayerLeaveZone(Player* player, bool isMainZone)
{
    // remove the buff from the player
    player->RemoveAurasDueToSpell(SPELL_STRENGTH_HALAANI);

    OutdoorPvP::HandlePlayerLeaveZone(player, isMainZone);
}

void OutdoorPvPNA::HandleObjectiveComplete(uint32 eventId, std::list<Player*> players, Team team)
{
    if (eventId == EVENT_HALAA_BANNER_WIN_ALLIANCE || eventId == EVENT_HALAA_BANNER_WIN_HORDE)
    {
        for (std::list<Player*>::iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            if ((*itr) && (*itr)->GetTeam() == team)
                (*itr)->KilledMonsterCredit(NPC_HALAA_COMBATANT);
        }
    }
}

// Cast player spell on opponent kill
void OutdoorPvPNA::HandlePlayerKillInsideArea(Player* player, Unit* victim)
{
    if (GameObject* capturePoint = player->GetMap()->GetGameObject(m_capturePoint))
    {
        // check capture point range
        GameObjectInfo const* info = capturePoint->GetGOInfo();
        if (info && player->IsWithinDistInMap(capturePoint, info->capturePoint.radius))
        {
            // check capture point team
            if (player->GetTeam() == m_zoneOwner)
                player->CastSpell(player, player->GetTeam() == ALLIANCE ? SPELL_NAGRAND_TOKEN_ALLIANCE : SPELL_NAGRAND_TOKEN_HORDE, true);

            return;
        }
    }
}

void OutdoorPvPNA::OnCreatureCreate(Creature* creature)
{
    switch (creature->GetEntry())
    {
        case NPC_ALLIANCE_HANAANI_GUARD:
        case NPC_RESEARCHER_KARTOS:
        case NPC_QUARTERMASTER_DAVIAN:
        case NPC_MERCHANT_ALDRAAN:
        case NPC_VENDOR_CENDRII:
        case NPC_AMMUNITIONER_BANRO:
            m_allianceSoldiers.push_back(creature->GetObjectGuid());
            if (m_zoneOwner == ALLIANCE)
                return;
            break;
        case NPC_HORDE_HALAANI_GUARD:
        case NPC_RESEARCHER_AMERELDINE:
        case NPC_QUARTERMASTER_NORELIQE:
        case NPC_MERCHANT_COREIEL:
        case NPC_VENDOR_EMBELAR:
        case NPC_AMMUNITIONER_TASALDAN:
            m_hordeSoldiers.push_back(creature->GetObjectGuid());
            if (m_zoneOwner == HORDE)
                return;
            break;

        default:
            return;
    }

    // Despawn creatures on create - will be spawned later in script
    creature->SetRespawnDelay(7 * DAY);
    creature->ForcedDespawn();
}

void OutdoorPvPNA::OnCreatureDeath(Creature* creature)
{
    if (creature->GetEntry() != NPC_HORDE_HALAANI_GUARD && creature->GetEntry() != NPC_ALLIANCE_HANAANI_GUARD)
        return;

    --m_guardsLeft;
    SendUpdateWorldState(WORLD_STATE_NA_GUARDS_LEFT, m_guardsLeft);

    if (m_guardsLeft == 0)
    {
        // make capturable
        UnlockHalaa(creature);

        // update world state
        SendUpdateWorldState(m_zoneMapState, WORLD_STATE_REMOVE);
        m_zoneMapState = m_zoneOwner == ALLIANCE ? WORLD_STATE_NA_HALAA_NEUTRAL_A : WORLD_STATE_NA_HALAA_NEUTRAL_H;
        SendUpdateWorldState(m_zoneMapState, WORLD_STATE_ADD);

        sWorld.SendDefenseMessage(ZONE_ID_NAGRAND, LANG_OPVP_NA_DEFENSELESS);
    }
}

void OutdoorPvPNA::OnCreatureRespawn(Creature* creature)
{
    if (creature->GetEntry() != NPC_HORDE_HALAANI_GUARD && creature->GetEntry() != NPC_ALLIANCE_HANAANI_GUARD)
        return;

    // prevent updating guard counter on owner take over
    if (m_guardsLeft == MAX_NA_GUARDS)
        return;

    if (m_guardsLeft == 0)
    {
        LockHalaa(creature);

        // update world state
        SendUpdateWorldState(m_zoneMapState, WORLD_STATE_REMOVE);
        m_zoneMapState = m_zoneOwner == ALLIANCE ? WORLD_STATE_NA_HALAA_ALLIANCE : WORLD_STATE_NA_HALAA_HORDE;
        SendUpdateWorldState(m_zoneMapState, WORLD_STATE_ADD);
    }

    ++m_guardsLeft;
    SendUpdateWorldState(WORLD_STATE_NA_GUARDS_LEFT, m_guardsLeft);
}

void OutdoorPvPNA::OnGameObjectCreate(GameObject* go)
{
    switch (go->GetEntry())
    {
        case GO_HALAA_BANNER:
            m_capturePoint = go->GetObjectGuid();
            go->SetGoArtKit(GetBannerArtKit(m_zoneOwner, CAPTURE_ARTKIT_ALLIANCE, CAPTURE_ARTKIT_HORDE, CAPTURE_ARTKIT_NEUTRAL));
            break;

        case GO_WYVERN_ROOST_ALLIANCE_SOUTH:
            m_allianceRoost[0] = go->GetObjectGuid();
            break;
        case GO_WYVERN_ROOST_ALLIANCE_NORTH:
            m_allianceRoost[1] = go->GetObjectGuid();
            break;
        case GO_WYVERN_ROOST_ALLIANCE_EAST:
            m_allianceRoost[2] = go->GetObjectGuid();
            break;
        case GO_WYVERN_ROOST_ALLIANCE_WEST:
            m_allianceRoost[3] = go->GetObjectGuid();
            break;

        case GO_BOMB_WAGON_HORDE_SOUTH:
            m_hordeWagons[0] = go->GetObjectGuid();
            break;
        case GO_BOMB_WAGON_HORDE_NORTH:
            m_hordeWagons[1] = go->GetObjectGuid();
            break;
        case GO_BOMB_WAGON_HORDE_EAST:
            m_hordeWagons[2] = go->GetObjectGuid();
            break;
        case GO_BOMB_WAGON_HORDE_WEST:
            m_hordeWagons[3] = go->GetObjectGuid();
            break;

        case GO_DESTROYED_ROOST_ALLIANCE_SOUTH:
            m_allianceBrokenRoost[0] = go->GetObjectGuid();
            break;
        case GO_DESTROYED_ROOST_ALLIANCE_NORTH:
            m_allianceBrokenRoost[1] = go->GetObjectGuid();
            break;
        case GO_DESTROYED_ROOST_ALLIANCE_EAST:
            m_allianceBrokenRoost[2] = go->GetObjectGuid();
            break;
        case GO_DESTROYED_ROOST_ALLIANCE_WEST:
            m_allianceBrokenRoost[3] = go->GetObjectGuid();
            break;

        case GO_WYVERN_ROOST_HORDE_SOUTH:
            m_hordeRoost[0] = go->GetObjectGuid();
            break;
        case GO_WYVERN_ROOST_HORDE_NORTH:
            m_hordeRoost[1] = go->GetObjectGuid();
            break;
        case GO_WYVERN_ROOST_HORDE_EAST:
            m_hordeRoost[2] = go->GetObjectGuid();
            break;
        case GO_WYVERN_ROOST_HORDE_WEST:
            m_hordeRoost[3] = go->GetObjectGuid();
            break;

        case GO_BOMB_WAGON_ALLIANCE_SOUTH:
            m_allianceWagons[0] = go->GetObjectGuid();
            break;
        case GO_BOMB_WAGON_ALLIANCE_NORTH:
            m_allianceWagons[1] = go->GetObjectGuid();
            break;
        case GO_BOMB_WAGON_ALLIANCE_EAST:
            m_allianceWagons[2] = go->GetObjectGuid();
            break;
        case GO_BOMB_WAGON_ALLIANCE_WEST:
            m_allianceWagons[3] = go->GetObjectGuid();
            break;

        case GO_DESTROYED_ROOST_HORDE_SOUTH:
            m_hordeBrokenRoost[0] = go->GetObjectGuid();
            break;
        case GO_DESTROYED_ROOST_HORDE_NORTH:
            m_hordeBrokenRoost[1] = go->GetObjectGuid();
            break;
        case GO_DESTROYED_ROOST_HORDE_EAST:
            m_hordeBrokenRoost[2] = go->GetObjectGuid();
            break;
        case GO_DESTROYED_ROOST_HORDE_WEST:
            m_hordeBrokenRoost[3] = go->GetObjectGuid();
            break;
    }
}

void OutdoorPvPNA::UpdateWorldState(uint32 value)
{
    SendUpdateWorldState(m_zoneWorldState, value);
    SendUpdateWorldState(m_zoneMapState, value);

    UpdateWyvernsWorldState(value);
}

void OutdoorPvPNA::UpdateWyvernsWorldState(uint32 value)
{
    for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
        SendUpdateWorldState(m_roostWorldState[i], value);
}

// process the capture events
void OutdoorPvPNA::OnProcessEvent(uint32 eventId, GameObject* go)
{
    // If we are not using the Halaa banner return
    if (go->GetEntry() != GO_HALAA_BANNER)
        return;

    switch (eventId)
    {
        case EVENT_HALAA_BANNER_WIN_ALLIANCE:
            ProcessCaptureEvent(go, ALLIANCE);
            break;
        case EVENT_HALAA_BANNER_WIN_HORDE:
            ProcessCaptureEvent(go, HORDE);
            break;
        case EVENT_HALAA_BANNER_PROGRESS_ALLIANCE:
            SetBannerVisual(go, CAPTURE_ARTKIT_ALLIANCE, CAPTURE_ANIM_ALLIANCE);
            sWorld.SendDefenseMessage(ZONE_ID_NAGRAND, LANG_OPVP_NA_PROGRESS_A);
            break;
        case EVENT_HALAA_BANNER_PROGRESS_HORDE:
            SetBannerVisual(go, CAPTURE_ARTKIT_HORDE, CAPTURE_ANIM_HORDE);
            sWorld.SendDefenseMessage(ZONE_ID_NAGRAND, LANG_OPVP_NA_PROGRESS_H);
            break;
    }
}

void OutdoorPvPNA::ProcessCaptureEvent(GameObject* go, Team team)
{
    BuffTeam(m_zoneOwner, SPELL_STRENGTH_HALAANI, true);

    // update capture point owner
    m_zoneOwner = team;

    // don't rely on OnCreatureRespawn to set guard counter / lock halaa as that would send a world state for each spawned guard
    LockHalaa(go);
    m_guardsLeft = MAX_NA_GUARDS;

    UpdateWorldState(WORLD_STATE_REMOVE);
    RespawnSoldiers(go);
    sObjectMgr.SetGraveYardLinkTeam(GRAVEYARD_ID_HALAA, GRAVEYARD_ZONE_ID_HALAA, m_zoneOwner);

    if (m_zoneOwner == ALLIANCE)
    {
        m_zoneWorldState = WORLD_STATE_NA_GUARDS_ALLIANCE;
        m_zoneMapState = WORLD_STATE_NA_HALAA_ALLIANCE;
    }
    else
    {
        m_zoneWorldState = WORLD_STATE_NA_GUARDS_HORDE;
        m_zoneMapState = WORLD_STATE_NA_HALAA_HORDE;
    }

    HandleFactionObjects(go);
    UpdateWorldState(WORLD_STATE_ADD);

    SendUpdateWorldState(WORLD_STATE_NA_GUARDS_LEFT, m_guardsLeft);

    BuffTeam(m_zoneOwner, SPELL_STRENGTH_HALAANI);
    sWorld.SendDefenseMessage(ZONE_ID_NAGRAND, m_zoneOwner == ALLIANCE ? LANG_OPVP_NA_CAPTURE_A: LANG_OPVP_NA_CAPTURE_H);
}

void OutdoorPvPNA::HandleFactionObjects(const WorldObject* objRef)
{
    if (m_zoneOwner == ALLIANCE)
    {
        for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
        {
            RespawnGO(objRef, m_hordeWagons[i], false);
            RespawnGO(objRef, m_allianceBrokenRoost[i], false);
            RespawnGO(objRef, m_allianceRoost[i], false);
            RespawnGO(objRef, m_allianceWagons[i], false);
            RespawnGO(objRef, m_hordeBrokenRoost[i], true);

            m_roostWorldState[i] = nagrandRoostStatesHordeNeutral[i];
        }
    }
    else
    {
        for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
        {
            RespawnGO(objRef, m_allianceWagons[i], false);
            RespawnGO(objRef, m_hordeBrokenRoost[i], false);
            RespawnGO(objRef, m_hordeRoost[i], false);
            RespawnGO(objRef, m_hordeWagons[i], false);
            RespawnGO(objRef, m_allianceBrokenRoost[i], true);

            m_roostWorldState[i] = nagrandRoostStatesAllianceNeutral[i];
        }
    }
}

void OutdoorPvPNA::RespawnSoldiers(const WorldObject* objRef)
{
    if (m_zoneOwner == ALLIANCE)
    {
        // despawn all horde vendors
        for (GuidList::const_iterator itr = m_hordeSoldiers.begin(); itr != m_hordeSoldiers.end(); ++itr)
        {
            if (Creature* soldier = objRef->GetMap()->GetCreature(*itr))
            {
                // reset respawn time
                soldier->SetRespawnDelay(7 * DAY);
                soldier->ForcedDespawn();
            }
        }

        // spawn all alliance soldiers and vendors
        for (GuidList::const_iterator itr = m_allianceSoldiers.begin(); itr != m_allianceSoldiers.end(); ++itr)
        {
            if (Creature* soldier = objRef->GetMap()->GetCreature(*itr))
            {
                // lower respawn time
                soldier->SetRespawnDelay(HOUR);
                soldier->Respawn();
            }
        }
    }
    else
    {
        // despawn all alliance vendors
        for (GuidList::const_iterator itr = m_allianceSoldiers.begin(); itr != m_allianceSoldiers.end(); ++itr)
        {
            if (Creature* soldier = objRef->GetMap()->GetCreature(*itr))
            {
                // reset respawn time
                soldier->SetRespawnDelay(7 * DAY);
                soldier->ForcedDespawn();
            }
        }

        // spawn all horde soldiers and vendors
        for (GuidList::const_iterator itr = m_hordeSoldiers.begin(); itr != m_hordeSoldiers.end(); ++itr)
        {
            if (Creature* soldier = objRef->GetMap()->GetCreature(*itr))
            {
                // lower respawn time
                soldier->SetRespawnDelay(HOUR);
                soldier->Respawn();
            }
        }
    }
}

bool OutdoorPvPNA::HandleObjectUse(Player* player, GameObject* go)
{
    UpdateWyvernsWorldState(WORLD_STATE_REMOVE);

    if (player->GetTeam() == ALLIANCE)
    {
        for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
        {
            if (go->GetEntry() == nagrandWagonsAlliance[i])
            {
                m_roostWorldState[i] = nagrandRoostStatesHordeNeutral[i];
                RespawnGO(go, m_hordeRoost[i], false);
                RespawnGO(go, m_hordeBrokenRoost[i], true);
            }
            else if (go->GetEntry() == nagrandRoostsBrokenAlliance[i])
            {
                m_roostWorldState[i] = nagrandRoostStatesAlliance[i];
                RespawnGO(go, m_hordeWagons[i], true);
                RespawnGO(go, m_allianceRoost[i], true, true);
            }
            else if (go->GetEntry() == nagrandRoostsAlliance[i])
            {
                // mark player as pvp
                player->UpdatePvP(true, true);
                player->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP);
            }
        }
    }
    else if (player->GetTeam() == HORDE)
    {
        for (uint8 i = 0; i < MAX_NA_ROOSTS; ++i)
        {
            if (go->GetEntry() == nagrandWagonsHorde[i])
            {
                m_roostWorldState[i] = nagrandRoostStatesAllianceNeutral[i];
                RespawnGO(go, m_allianceRoost[i], false);
                RespawnGO(go, m_allianceBrokenRoost[i], true);
            }
            else if (go->GetEntry() == nagrandRoostsBrokenHorde[i])
            {
                m_roostWorldState[i] = nagrandRoostStatesHorde[i];
                RespawnGO(go, m_allianceWagons[i], true);
                RespawnGO(go, m_hordeRoost[i], true, true);
            }
            else if (go->GetEntry() == nagrandRoostsHorde[i])
            {
                // mark player as pvp
                player->UpdatePvP(true, true);
                player->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP);
            }
        }
    }

    UpdateWyvernsWorldState(WORLD_STATE_ADD);

    return false;
}

void OutdoorPvPNA::RespawnGO(const WorldObject* objRef, ObjectGuid goGuid, bool respawn, bool resetFlag)
{
    if (GameObject* banner = objRef->GetMap()->GetGameObject(goGuid))
    {
        if (respawn)
        {
            banner->SetRespawnTime(7 * DAY);
            banner->Refresh();

            // Set no-despawn flag for the Roosts
            if (resetFlag)
                banner->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NODESPAWN);
        }
        else if (banner->isSpawned())
        {
            if (banner->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_NODESPAWN))
                banner->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NODESPAWN);

            banner->SetLootState(GO_JUST_DEACTIVATED);
        }
    }
}

void OutdoorPvPNA::LockHalaa(const WorldObject* objRef)
{
    if (GameObject* go = objRef->GetMap()->GetGameObject(m_capturePoint))
        go->SetLootState(GO_JUST_DEACTIVATED);

    sOutdoorPvPMgr.SetCapturePointSlider(m_capturePoint, m_zoneOwner == ALLIANCE ? CAPTURE_SLIDER_ALLIANCE_LOCKED : CAPTURE_SLIDER_HORDE_LOCKED);
}

void OutdoorPvPNA::UnlockHalaa(const WorldObject* objRef)
{
    if (GameObject* go = objRef->GetMap()->GetGameObject(m_capturePoint))
        go->SetCapturePointSlider(m_zoneOwner == ALLIANCE ? CAPTURE_SLIDER_ALLIANCE : CAPTURE_SLIDER_HORDE);
        // no banner visual update needed because it already has the correct one
    else
        // if grid is unloaded, resetting the slider value is enough
        sOutdoorPvPMgr.SetCapturePointSlider(m_capturePoint, m_zoneOwner == ALLIANCE ? CAPTURE_SLIDER_ALLIANCE : CAPTURE_SLIDER_HORDE);
}
