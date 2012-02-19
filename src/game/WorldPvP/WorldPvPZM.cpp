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

#include "WorldPvP.h"
#include "WorldPvPZM.h"
#include "../GameObject.h"


WorldPvPZM::WorldPvPZM() : WorldPvP(),
    m_uiGraveyardWorldState(WORLD_STATE_GRAVEYARD_NEUTRAL),
    m_uiAllianceScoutWorldState(WORLD_STATE_ALLIANCE_FLAG_NOT_READY),
    m_uiHordeScoutWorldState(WORLD_STATE_HORDE_FLAG_NOT_READY),

    m_uiGraveyardOwner(TEAM_NONE),
    m_uiTowersAlliance(0),
    m_uiTowersHorde(0)
{
    // init world states
    m_uiBeaconWorldState[0] = WORLD_STATE_TOWER_EAST_NEUTRAL;
    m_uiBeaconWorldState[1] = WORLD_STATE_TOWER_WEST_NEUTRAL;
    m_uiBeaconMapState[0] = WORLD_STATE_BEACON_EAST_NEUTRAL;
    m_uiBeaconMapState[1] = WORLD_STATE_BEACON_WEST_NEUTRAL;

    for (uint8 i = 0; i < MAX_ZM_TOWERS; ++i)
        m_uiBeaconOwner[i] = TEAM_NONE;
}

bool WorldPvPZM::InitWorldPvPArea()
{
    RegisterZone(ZONE_ID_ZANGARMARSH);
    RegisterZone(ZONE_ID_SERPENTSHRINE_CAVERN);
    RegisterZone(ZONE_ID_STREAMVAULT);
    RegisterZone(ZONE_ID_UNDERBOG);
    RegisterZone(ZONE_ID_SLAVE_PENS);

    return true;
}

void WorldPvPZM::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    FillInitialWorldState(data, count, m_uiAllianceScoutWorldState, WORLD_STATE_ADD);
    FillInitialWorldState(data, count, m_uiHordeScoutWorldState, WORLD_STATE_ADD);
    FillInitialWorldState(data, count, m_uiGraveyardWorldState, WORLD_STATE_ADD);

    for (uint8 i = 0; i < MAX_ZM_TOWERS; ++i)
    {
        FillInitialWorldState(data, count, m_uiBeaconWorldState[i], WORLD_STATE_ADD);
        FillInitialWorldState(data, count, m_uiBeaconMapState[i], WORLD_STATE_ADD);
    }
}

void WorldPvPZM::SendRemoveWorldStates(Player* pPlayer)
{
    pPlayer->SendUpdateWorldState(m_uiAllianceScoutWorldState, WORLD_STATE_REMOVE);
    pPlayer->SendUpdateWorldState(m_uiHordeScoutWorldState, WORLD_STATE_REMOVE);
    pPlayer->SendUpdateWorldState(m_uiGraveyardWorldState, WORLD_STATE_REMOVE);

    for (uint8 i = 0; i < MAX_ZM_TOWERS; ++i)
    {
        pPlayer->SendUpdateWorldState(m_uiBeaconWorldState[i], WORLD_STATE_REMOVE);
        pPlayer->SendUpdateWorldState(m_uiBeaconMapState[i], WORLD_STATE_REMOVE);
    }
}

void WorldPvPZM::HandlePlayerEnterZone(Player* pPlayer)
{
    // remove the buff from the player first; Sometimes on relog players still have the aura
    pPlayer->RemoveAurasDueToSpell(SPELL_TWIN_SPIRE_BLESSING);

    // cast buff the the player which enters the zone
    if ((pPlayer->GetTeam() == ALLIANCE ? m_uiTowersAlliance : m_uiTowersHorde) == MAX_ZM_TOWERS)
        pPlayer->CastSpell(pPlayer, SPELL_TWIN_SPIRE_BLESSING, true);

    WorldPvP::HandlePlayerEnterZone(pPlayer);
}

void WorldPvPZM::HandlePlayerLeaveZone(Player* pPlayer)
{
    // remove the buff from the player
    pPlayer->RemoveAurasDueToSpell(SPELL_TWIN_SPIRE_BLESSING);

    WorldPvP::HandlePlayerLeaveZone(pPlayer);
}

