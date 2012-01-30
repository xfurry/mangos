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
#include "WorldPvPGH.h"


WorldPvPGH::WorldPvPGH() : WorldPvP(),
    m_uiZoneController(TEAM_NONE)
{
    m_uiTypeId = WORLD_PVP_TYPE_GH;
}

bool WorldPvPGH::InitWorldPvPArea()
{
    RegisterZone(ZONE_ID_GRIZZLY_HILLS);

    return true;
}

void WorldPvPGH::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
        case NPC_WESTFALL_BRIGADE_DEFENDER:
        case NPC_COMMANDER_HOWSER:
            lAllianceSoldiers.push_back(pCreature->GetObjectGuid());
            if (m_uiZoneController == ALLIANCE)
                return;
            break;
        case NPC_BLACKSMITH_JASON_RIGGINS:
        case NPC_STABLE_MASTER_TIM:
        case NPC_VENDOR_ADAMS:
        case NPC_HORSE:
            // check the zone id because the horses can be found in other areas too
            if (pCreature->GetZoneId() == ZONE_ID_GRIZZLY_HILLS)
                lAllianceVendors.push_back(pCreature->GetObjectGuid());
            if (m_uiZoneController == ALLIANCE)
                return;
            break;
        case NPC_CONQUEST_HOLD_DEFENDER:
        case NPC_GENERAL_GORLOK:
            lHordeSoldiers.push_back(pCreature->GetObjectGuid());
            if (m_uiZoneController == HORDE)
                return;
            break;
        case NPC_BLACKSMITH_KOLOTH:
        case NPC_STABLE_MASTER_KOR:
        case NPC_VENDOR_PURKOM:
        case NPC_RIDING_WOLF:
            // check the zone id because the wolfs can be found in other areas too
            if (pCreature->GetZoneId() == ZONE_ID_GRIZZLY_HILLS)
                lHordeVendors.push_back(pCreature->GetObjectGuid());
            if (m_uiZoneController == HORDE)
                return;
            break;

        default:
            return;
    }

    // Despawn creatures on create - will be spawned later in script
    pCreature->SetRespawnDelay(7 * DAY);
    pCreature->ForcedDespawn();
}

void WorldPvPGH::OnGameObjectCreate(GameObject* pGo)
{
    if (pGo->GetEntry() == GO_VENTURE_BAY_LIGHTHOUSE)
    {
        m_TowerBannerLighthouseGuid = pGo->GetObjectGuid();
        pGo->SetGoArtKit(GO_ARTKIT_BANNER_NEUTRAL);
    }
}

// process the capture events
void WorldPvPGH::ProcessEvent(GameObject* pGo, uint32 uiEventId)
{
    // If we are not using the lighthouse return
    if (pGo->GetEntry() != GO_VENTURE_BAY_LIGHTHOUSE)
        return;

    switch (uiEventId)
    {
        case EVENT_LIGHTHOUSE_WIN_ALLIANCE:
            // Spawn the npcs only when the tower is fully controlled
            DoRespawnSoldiers(pGo, ALLIANCE);
            m_uiZoneController = ALLIANCE;
            break;
        case EVENT_LIGHTHOUSE_WIN_HORDE:
            // Spawn the npcs only when the tower is fully controlled
            DoRespawnSoldiers(pGo, HORDE);
            m_uiZoneController = HORDE;
            break;
        case EVENT_LIGHTHOUSE_PROGRESS_ALLIANCE:
            SetBannerArtKit(pGo, GO_ARTKIT_BANNER_ALLIANCE);
            break;
        case EVENT_LIGHTHOUSE_PROGRESS_HORDE:
            SetBannerArtKit(pGo, GO_ARTKIT_BANNER_HORDE);
            break;
        case EVENT_LIGHTHOUSE_NEUTRAL_ALLIANCE:
        case EVENT_LIGHTHOUSE_NEUTRAL_HORDE:
            SetBannerArtKit(pGo, GO_ARTKIT_BANNER_NEUTRAL);
            m_uiZoneController = TEAM_NONE;
            break;
    }
}

void WorldPvPGH::DoRespawnSoldiers(GameObject* pGoReference, Team faction)
{
    if (faction == ALLIANCE)
    {
        // despawn all horde vendors
        for (std::list<ObjectGuid>::const_iterator itr = lHordeVendors.begin(); itr != lHordeVendors.end(); ++itr)
        {
            if (Creature* pSoldier = pGoReference->GetMap()->GetCreature(*itr))
                pSoldier->ForcedDespawn();
        }

        // spawn all alliance soldiers and vendors
        for (std::list<ObjectGuid>::const_iterator itr = lAllianceSoldiers.begin(); itr != lAllianceSoldiers.end(); ++itr)
        {
            if (Creature* pSoldier = pGoReference->GetMap()->GetCreature(*itr))
                pSoldier->Respawn();
        }
        for (std::list<ObjectGuid>::const_iterator itr = lAllianceVendors.begin(); itr != lAllianceVendors.end(); ++itr)
        {
            if (Creature* pSoldier = pGoReference->GetMap()->GetCreature(*itr))
                pSoldier->Respawn();
        }
    }
    else if (faction == HORDE)
    {
        // despawn all alliance vendors
        for (std::list<ObjectGuid>::const_iterator itr = lAllianceVendors.begin(); itr != lAllianceVendors.end(); ++itr)
        {
            if (Creature* pSoldier = pGoReference->GetMap()->GetCreature(*itr))
                pSoldier->ForcedDespawn();
        }

        // spawn all horde soldiers and vendors
        for (std::list<ObjectGuid>::const_iterator itr = lHordeSoldiers.begin(); itr != lHordeSoldiers.end(); ++itr)
        {
            if (Creature* pSoldier = pGoReference->GetMap()->GetCreature(*itr))
                pSoldier->Respawn();
        }
        for (std::list<ObjectGuid>::const_iterator itr = lHordeVendors.begin(); itr != lHordeVendors.end(); ++itr)
        {
            if (Creature* pSoldier = pGoReference->GetMap()->GetCreature(*itr))
                pSoldier->Respawn();
        }
    }
}

void WorldPvPHP::SetBannerArtKit(GameObject* pGo, uint32 uiArtkit)
{
    pGo->SetGoArtKit(uiArtkit);
    pGo->Refresh();
}