void WorldPvPZM::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
        case NPC_ALLIANCE_FIELD_SCOUT:
            m_AllianceScoutGUID = pCreature->GetObjectGuid();
            break;
        case NPC_HORDE_FIELD_SCOUT:
            m_HorderScoutGUID = pCreature->GetObjectGuid();
            break;
        case NPC_PVP_BEAM_RED:
            // East Beam
            if (pCreature->GetPositionY() < 7000.0f)
            {
                m_BeamRedGUID[0] = pCreature->GetObjectGuid();
                if (m_uiBeaconOwner[0] == HORDE)
                    return;
            }
            // Center Beam
            else if (pCreature ->GetPositionY() < 7300.0f)
            {
                m_BeamCenterRedGUID = pCreature->GetObjectGuid();
                if (m_uiGraveyardOwner == HORDE)
                    return;
            }
            // West Beam
            else
            {
                m_BeamRedGUID[1] = pCreature->GetObjectGuid();
                if (m_uiBeaconOwner[1] == HORDE)
                    return;
            }

            pCreature->SetRespawnDelay(7*DAY);
            pCreature->ForcedDespawn();
            break;
        case NPC_PVP_BEAM_BLUE:
            // East Beam
            if (pCreature->GetPositionY() < 7000.0f)
            {
                m_BeamBlueGUID[0] = pCreature->GetObjectGuid();
                if (m_uiBeaconOwner[0] == ALLIANCE)
                    return;
            }
            // Center Beam
            else if (pCreature ->GetPositionY() < 7300.0f)
            {
                m_BeamCenterBlueGUID = pCreature->GetObjectGuid();
                if (m_uiGraveyardOwner == ALLIANCE)
                    return;
            }
            // West Beam
            else
            {
                m_BeamBlueGUID[1] = pCreature->GetObjectGuid();
                if (m_uiBeaconOwner[1] == ALLIANCE)
                    return;
            }

            pCreature->SetRespawnDelay(7 * DAY);
            pCreature->ForcedDespawn();
            break;
    }
}

void WorldPvPZM::OnGameObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
        case GO_ZANGA_BANNER_WEST:
            m_TowerBannerGUID[1] = pGo->GetObjectGuid();
            break;
        case GO_ZANGA_BANNER_EAST:
            m_TowerBannerGUID[0] = pGo->GetObjectGuid();
            break;
        case GO_ZANGA_BANNER_CENTER_ALLIANCE:
            m_TowerBannerCenterAllianceGUID = pGo->GetObjectGuid();
            break;
        case GO_ZANGA_BANNER_CENTER_HORDE:
            m_TowerBannerCenterHordeGUID = pGo->GetObjectGuid();
            break;
        case GO_ZANGA_BANNER_CENTER_NEUTRAL:
            m_TowerBannerCenterNeutralGUID = pGo->GetObjectGuid();
            break;
    }
}

// Cast player spell on opponent kill
void WorldPvPZM::HandlePlayerKillInsideArea(Player* pPlayer, Unit* pVictim)
{
    for (uint8 i = 0; i < MAX_ZM_TOWERS; ++i)
    {
        if (GameObject* capturePoint = pPlayer->GetMap()->GetGameObject(m_TowerBannerGUID[i]))
        {
            // check capture point range
            GameObjectInfo const* info = capturePoint->GetGOInfo();
            if (info && pPlayer->IsWithinDistInMap(capturePoint, info->capturePoint.radius))
            {
                // check capture point faction
                if (pPlayer->GetTeam() == m_uiBeaconOwner[i])
                    pPlayer->CastSpell(pPlayer, pPlayer->GetTeam() == ALLIANCE ? SPELL_ZANGA_TOWER_TOKEN_ALLIANCE : SPELL_ZANGA_TOWER_TOKEN_HORDE, true);

                return;
            }
        }
    }
}

// process the capture events
void WorldPvPZM::ProcessEvent(uint32 uiEventId, GameObject* pGo)
{
    for (uint8 i = 0; i < MAX_ZM_TOWERS; ++i)
    {
        if (aZangaTowers[i] == pGo->GetEntry())
        {
            for (uint8 j = 0; j < 4; ++j)
            {
                if (aZangaTowerEvents[i][j].uiEventEntry == uiEventId)
                {
                    if (aZangaTowerEvents[i][j].faction != m_uiBeaconOwner[i])
                    {
                        ProcessCaptureEvent(pGo, i, aZangaTowerEvents[i][j].faction, aZangaTowerEvents[i][j].uiWorldState, aZangaTowerEvents[i][j].uiMapState);
                        sWorld.SendZoneText(ZONE_ID_ZANGARMARSH, sObjectMgr.GetMangosStringForDBCLocale(aZangaTowerEvents[i][j].uiZoneText));
                    }
                    return;
                }
            }
            return;
        }
    }
}

void WorldPvPZM::ProcessCaptureEvent(GameObject* pGo, uint32 uiTowerId, Team faction, uint32 uiNewWorldState, uint32 uiNewMapState)
{
    if (faction == ALLIANCE)
    {
        SetBeaconArtKit(pGo, m_BeamBlueGUID[uiTowerId], true);
        ++m_uiTowersAlliance;

        if (m_uiTowersAlliance == MAX_ZM_TOWERS)
            PrepareFactionScouts(pGo, ALLIANCE);
    }
    else if (faction == HORDE)
    {
        SetBeaconArtKit(pGo, m_BeamRedGUID[uiTowerId], true);
        ++m_uiTowersHorde;

        if (m_uiTowersHorde == MAX_ZM_TOWERS)
            PrepareFactionScouts(pGo, HORDE);
    }
    else
    {
        if (m_uiBeaconOwner[uiTowerId] == ALLIANCE)
        {
            SetBeaconArtKit(pGo, m_BeamBlueGUID[uiTowerId], false);

            if (m_uiTowersAlliance == MAX_ZM_TOWERS)
                ResetScouts(pGo, ALLIANCE);

            --m_uiTowersAlliance;
        }
        else
        {
            SetBeaconArtKit(pGo, m_BeamRedGUID[uiTowerId], false);

            if (m_uiTowersHorde == MAX_ZM_TOWERS)
                ResetScouts(pGo, HORDE);

            --m_uiTowersHorde;
        }
    }

    // update tower state
    SendUpdateWorldState(m_uiBeaconWorldState[uiTowerId], WORLD_STATE_REMOVE);
    m_uiBeaconWorldState[uiTowerId] = uiNewWorldState;
    SendUpdateWorldState(m_uiBeaconWorldState[uiTowerId], WORLD_STATE_ADD);

    SendUpdateWorldState(m_uiBeaconMapState[uiTowerId], WORLD_STATE_REMOVE);
    m_uiBeaconMapState[uiTowerId] = uiNewMapState;
    SendUpdateWorldState(m_uiBeaconMapState[uiTowerId], WORLD_STATE_ADD);;

    // update tower owner
    m_uiBeaconOwner[uiTowerId] = faction;
}

void WorldPvPZM::PrepareFactionScouts(const WorldObject* objRef, Team faction)
{
    if (faction == ALLIANCE)
    {
        if (Creature* pScout = objRef->GetMap()->GetCreature(m_AllianceScoutGUID))
            pScout->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

        SendUpdateWorldState(m_uiAllianceScoutWorldState, WORLD_STATE_REMOVE);
        m_uiAllianceScoutWorldState = WORLD_STATE_ALLIANCE_FLAG_READY;
        SendUpdateWorldState(m_uiAllianceScoutWorldState, WORLD_STATE_ADD);
    }
    else if (faction == HORDE)
    {
        if (Creature* pScout = objRef->GetMap()->GetCreature(m_HorderScoutGUID))
            pScout->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

        SendUpdateWorldState(m_uiHordeScoutWorldState, WORLD_STATE_REMOVE);
        m_uiHordeScoutWorldState = WORLD_STATE_HORDE_FLAG_READY;
        SendUpdateWorldState(m_uiHordeScoutWorldState, WORLD_STATE_ADD);
    }
}

void WorldPvPZM::ResetScouts(const WorldObject* objRef, Team faction, bool bIncludeWorldStates)
{
    if (faction == ALLIANCE)
    {
        if (Creature* pScout = objRef->GetMap()->GetCreature(m_AllianceScoutGUID))
            pScout->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

        // reset world states only if requested
        if (bIncludeWorldStates)
        {
            SendUpdateWorldState(m_uiAllianceScoutWorldState, WORLD_STATE_REMOVE);
            m_uiAllianceScoutWorldState = WORLD_STATE_ALLIANCE_FLAG_NOT_READY;
            SendUpdateWorldState(m_uiAllianceScoutWorldState, WORLD_STATE_ADD);
        }
    }
    else if (faction == HORDE)
    {
        if (Creature* pScout = objRef->GetMap()->GetCreature(m_HorderScoutGUID))
            pScout->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

        // reset world states only if requested
        if (bIncludeWorldStates)
        {
            SendUpdateWorldState(m_uiHordeScoutWorldState, WORLD_STATE_REMOVE);
            m_uiHordeScoutWorldState = WORLD_STATE_HORDE_FLAG_NOT_READY;
            SendUpdateWorldState(m_uiHordeScoutWorldState, WORLD_STATE_ADD);
        }
    }
}

bool WorldPvPZM::HandleObjectUse(Player* pPlayer, GameObject* pGo)
{
    if (!pPlayer->HasAura(pPlayer->GetTeam() == ALLIANCE ? SPELL_BATTLE_STANDARD_ALLIANCE : SPELL_BATTLE_STANDARD_HORDE))
        return false;

    switch (pGo->GetEntry())
    {
        case GO_ZANGA_BANNER_CENTER_ALLIANCE:
            if (pPlayer->GetTeam() == ALLIANCE)
                return false;

            // change banners
            SetGraveyardArtKit(pGo, m_TowerBannerCenterAllianceGUID, false);
            SetGraveyardArtKit(pGo, m_TowerBannerCenterHordeGUID, true);
            SetBeaconArtKit(pGo, m_BeamCenterBlueGUID, false);
            sWorld.SendZoneText(ZONE_ID_ZANGARMARSH, sObjectMgr.GetMangosStringForDBCLocale(LANG_OPVP_ZM_LOOSE_GY_A));

            // remove buff and graveyard
            DoProcessTeamBuff(ALLIANCE, SPELL_TWIN_SPIRE_BLESSING, true);
            SetGraveyard(ALLIANCE, true);

            SendUpdateWorldState(m_uiGraveyardWorldState, WORLD_STATE_REMOVE);
            m_uiGraveyardWorldState = WORLD_STATE_GRAVEYARD_HORDE;
            SendUpdateWorldState(m_uiGraveyardWorldState, WORLD_STATE_ADD);

            // add the buff and the graveyard
            DoProcessTeamBuff(HORDE, SPELL_TWIN_SPIRE_BLESSING);
            SetGraveyard(HORDE);

            // reset scout and remove player aura
            ResetScouts(pGo, HORDE);
            m_uiGraveyardOwner = HORDE;
            pPlayer->RemoveAurasDueToSpell(SPELL_BATTLE_STANDARD_HORDE);
            SetBeaconArtKit(pGo, m_BeamCenterRedGUID, true);
            sWorld.SendZoneText(ZONE_ID_ZANGARMARSH, sObjectMgr.GetMangosStringForDBCLocale(LANG_OPVP_ZM_CAPTURE_GY_H));

            return true;
        case GO_ZANGA_BANNER_CENTER_HORDE:
            if (pPlayer->GetTeam() == HORDE)
                return false;

            // change banners
            SetGraveyardArtKit(pGo, m_TowerBannerCenterHordeGUID, false);
            SetGraveyardArtKit(pGo, m_TowerBannerCenterAllianceGUID, true);
            SetBeaconArtKit(pGo, m_BeamCenterRedGUID, false);
            sWorld.SendZoneText(ZONE_ID_ZANGARMARSH, sObjectMgr.GetMangosStringForDBCLocale(LANG_OPVP_ZM_LOOSE_GY_H));

            // remove buff and graveyard
            DoProcessTeamBuff(HORDE, SPELL_TWIN_SPIRE_BLESSING, true);
            SetGraveyard(HORDE, true);

            SendUpdateWorldState(m_uiGraveyardWorldState, WORLD_STATE_REMOVE);
            m_uiGraveyardWorldState = WORLD_STATE_GRAVEYARD_ALLIANCE;
            SendUpdateWorldState(m_uiGraveyardWorldState, WORLD_STATE_ADD);

            // add the buff and the graveyard to horde
            DoProcessTeamBuff(ALLIANCE, SPELL_TWIN_SPIRE_BLESSING);
            SetGraveyard(ALLIANCE);

            // reset scout and remove player aura
            ResetScouts(pGo, ALLIANCE);
            m_uiGraveyardOwner = ALLIANCE;
            pPlayer->RemoveAurasDueToSpell(SPELL_BATTLE_STANDARD_ALLIANCE);
            SetBeaconArtKit(pGo, m_BeamCenterBlueGUID, true);
            sWorld.SendZoneText(ZONE_ID_ZANGARMARSH, sObjectMgr.GetMangosStringForDBCLocale(LANG_OPVP_ZM_CAPTURE_GY_A));

            return true;
        case GO_ZANGA_BANNER_CENTER_NEUTRAL:

            // remove old world state
            SendUpdateWorldState(m_uiGraveyardWorldState, WORLD_STATE_REMOVE);

            if (pPlayer->GetTeam() == ALLIANCE)
            {
                // change banners
                SetGraveyardArtKit(pGo, m_TowerBannerCenterNeutralGUID, false);
                SetGraveyardArtKit(pGo, m_TowerBannerCenterAllianceGUID, true);

                // add the buff and the graveyard to horde
                m_uiGraveyardWorldState = WORLD_STATE_GRAVEYARD_ALLIANCE;
                DoProcessTeamBuff(ALLIANCE, SPELL_TWIN_SPIRE_BLESSING);
                SetGraveyard(ALLIANCE);

                // reset scout and remove player aura
                ResetScouts(pGo, ALLIANCE);
                m_uiGraveyardOwner = ALLIANCE;
                pPlayer->RemoveAurasDueToSpell(SPELL_BATTLE_STANDARD_ALLIANCE);
                SetBeaconArtKit(pGo, m_BeamCenterBlueGUID, true);
                sWorld.SendZoneText(ZONE_ID_ZANGARMARSH, sObjectMgr.GetMangosStringForDBCLocale(LANG_OPVP_ZM_CAPTURE_GY_H));
            }
            else if (pPlayer->GetTeam() == HORDE)
            {
                // change banners
                SetGraveyardArtKit(pGo, m_TowerBannerCenterNeutralGUID, false);
                SetGraveyardArtKit(pGo, m_TowerBannerCenterHordeGUID, true);

                // add the buff and the graveyard to horde
                m_uiGraveyardWorldState = WORLD_STATE_GRAVEYARD_HORDE;
                DoProcessTeamBuff(HORDE, SPELL_TWIN_SPIRE_BLESSING);
                SetGraveyard(HORDE);

                // reset scout and remove player aura
                ResetScouts(pGo, HORDE);
                m_uiGraveyardOwner = HORDE;
                pPlayer->RemoveAurasDueToSpell(SPELL_BATTLE_STANDARD_HORDE);
                SetBeaconArtKit(pGo, m_BeamCenterRedGUID, true);
                sWorld.SendZoneText(ZONE_ID_ZANGARMARSH, sObjectMgr.GetMangosStringForDBCLocale(LANG_OPVP_ZM_CAPTURE_GY_H));
            }

            // add new world state
            SendUpdateWorldState(m_uiGraveyardWorldState, WORLD_STATE_ADD);
            return true;
    }

    return false;
}

void WorldPvPZM::SetGraveyard(Team faction, bool bRemove)
{
    if (bRemove)
        sObjectMgr.RemoveGraveYardLink(GRAVEYARD_ID_TWIN_SPIRE, GRAVEYARD_ZONE_TWIN_SPIRE, faction, false);
    else
        sObjectMgr.AddGraveYardLink(GRAVEYARD_ID_TWIN_SPIRE, GRAVEYARD_ZONE_TWIN_SPIRE, faction, false);
}

void WorldPvPZM::SetGraveyardArtKit(const WorldObject* objRef, ObjectGuid goGuid, bool bRespawn)
{
    if (GameObject* pBanner = objRef->GetMap()->GetGameObject(goGuid))
    {
        if (bRespawn)
        {
            pBanner->SetRespawnTime(7 * DAY);
            pBanner->Refresh();
        }
        else if (pBanner->isSpawned())
            pBanner->Delete();
    }
}

void WorldPvPZM::SetBeaconArtKit(const WorldObject* objRef, ObjectGuid goGuid, bool bRespawn)
{
    if (Creature* pBeam = objRef->GetMap()->GetCreature(goGuid))
    {
        if (bRespawn)
            pBeam->Respawn();
        else
            pBeam->ForcedDespawn();
    }
}
