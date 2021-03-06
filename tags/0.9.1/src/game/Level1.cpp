/* 
 * Copyright (C) 2005,2006,2007 MaNGOS <http://www.mangosproject.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Opcodes.h"
#include "Chat.h"
#include "Log.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "Language.h"
#include "RedZoneDistrict.h"
#ifdef _DEBUG_VMAPS
#include "VMapFactory.h"
#endif

bool ChatHandler::HandleSayCommand(const char* args)
{
    uint64 guid = m_session->GetPlayer()->GetSelection();
    Creature* pCreature = ObjectAccessor::GetCreature(*m_session->GetPlayer(), guid);

    if(!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    pCreature->Say(args, LANG_UNIVERSAL, 0);

    return true;
}

bool ChatHandler::HandleYellCommand(const char* args)
{
    uint64 guid = m_session->GetPlayer()->GetSelection();
    Creature* pCreature = ObjectAccessor::GetCreature(*m_session->GetPlayer(), guid);

    if(!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    pCreature->Yell(args, LANG_UNIVERSAL, 0);

    return true;
}

//show text emote by creature in chat
bool ChatHandler::HandleTextEmoteCommand(const char* args)
{
    if(!*args) return false;

    Creature* pCreature = getSelectedCreature();

    if(!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    pCreature->TextEmote(args, 0);

    return true;
}

// make npc whisper to player
bool ChatHandler::HandleNpcWhisperCommand(const char* args)
{
    char* receiver = strtok((char*)args, " ");
    char* text = strtok(NULL, "");

    uint64 guid = m_session->GetPlayer()->GetSelection();  
    Creature* pCreature = ObjectAccessor::GetCreature(*m_session->GetPlayer(), guid);

    if(!pCreature || !receiver || !text)
    {
        return false;
    }

    pCreature->Whisper(atol(receiver), text);
    
    return true;
}

// global announce
bool ChatHandler::HandleAnnounceCommand(const char* args)
{
    if(!*args)
        return false;

    std::string str = LANG_SYSTEMMESSAGE;
    str += args;
    sWorld.SendWorldText(str.c_str(), NULL);

    #ifdef _DEBUG_VMAPS
    VMAP::IVMapManager *vMapManager = VMAP::VMapFactory::createOrGetVMapManager();
    float x,y,z;
    m_session->GetPlayer()->GetPosition(x,y,z);
    char buffer[100];
    sprintf(buffer, "pos %f,%f,%f",x,y,z);
    vMapManager->processCommand(buffer);
    #endif
    return true;
}

//notification player at the screen
bool ChatHandler::HandleNotifyCommand(const char* args)
{
    if(!*args)
        return false;

    std::string str = LANG_GLOBAL_NOTIFY;
    str += args;

    WorldPacket data(SMSG_NOTIFICATION, (str.size()+1));
    data << str;
    sWorld.SendGlobalMessage(&data);

    return true;
}

//Enable\Dissable GM Mode
bool ChatHandler::HandleGMmodeCommand(const char* args)
{
    if(!*args) return false;

    std::string argstr = (char*)args;

    if (argstr == "on")
    {
        m_session->GetPlayer()->SetGameMaster(true);
        m_session->SendNotification("GM mode is ON");
        #ifdef _DEBUG_VMAPS
        VMAP::IVMapManager *vMapManager = VMAP::VMapFactory::createOrGetVMapManager();
        vMapManager->processCommand("stoplog");
        #endif
        return true;
    }

    if (argstr == "off")
    {
        m_session->GetPlayer()->SetGameMaster(false);
        m_session->SendNotification("GM mode is OFF");
        #ifdef _DEBUG_VMAPS
        VMAP::IVMapManager *vMapManager = VMAP::VMapFactory::createOrGetVMapManager();
        vMapManager->processCommand("startlog");
        #endif
        return true;
    }

    SendSysMessage(LANG_BAD_VALUE);
    return false;
}

//Enable\Dissable Invisible mode
bool ChatHandler::HandleVisibleCommand(const char* args)
{
    if (!*args)
    {
        SendSysMessage(LANG_USE_BOL);
        PSendSysMessage(LANG_YOU_ARE, m_session->GetPlayer()->isGMVisible() ?  LANG_VISIBLE : LANG_INVISIBLE);
        return true;
    }

    std::string argstr = (char*)args;

    if (argstr == "on")
    {
        m_session->GetPlayer()->SetGMVisible(true);
        m_session->SendNotification( LANG_INVISIBLE_VISIBLE );
        return true;
    }

    if (argstr == "off")
    {
        m_session->SendNotification( LANG_INVISIBLE_INVISIBLE );
        m_session->GetPlayer()->SetGMVisible(false);
        return true;
    }

    SendSysMessage(LANG_BAD_VALUE);
    return true;
}

bool ChatHandler::HandleGPSCommand(const char* args)
{
    WorldObject *obj = getSelectedUnit();

    if(!obj)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        return true;
    }

    CellPair cell_val = MaNGOS::ComputeCellPair(obj->GetPositionX(), obj->GetPositionY());
    Cell cell = RedZone::GetZone(cell_val);

    uint32 zone_id = obj->GetZoneId();
    uint32 area_id = obj->GetAreaId();

    MapEntry const* mapEntry = sMapStore.LookupEntry(obj->GetMapId());
    AreaTableEntry const* zoneEntry = GetAreaEntryByAreaID(zone_id);
    AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(area_id);

    float zone_x = obj->GetPositionX();
    float zone_y = obj->GetPositionY();

    Map2ZoneCoordinates(zone_x,zone_y,zone_id);

    PSendSysMultilineMessage(LANG_MAP_POSITION,
        obj->GetMapId(), (mapEntry ? mapEntry->name[sWorld.GetDBClang()] : "<unknown>" ), 
        zone_id, (zoneEntry ? zoneEntry->area_name[sWorld.GetDBClang()] : "<unknown>" ), 
        area_id, (areaEntry ? areaEntry->area_name[sWorld.GetDBClang()] : "<unknown>" ), 
        obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ(),
        obj->GetOrientation(),cell.GridX(), cell.GridY(), cell.CellX(), cell.CellY(),obj->GetInstanceId(),
        zone_x,zone_y );

    sLog.outDebug("Player %s GPS call %s %u " LANG_MAP_POSITION, m_session->GetPlayer()->GetName(),
        (obj->GetTypeId() == TYPEID_PLAYER ? "player" : "creature"), obj->GetGUIDLow(),
        obj->GetMapId(), (mapEntry ? mapEntry->name[sWorld.GetDBClang()] : "<unknown>" ), 
        zone_id, (zoneEntry ? zoneEntry->area_name[sWorld.GetDBClang()] : "<unknown>" ), 
        area_id, (areaEntry ? areaEntry->area_name[sWorld.GetDBClang()] : "<unknown>" ), 
        obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ(),
        obj->GetOrientation(), cell.GridX(), cell.GridY(), cell.CellX(), cell.CellY(),obj->GetInstanceId(),
        zone_x,zone_y );

    return true;
}

//Summon Player
bool ChatHandler::HandleNamegoCommand(const char* args)
{
    if(!*args)
        return false;

    std::string name = args;
    normalizePlayerName(name);
    //WorldDatabase.escape_string(name);                          // prevent SQL injection - normal name don't must changed by this call

    Player *chr = objmgr.GetPlayer(name.c_str());
    if (chr)
    {
        if(chr->IsBeingTeleported()==true)
        {
            PSendSysMessage(LANG_IS_TELEPORTED, chr->GetName());
            return true;
        }

        if(chr->isInFlight())
        {
            PSendSysMessage(LANG_CHAR_IN_FLIGHT,chr->GetName());
            return true;
        }

        Map* pMap = MapManager::Instance().GetMap(m_session->GetPlayer()->GetMapId(),m_session->GetPlayer());

        if(pMap->Instanceable())
        {
            Map* cMap = MapManager::Instance().GetMap(chr->GetMapId(),chr);
            if( cMap->Instanceable() && cMap->GetInstanceId() != pMap->GetInstanceId() )
            {
                // cannot summon from instance to instance
                PSendSysMessage(LANG_CANNOT_SUMMON_TO_INST,chr->GetName());
                return true;
            }

            // we are in instance, and can summon only player in our group with us as lead
            if ( !m_session->GetPlayer()->GetGroup() || !chr->GetGroup() ||
                (chr->GetGroup()->GetLeaderGUID() != m_session->GetPlayer()->GetGUID()) ||
                (m_session->GetPlayer()->GetGroup()->GetLeaderGUID() != m_session->GetPlayer()->GetGUID()) )
                // the last check is a bit excessive, but let it be, just in case
            {
                PSendSysMessage(LANG_CANNOT_SUMMON_TO_INST,chr->GetName());
                return true;
            }
        }

        PSendSysMessage(LANG_SUMMONING, chr->GetName(),"");

        if (m_session->GetPlayer()->IsVisibleGloballyFor(chr))
        {
            char buf0[256];
            snprintf((char*)buf0,256,LANG_SUMMONED_BY, m_session->GetPlayer()->GetName());

            WorldPacket data;
            FillSystemMessageData(&data, m_session, buf0);
            chr->GetSession()->SendPacket( &data );
        }

        chr->SaveRecallPosition();

        // before GM
        float x,y,z;
        m_session->GetPlayer()->GetClosePoint(x,y,z,chr->GetObjectSize());
        chr->TeleportTo(m_session->GetPlayer()->GetMapId(),x,y,z,chr->GetOrientation());
    }
    else if (uint64 guid = objmgr.GetPlayerGUIDByName(name))
    {
        PSendSysMessage(LANG_SUMMONING, name.c_str()," (offline)");

        // in point where GM stay
        Player::SavePositionInDB(m_session->GetPlayer()->GetMapId(),
            m_session->GetPlayer()->GetPositionX(),
            m_session->GetPlayer()->GetPositionY(),
            m_session->GetPlayer()->GetPositionZ(),
            m_session->GetPlayer()->GetOrientation(),
            m_session->GetPlayer()->GetZoneId(),
            guid);
    }
    else
        PSendSysMessage(LANG_NO_PLAYER, args);

    return true;
}

//Teleport to Player
bool ChatHandler::HandleGonameCommand(const char* args)
{
    if(!*args)
        return false;

    Player* _player = m_session->GetPlayer();

    if(_player->isInFlight())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        return true;
    }

    std::string name = args;
    normalizePlayerName(name);
    //WorldDatabase.escape_string(name);                          // prevent SQL injection - normal name don't must changed by this call

    Player *chr = objmgr.GetPlayer(name.c_str());
    if (chr)
    {
        Map* cMap = MapManager::Instance().GetMap(chr->GetMapId(),chr);
        if(cMap->Instanceable())
        {
            Map* pMap = MapManager::Instance().GetMap(_player->GetMapId(),_player);
            if( pMap->Instanceable() && cMap->GetInstanceId() != pMap->GetInstanceId() )
            {
                // cannot go from instance to instance
                PSendSysMessage(LANG_CANNOT_GO_INST_INST,chr->GetName());
                return true;
            }

            // we have to go to instance, and can go to player only if:
            //   1) we are in his group (either as leader or as member)
            //   2) we are not bound to any group and have GM mode on
            if (_player->GetGroup())
            {
                // we are in group, we can go only if we are in the player group
                if (_player->GetGroup() != chr->GetGroup())
                {
                    PSendSysMessage(LANG_CANNOT_GO_TO_INST_PARTY,chr->GetName());
                    return true;
                }
            }
            else
            {
                // we are not in group, let's verify our GM mode
                if (!_player->isGameMaster())
                {
                    PSendSysMessage(LANG_CANNOT_GO_TO_INST_GM,chr->GetName());
                    return true;
                }
            }

            // bind us to the players instance
            BoundInstancesMap::iterator i = chr->m_BoundInstances.find(chr->GetMapId());
                                                            // error, the player has no instance bound!!!
            if (i == chr->m_BoundInstances.end()) return true;
            _player->m_BoundInstances[chr->GetMapId()] = std::pair < uint32, uint32 >(i->second.first, i->second.second);
            _player->SetInstanceId(chr->GetInstanceId());
        }

        PSendSysMessage(LANG_APPEARING_AT, chr->GetName());

        if (_player->IsVisibleGloballyFor(chr))
        {
            char buf0[256];
            sprintf((char*)buf0,LANG_APPEARING_TO, _player->GetName());

            WorldPacket data;
            FillSystemMessageData(&data, m_session, buf0);
            chr->GetSession()->SendPacket(&data);
        }

        _player->SaveRecallPosition();

        // to point to see at target with same orientation
        float x,y,z;
        chr->GetContactPoint(m_session->GetPlayer(),x,y,z);

        _player->TeleportTo(chr->GetMapId(), x, y, z, _player->GetAngle( chr ), true, true, true);
        return true;
    }

    if (uint64 guid = objmgr.GetPlayerGUIDByName(name))
    {
        PSendSysMessage(LANG_APPEARING_AT, name.c_str());

        // to point where player stay (if loaded)
        float x,y,z,o;
        uint32 map;
        if(Player::LoadPositionFromDB(map,x,y,z,o,guid))
        {
            _player->SaveRecallPosition();
            _player->TeleportTo(map, x, y, z,_player->GetOrientation());
            return true;
        }
    }

    PSendSysMessage(LANG_NO_PLAYER, args);

    return true;
}

// Teleport player to last position
bool ChatHandler::HandleRecallCommand(const char* args)
{
    Player* chr = NULL;

    if(!*args)
    {
        chr = getSelectedPlayer();

        if(!chr)
        {
            chr = m_session->GetPlayer();

            if(chr->isInFlight())
            {
                SendSysMessage(LANG_TARGET_IN_FLIGHT);
                return true;
            }
        }
    }
    else
    {
        std::string name = args;
        normalizePlayerName(name);
        //WorldDatabase.escape_string(name);                      // prevent SQL injection - normal name don't must changed by this call

        chr = objmgr.GetPlayer(name.c_str());

        if(!chr)
        {
            PSendSysMessage(LANG_NO_PLAYER, args);
            return true;
        }
    }

    if(chr->IsBeingTeleported())
    {
        PSendSysMessage(LANG_IS_TELEPORTED, chr->GetName());
        return true;
    }

    if(chr->isInFlight())
    {
        PSendSysMessage(LANG_CHAR_IN_FLIGHT, chr->GetName());
        return true;
    }

    chr->TeleportTo(chr->m_recallMap, chr->m_recallX, chr->m_recallY, chr->m_recallZ, chr->m_recallO);
    return true;
}

//Edit Player KnownTitles
bool ChatHandler::HandleModifyKnownTitlesCommand(const char* args)
{
    if(!*args)
        return false;

    uint32 titles = atoi((char*)args);

    Player *chr = getSelectedPlayer();
    if (!chr)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    chr->SetUInt32Value(PLAYER__FIELD_KNOWN_TITLES, titles);
    SendSysMessage(LANG_DONE);

    return true;
}

//Edit Player HP
bool ChatHandler::HandleModifyHPCommand(const char* args)
{
    if(!*args)
        return false;

    //    char* pHp = strtok((char*)args, " ");
    //    if (!pHp)
    //        return false;

    //    char* pHpMax = strtok(NULL, " ");
    //    if (!pHpMax)
    //        return false;

    //    int32 hpm = atoi(pHpMax);
    //    int32 hp = atoi(pHp);

    int32 hp = atoi((char*)args);
    int32 hpm = atoi((char*)args);

    if (hp <= 0 || hpm <= 0 || hpm < hp)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    PSendSysMessage(LANG_YOU_CHANGE_HP, hp, hpm, chr->GetName());

    WorldPacket data;
    char buf[256];
    sprintf((char*)buf,LANG_YOURS_HP_CHANGED, m_session->GetPlayer()->GetName(), hp, hpm);
    FillSystemMessageData(&data, m_session, buf);

    chr->GetSession()->SendPacket(&data);

    chr->SetMaxHealth( hpm );
    chr->SetHealth( hp );

    return true;
}

//Edit Player Mana
bool ChatHandler::HandleModifyManaCommand(const char* args)
{
    if(!*args)
        return false;

    // char* pmana = strtok((char*)args, " ");
    // if (!pmana)
    //     return false;

    // char* pmanaMax = strtok(NULL, " ");
    // if (!pmanaMax)
    //     return false;

    // int32 manam = atoi(pmanaMax);
    // int32 mana = atoi(pmana);
    int32 mana = atoi((char*)args);
    int32 manam = atoi((char*)args);

    if (mana <= 0 || manam <= 0 || manam < mana)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    PSendSysMessage(LANG_YOU_CHANGE_MANA, mana, manam, chr->GetName());

    WorldPacket data;
    char buf[256];
    sprintf((char*)buf,LANG_YOURS_MANA_CHANGED, m_session->GetPlayer()->GetName(), mana, manam);
    FillSystemMessageData(&data, m_session, buf);

    chr->GetSession()->SendPacket(&data);

    chr->SetMaxPower(POWER_MANA,manam );
    chr->SetPower(POWER_MANA, mana );

    return true;
}

//Edit Player Energy
bool ChatHandler::HandleModifyEnergyCommand(const char* args)
{
    if(!*args)
        return false;

    // char* pmana = strtok((char*)args, " ");
    // if (!pmana)
    //     return false;

    // char* pmanaMax = strtok(NULL, " ");
    // if (!pmanaMax)
    //     return false;

    // int32 manam = atoi(pmanaMax);
    // int32 mana = atoi(pmana);

    int32 energy = atoi((char*)args)*10;
    int32 energym = atoi((char*)args)*10;

    if (energy <= 0 || energym <= 0 || energym < energy)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    Player *chr = getSelectedPlayer();
    if (!chr)
    {
        PSendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    PSendSysMessage(LANG_YOU_CHANGE_ENERGY, energy/10, energym/10, chr->GetName());

    WorldPacket data;
    char buf[256];
    sprintf((char*)buf,LANG_YOURS_ENERGY_CHANGED, m_session->GetPlayer()->GetName(), energy/10, energym/10);
    FillSystemMessageData(&data, m_session, buf);
    chr->GetSession()->SendPacket(&data);

    chr->SetMaxPower(POWER_ENERGY,energym );
    chr->SetPower(POWER_ENERGY, energy );

    sLog.outDetail(LANG_CURRENT_ENERGY,chr->GetMaxPower(POWER_ENERGY));

    return true;
}

//Edit Player Rage
bool ChatHandler::HandleModifyRageCommand(const char* args)
{
    if(!*args)
        return false;

    // char* pmana = strtok((char*)args, " ");
    // if (!pmana)
    //     return false;

    // char* pmanaMax = strtok(NULL, " ");
    // if (!pmanaMax)
    //     return false;

    // int32 manam = atoi(pmanaMax);
    // int32 mana = atoi(pmana);

    int32 rage = atoi((char*)args)*10;
    int32 ragem = atoi((char*)args)*10;

    if (rage <= 0 || ragem <= 0 || ragem < rage)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    PSendSysMessage(LANG_YOU_CHANGE_RAGE, rage/10, ragem/10, chr->GetName());

    WorldPacket data;
    char buf[256];
    sprintf((char*)buf,LANG_YOURS_RAGE_CHANGED, m_session->GetPlayer()->GetName(), rage/10, ragem/10);
    FillSystemMessageData(&data, m_session, buf);

    chr->GetSession()->SendPacket(&data);

    chr->SetMaxPower(POWER_RAGE,ragem );
    chr->SetPower(POWER_RAGE, rage );

    return true;
}

//Edit Player Faction
bool ChatHandler::HandleModifyFactionCommand(const char* args)
{
    if(!*args)
        return false;

    char* pfactionid = strtok((char*)args, " ");

    Creature* chr = getSelectedCreature();
    if(!chr)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    if(!pfactionid)
    {
        if(chr)
        {
            uint32 factionid = chr->getFaction();
            uint32 flag      = chr->GetUInt32Value(UNIT_FIELD_FLAGS);
            uint32 npcflag   = chr->GetUInt32Value(UNIT_NPC_FLAGS);
            uint32 dyflag    = chr->GetUInt32Value(UNIT_DYNAMIC_FLAGS);
            PSendSysMessage(LANG_CURRENT_FACTION,chr->GetGUIDLow(),factionid,flag,npcflag,dyflag);
        }
        return true;
    }

    if( !chr )
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    uint32 factionid = atoi(pfactionid);
    uint32 flag;

    char *pflag = strtok(NULL, " ");
    if (!pflag)
        flag = chr->GetUInt32Value(UNIT_FIELD_FLAGS);
    else
        flag = atoi(pflag);

    char* pnpcflag = strtok(NULL, " ");

    uint32 npcflag;
    if(!pnpcflag)
        npcflag   = chr->GetUInt32Value(UNIT_NPC_FLAGS);
    else
        npcflag = atoi(pnpcflag);

    char* pdyflag = strtok(NULL, " ");

    uint32  dyflag;
    if(!pdyflag)
        dyflag   = chr->GetUInt32Value(UNIT_DYNAMIC_FLAGS);
    else
        dyflag = atoi(pdyflag);

    if(!sFactionTemplateStore.LookupEntry(factionid))
    {
        PSendSysMessage(LANG_WRONG_FACTION, factionid);
        return true;
    }

    PSendSysMessage(LANG_YOU_CHANGE_FACTION, chr->GetGUIDLow(),factionid,flag,npcflag,dyflag);

    //sprintf((char*)buf,"%s changed your Faction to %i.", m_session->GetPlayer()->GetName(), factionid);
    //FillSystemMessageData(&data, m_session, buf);

    //chr->GetSession()->SendPacket(&data);

    chr->setFaction(factionid);
    chr->SetUInt32Value(UNIT_FIELD_FLAGS,flag);
    chr->SetUInt32Value(UNIT_NPC_FLAGS,npcflag);
    chr->SetUInt32Value(UNIT_DYNAMIC_FLAGS,dyflag);

    return true;
}

//Edit Player Spell
bool ChatHandler::HandleModifySpellCommand(const char* args)
{
    if(!*args) return false;
    char* pspellflatid = strtok((char*)args, " ");
    if (!pspellflatid)
        return false;

    char* pop = strtok(NULL, " ");
    if (!pop)
        return false;

    char* pval = strtok(NULL, " ");
    if (!pval)
        return false;

    uint16 mark;

    char* pmark = strtok(NULL, " ");

    uint8 spellflatid = atoi(pspellflatid);
    uint8 op   = atoi(pop);
    uint16 val = atoi(pval);
    if(!pmark)
        mark = 65535;
    else
        mark = atoi(pmark);

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    PSendSysMessage(LANG_YOU_CHANGE_SPELLFLATID, spellflatid, val, mark, chr->GetName());

    WorldPacket data;
    char buf[256];
    sprintf((char*)buf,LANG_YOURS_SPELLFLATID_CHANGED, m_session->GetPlayer()->GetName(), spellflatid, val, mark);
    FillSystemMessageData(&data, m_session, buf);

    chr->GetSession()->SendPacket(&data);

    data.Initialize(SMSG_SET_FLAT_SPELL_MODIFIER, (1+1+2+2));
    data << uint8(spellflatid);
    data << uint8(op);
    data << uint16(val);
    data << uint16(mark);
    chr->GetSession()->SendPacket(&data);

    return true;
}

//Edit Player TP
bool ChatHandler::HandleModifyTalentCommand (const char* args)
{
    if (!*args)
        return false;

    int tp = atoi((char*)args);
    if (tp>0)
    {
        Player* player = getSelectedPlayer();
        if(!player)
        {
            SendSysMessage(LANG_NO_CHAR_SELECTED);
            return true;
        }
        player->SetFreeTalentPoints(tp);
        return true;
    }
    return false;
}

//Enable On\OFF all taxi paths
bool ChatHandler::HandleTaxiCheatCommand(const char* args)
{
    if (!*args)
        return false;
    std::string argstr = (char*)args;

    Player *chr = getSelectedPlayer();
    if (!chr)
    {
        chr=m_session->GetPlayer();
    }

    if (argstr == "on")
    {
        chr->SetTaxiCheater(true);
        PSendSysMessage(LANG_YOU_GIVE_TAXIS, chr->GetName());

        if(chr != m_session->GetPlayer())
        {
            WorldPacket data;
            char buf[256];
            sprintf((char*)buf,LANG_YOURS_TAXIS_ADDED, m_session->GetPlayer()->GetName());
            FillSystemMessageData(&data, m_session, buf);
            chr->GetSession()->SendPacket(&data);
        }
        return true;
    }

    if (argstr == "off")
    {
        chr->SetTaxiCheater(false);
        PSendSysMessage(LANG_YOU_REMOVE_TAXIS, chr->GetName());

        if(chr != m_session->GetPlayer())
        {
            WorldPacket data;
            char buf[256];
            sprintf((char*)buf,LANG_YOURS_TAXIS_REMOVED, m_session->GetPlayer()->GetName());
            FillSystemMessageData(&data, m_session, buf);
            chr->GetSession()->SendPacket(&data);
        }
        return true;
    }

    return false;
}

//Edit Player Aspeed
bool ChatHandler::HandleModifyASpeedCommand(const char* args)
{
    if (!*args)
        return false;

    float ASpeed = (float)atof((char*)args);

    if (ASpeed > 10 || ASpeed < 0.1)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    if(chr->isInFlight())
    {
        PSendSysMessage(LANG_CHAR_IN_FLIGHT,chr->GetName());
        return true;
    }

    PSendSysMessage(LANG_YOU_CHANGE_ASPEED, ASpeed, chr->GetName());

    WorldPacket data;
    char buf[256];
    sprintf((char*)buf,LANG_YOURS_ASPEED_CHANGED, m_session->GetPlayer()->GetName(), ASpeed);
    FillSystemMessageData(&data, m_session, buf);

    chr->GetSession()->SendPacket(&data);

    chr->SetSpeed(MOVE_WALK,    ASpeed,true);
    chr->SetSpeed(MOVE_RUN,     ASpeed,true);
    chr->SetSpeed(MOVE_WALKBACK,ASpeed,true);
    chr->SetSpeed(MOVE_SWIM,    ASpeed,true);
    chr->SetSpeed(MOVE_SWIMBACK,ASpeed,true);
    //chr->SetSpeed(MOVE_TURN,    ASpeed,true);
    chr->SetSpeed(MOVE_FLY,     ASpeed,true);
    chr->SetSpeed(MOVE_MOUNTED, ASpeed,true);
    return true;
}

//Edit Player Speed
bool ChatHandler::HandleModifySpeedCommand(const char* args)
{
    if (!*args)
        return false;

    float Speed = (float)atof((char*)args);

    if (Speed > 10 || Speed < 0.1)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    if(chr->isInFlight())
    {
        PSendSysMessage(LANG_CHAR_IN_FLIGHT,chr->GetName());
        return true;
    }

    PSendSysMessage(LANG_YOU_CHANGE_SPEED, Speed, chr->GetName());

    WorldPacket data;
    char buf[256];
    sprintf((char*)buf,LANG_YOURS_SPEED_CHANGED, m_session->GetPlayer()->GetName(), Speed);
    FillSystemMessageData(&data, m_session, buf);

    chr->GetSession()->SendPacket(&data);

    chr->SetSpeed(MOVE_RUN,Speed,true);

    return true;
}

//Edit Player Swim Speed
bool ChatHandler::HandleModifySwimCommand(const char* args)
{
    if (!*args)
        return false;

    float Swim = (float)atof((char*)args);

    if (Swim > 10.0f || Swim < 0.01f)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    if(chr->isInFlight())
    {
        PSendSysMessage(LANG_CHAR_IN_FLIGHT,chr->GetName());
        return true;
    }

    PSendSysMessage(LANG_YOU_CHANGE_SWIM_SPEED, Swim, chr->GetName());

    WorldPacket data;
    char buf[256];
    sprintf((char*)buf,LANG_YOURS_SWIM_SPEED_CHANGED, m_session->GetPlayer()->GetName(), Swim);
    FillSystemMessageData(&data, m_session, buf);

    chr->GetSession()->SendPacket(&data);

    chr->SetSpeed(MOVE_SWIM,Swim,true);

    return true;
}

//Edit Player Walk Speed
bool ChatHandler::HandleModifyBWalkCommand(const char* args)
{
    if (!*args)
        return false;

    float BSpeed = (float)atof((char*)args);

    if (BSpeed > 10.0f || BSpeed < 0.1f)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    if(chr->isInFlight())
    {
        PSendSysMessage(LANG_CHAR_IN_FLIGHT,chr->GetName());
        return true;
    }

    PSendSysMessage(LANG_YOU_CHANGE_BACK_SPEED, BSpeed, chr->GetName());

    WorldPacket data;
    char buf[256];
    sprintf((char*)buf,LANG_YOURS_BACK_SPEED_CHANGED, m_session->GetPlayer()->GetName(), BSpeed);
    FillSystemMessageData(&data, m_session, buf);

    chr->GetSession()->SendPacket(&data);

    chr->SetSpeed(MOVE_WALKBACK,BSpeed,true);

    return true;
}

//Edit Player Fly
bool ChatHandler::HandleModifyFlyCommand(const char* args)
{
    if (!*args)
        return false;

    float FSpeed = (float)atof((char*)args);

    if (FSpeed > 10.0f || FSpeed < 0.1f)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    PSendSysMessage(LANG_YOU_CHANGE_FLY_SPEED, FSpeed, chr->GetName());

    WorldPacket data;
    char buf[256];
    sprintf((char*)buf,LANG_YOURS_FLY_SPEED_CHANGED, m_session->GetPlayer()->GetName(), FSpeed);
    FillSystemMessageData(&data, m_session, buf);

    chr->GetSession()->SendPacket(&data);

    chr->SetSpeed(MOVE_FLY,FSpeed,true);

    return true;
}

//Edit Player Scale
bool ChatHandler::HandleModifyScaleCommand(const char* args)
{
    if (!*args)
        return false;

    float Scale = (float)atof((char*)args);
    if (Scale > 3.0f || Scale <= 0.0f)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    PSendSysMessage(LANG_YOU_CHANGE_SIZE, Scale, chr->GetName());

    WorldPacket data;
    char buf[256];
    sprintf((char*)buf,LANG_YOURS_SIZE_CHANGED, m_session->GetPlayer()->GetName(), Scale);
    FillSystemMessageData(&data, m_session, buf);

    chr->GetSession()->SendPacket(&data);

    chr->SetFloatValue(OBJECT_FIELD_SCALE_X, Scale);

    return true;
}

//Enable Player mount
bool ChatHandler::HandleModifyMountCommand(const char* args)
{
    if(!*args)
        return false;

    uint16 mId = 1147;
    float speed = (float)15;
    uint32 num = 0;

    num = atoi((char*)args);
    switch(num)
    {
        case 1:
            mId=14340;
            break;
        case 2:
            mId=4806;
            break;
        case 3:
            mId=6471;
            break;
        case 4:
            mId=12345;
            break;
        case 5:
            mId=6472;
            break;
        case 6:
            mId=6473;
            break;
        case 7:
            mId=10670;
            break;
        case 8:
            mId=10719;
            break;
        case 9:
            mId=10671;
            break;
        case 10:
            mId=10672;
            break;
        case 11:
            mId=10720;
            break;
        case 12:
            mId=14349;
            break;
        case 13:
            mId=11641;
            break;
        case 14:
            mId=12244;
            break;
        case 15:
            mId=12242;
            break;
        case 16:
            mId=14578;
            break;
        case 17:
            mId=14579;
            break;
        case 18:
            mId=14349;
            break;
        case 19:
            mId=12245;
            break;
        case 20:
            mId=14335;
            break;
        case 21:
            mId=207;
            break;
        case 22:
            mId=2328;
            break;
        case 23:
            mId=2327;
            break;
        case 24:
            mId=2326;
            break;
        case 25:
            mId=14573;
            break;
        case 26:
            mId=14574;
            break;
        case 27:
            mId=14575;
            break;
        case 28:
            mId=604;
            break;
        case 29:
            mId=1166;
            break;
        case 30:
            mId=2402;
            break;
        case 31:
            mId=2410;
            break;
        case 32:
            mId=2409;
            break;
        case 33:
            mId=2408;
            break;
        case 34:
            mId=2405;
            break;
        case 35:
            mId=14337;
            break;
        case 36:
            mId=6569;
            break;
        case 37:
            mId=10661;
            break;
        case 38:
            mId=10666;
            break;
        case 39:
            mId=9473;
            break;
        case 40:
            mId=9476;
            break;
        case 41:
            mId=9474;
            break;
        case 42:
            mId=14374;
            break;
        case 43:
            mId=14376;
            break;
        case 44:
            mId=14377;
            break;
        case 45:
            mId=2404;
            break;
        case 46:
            mId=2784;
            break;
        case 47:
            mId=2787;
            break;
        case 48:
            mId=2785;
            break;
        case 49:
            mId=2736;
            break;
        case 50:
            mId=2786;
            break;
        case 51:
            mId=14347;
            break;
        case 52:
            mId=14346;
            break;
        case 53:
            mId=14576;
            break;
        case 54:
            mId=9695;
            break;
        case 55:
            mId=9991;
            break;
        case 56:
            mId=6448;
            break;
        case 57:
            mId=6444;
            break;
        case 58:
            mId=6080;
            break;
        case 59:
            mId=6447;
            break;
        case 60:
            mId=4805;
            break;
        case 61:
            mId=9714;
            break;
        case 62:
            mId=6448;
            break;
        case 63:
            mId=6442;
            break;
        case 64:
            mId=14632;
            break;
        case 65:
            mId=14332;
            break;
        case 66:
            mId=14331;
            break;
        case 67:
            mId=8469;
            break;
        case 68:
            mId=2830;
            break;
        case 69:
            mId=2346;
            break;
        default:
            SendSysMessage(LANG_NO_MOUNT);
            return true;
    }

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    PSendSysMessage(LANG_YOU_GIVE_MOUNT, chr->GetName());

    WorldPacket data;
    char buf[256];
    sprintf((char*)buf,LANG_MOUNT_GIVED, m_session->GetPlayer()->GetName());
    FillSystemMessageData(&data, m_session, buf);

    chr->GetSession()->SendPacket(&data);

    chr->SetUInt32Value( UNIT_FIELD_FLAGS , 0x001000 );
    chr->Mount(mId);

    data.Initialize( SMSG_FORCE_RUN_SPEED_CHANGE, (8+4+1+4) );
    data.append(chr->GetPackGUID());
    data << (uint32)0;
    data << (uint8)0;                                       //new 2.1.0
    data << float(speed);
    chr->SendMessageToSet( &data, true );

    data.Initialize( SMSG_FORCE_SWIM_SPEED_CHANGE, (8+4+4) );
    data.append(chr->GetPackGUID());
    data << (uint32)0;
    data << float(speed);
    chr->SendMessageToSet( &data, true );

    return true;
}

//Edit Player money
bool ChatHandler::HandleModifyMoneyCommand(const char* args)
{
    if (!*args)
        return false;

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    int32 addmoney = atoi((char*)args);

    uint32 moneyuser = m_session->GetPlayer()->GetMoney();

    if(addmoney < 0)
    {
        int32 newmoney = moneyuser + addmoney;

        sLog.outDetail(LANG_CURRENT_MONEY, moneyuser, addmoney, newmoney);
        if(newmoney <= 0 )
        {
            PSendSysMessage(LANG_YOU_TAKE_ALL_MONEY, chr->GetName());

            WorldPacket data;
            char buf[256];
            sprintf((char*)buf,LANG_YOURS_ALL_MONEY_GONE, m_session->GetPlayer()->GetName());
            FillSystemMessageData(&data, m_session, buf);
            chr->GetSession()->SendPacket(&data);

            chr->SetMoney(0);
        }
        else
        {
            PSendSysMessage(LANG_YOU_TAKE_MONEY, abs(addmoney), chr->GetName());

            WorldPacket data;
            char buf[256];
            sprintf((char*)buf,LANG_YOURS_MONEY_TAKEN, m_session->GetPlayer()->GetName(), abs(addmoney));
            FillSystemMessageData(&data, m_session, buf);
            chr->GetSession()->SendPacket(&data);

            chr->SetMoney( newmoney );
        }
    }
    else
    {
        PSendSysMessage(LANG_YOU_GIVE_MONEY, addmoney, chr->GetName());

        WorldPacket data;
        char buf[256];
        sprintf((char*)buf,LANG_YOURS_MONEY_GIVEN, m_session->GetPlayer()->GetName(), addmoney);
        FillSystemMessageData(&data, m_session, buf);
        chr->GetSession()->SendPacket(&data);

        chr->ModifyMoney( addmoney );
    }

    sLog.outDetail(LANG_NEW_MONEY, moneyuser, addmoney, chr->GetMoney() );

    return true;
}

//Edit Player field
bool ChatHandler::HandleModifyBitCommand(const char* args)
{
    if( !*args )
        return false;

    Player *chr = getSelectedPlayer();
    if (chr == NULL)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    char* pField = strtok((char*)args, " ");
    if (!pField)
        return false;

    char* pBit = strtok(NULL, " ");
    if (!pBit)
        return false;

    uint16 field = atoi(pField);
    uint32 bit   = atoi(pBit);

    if (field < 1 || field >= PLAYER_END)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    if (bit < 1 || bit > 32)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    if ( chr->HasFlag( field, (1<<(bit-1)) ) )
    {
        chr->RemoveFlag( field, (1<<(bit-1)) );
        PSendSysMessage(LANG_REMOVE_BIT, bit, field);
    }
    else
    {
        chr->SetFlag( field, (1<<(bit-1)) );
        PSendSysMessage(LANG_SET_BIT, bit, field);
    }

    return true;
}

//Teleport by game_tele entry
bool ChatHandler::HandleModifyHonorCommand (const char* args)
{
    if (!*args)
        return false;

    Player *target = getSelectedPlayer();
    if(!target)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        return true;
    }

    uint32 amount = (uint32)atoi(args);

    if (amount < 0 || amount > 999999)
    {
        SendSysMessage(LANG_BAD_VALUE);
        return true;
    }

    target->SetHonorPoints(amount);

    PSendSysMessage(LANG_COMMAND_MODIFY_HONOR, target->GetName(), amount);

    return true;
}

bool ChatHandler::HandleTeleCommand(const char * args)
{
    if(!*args)
        return false;

    Player* _player = m_session->GetPlayer();

    if(_player->isInFlight())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        return true;
    }

    if(_player->InBattleGround())
    {
        SendSysMessage(LANG_YOU_IN_BATTLEGROUND);
        return true;
    }

    char* cId = extractKeyFromLink((char*)args,"Htele");    // string or [name] Shift-click form |color|Htele:name|h[name]|h|r
    if(!cId)
        return false;

    std::string name = cId;
    WorldDatabase.escape_string(name);

    QueryResult *result = WorldDatabase.PQuery("SELECT `position_x`,`position_y`,`position_z`,`orientation`,`map` FROM `game_tele` WHERE `name` = '%s'",name.c_str());
    if (!result)
    {
        SendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
        return true;
    }
    Field *fields = result->Fetch();
    float x = fields[0].GetFloat();
    float y = fields[1].GetFloat();
    float z = fields[2].GetFloat();
    float ort = fields[3].GetFloat();
    int mapid = fields[4].GetUInt16();
    delete result;

    if(!MapManager::IsValidMapCoord(mapid,x,y))
    {
        PSendSysMessage(LANG_INVALID_TARGET_COORD,x,y,mapid);
        return true;
    }

    _player->SaveRecallPosition();

    _player->TeleportTo(mapid, x, y, z, ort);
    return true;
}

bool ChatHandler::HandleLookupAreaCommand(const char* args)
{
    if(!*args)
        return false;

    std::string namepart = args;
    uint32 counter = 0;                                     // Counter for figure out that we found smth.

    // Search in AreaTable.dbc
    for (uint32 areaflag = 0; areaflag < sAreaStore.GetNumRows(); ++areaflag)
    {
        AreaTableEntry const *areaEntry = sAreaStore.LookupEntry(areaflag);
        if(areaEntry)
        {
            // name - is first name field from dbc (English localized)
            std::string name = areaEntry->area_name[sWorld.GetDBClang()];

            // converting SpellName to lower case
            std::transform( name.begin(), name.end(), name.begin(), ::tolower );

            // converting string that we try to find to lower case
            std::transform( namepart.begin(), namepart.end(), namepart.begin(), ::tolower );

            if (name.find(namepart) != std::string::npos)
            {
                // send area in "id - [name]" format
                std::ostringstream ss;
                ss << areaEntry->ID << " - |cffffffff|Harea:" << areaEntry->ID << "|h[" << areaEntry->area_name[sWorld.GetDBClang()] << "]|h|r";

                SendSysMessage(ss.str().c_str());

                ++counter;
            }
        }
    }
    if (counter == 0)                                       // if counter == 0 then we found nth
        SendSysMessage(LANG_COMMAND_NOAREAFOUND);
    return true;
}

//Find tele in game_tele order by name
bool ChatHandler::HandleLookupTeleCommand(const char * args)
{
    if(!*args)
    {
        SendSysMessage(LANG_COMMAND_TELE_PARAMETER);
        return true;
    }
    char const* str = strtok((char*)args, " ");
    if(!str)
        return false;

    std::string namepart = str;
    WorldDatabase.escape_string(namepart);
    QueryResult *result = WorldDatabase.PQuery("SELECT `name` FROM `game_tele` WHERE `name` LIKE '%%%s%%'",namepart.c_str());
    if (!result)
    {
        SendSysMessage(LANG_COMMAND_TELE_NOREQUEST);
        return true;
    }
    std::string reply;
    for (uint64 i=0; i < result->GetRowCount(); i++)
    {
        Field *fields = result->Fetch();
        reply += "  |cffffffff|Htele:";
        reply += fields[0].GetCppString();
        reply += "|h[";
        reply += fields[0].GetCppString();
        reply += "]|h|r\n";
        result->NextRow();
    }
    delete result;

    if(reply.empty())
        SendSysMessage(LANG_COMMAND_TELE_NOLOCATION);
    else
    {
        reply = LANG_COMMAND_TELE_LOCATION + reply;
        SendSysMultilineMessage(reply.c_str());
    }
    return true;
}

//Enable\Dissable accept whispers (for GM)
bool ChatHandler::HandleWhispersCommand(const char* args)
{
    if(!*args)
    {
        PSendSysMessage(LANG_COMMAND_WHISPERACCEPTING, m_session->GetPlayer()->isAcceptWhispers() ?  LANG_ON : LANG_OFF);
        return true;
    }

    std::string argstr = (char*)args;
    // ticket on
    if (argstr == "on")
    {
        m_session->GetPlayer()->SetAcceptWhispers(true);
        SendSysMessage(LANG_COMMAND_WHISPERON);
        return true;
    }

    // ticket off
    if (argstr == "off")
    {
        m_session->GetPlayer()->SetAcceptWhispers(false);
        SendSysMessage(LANG_COMMAND_WHISPEROFF);
        return true;
    }

    return false;
}

//Play sound
bool ChatHandler::HandlePlaySoundCommand(const char* args)
{
    // USAGE: .playsound #soundid
    // #soundid - ID decimal number from SoundEntries.dbc (1 column)
    // this file have about 5000 sounds.
    // In this realisation only caller can hear this sound.
    if( *args )
    {
        int dwSoundId = atoi((char*)args);
        if( dwSoundId >= 0 )
        {
            WorldPacket data(SMSG_PLAY_OBJECT_SOUND,4+8);
            data << uint32(dwSoundId) << m_session->GetPlayer()->GetGUID();
            m_session->SendPacket(&data);

            sLog.outDebug("Player %s use command .playsound with #soundid=%u", m_session->GetPlayer()->GetName(), dwSoundId);
            PSendSysMessage(LANG_YOU_HEAR_SOUND, dwSoundId);
            return true;
        }
    }

    SendSysMessage(LANG_BAD_VALUE);
    return false;
}

//Save all players in the world
bool ChatHandler::HandleSaveAllCommand(const char* args)
{
    ObjectAccessor::Instance().SaveAllPlayers();
    SendSysMessage(LANG_PLAYERS_SAVED);
    return true;
}

//Send mail by command
bool ChatHandler::HandleSendMailCommand(const char* args)
{
    if(!*args)
        return false;

    char* pName = strtok((char*)args, " ");
    char* msgSubject = strtok(NULL, " ");
    char* msgText = strtok(NULL, "");

    if (!msgText)
        return false;

    // pName, msgSubject, msgText isn't NUL after prev. check

    std::string name    = pName;
    std::string subject = msgSubject;
    std::string text    = msgText;

    normalizePlayerName(name);

    uint32 receiver_guid = objmgr.GetPlayerGUIDByName(name);

    if(!receiver_guid)
        return false;

    uint32 mailId = objmgr.GenerateMailID();
    uint32 sender_guid = m_session->GetPlayer()->GetGUID();
    time_t dtime = time(NULL);
    time_t etime = dtime + (30 * DAY);
    uint32 messagetype = MAIL_GM;
    uint32 itemTextId = 0;
    if (!text.empty())
    {
        itemTextId = objmgr.CreateItemText( text );
    }

    Player *receiver = objmgr.GetPlayer((uint32) receiver_guid);
    if(receiver)
        receiver->CreateMail(mailId,messagetype,sender_guid,subject.c_str(),itemTextId,0,0,(uint64)etime,(uint64)dtime,0,0,0,0);

    CharacterDatabase.escape_string(subject);
    CharacterDatabase.PExecute("INSERT INTO `mail` (`id`,`messageType`,`sender`,`receiver`,`subject`,`itemTextId`,`item_guid`,`item_template`,`expire_time`,`deliver_time`,`money`,`cod`,`checked`) "
        "VALUES ('%u', '%u', '%u', '%u', '%s', '%u', '0', '0', '" I64FMTD "','" I64FMTD "', '0', '0', '%d')",
        mailId, messagetype, sender_guid, receiver_guid, subject.c_str(), itemTextId, (uint64)etime, (uint64)dtime, NOT_READ);

    PSendSysMessage(LANG_MAIL_SENT, name.c_str());
    return true;
}

// teleport player to given game_tele.entry
bool ChatHandler::HandleNameTeleCommand(const char * args)
{
    if(!*args)
        return false;

    char* pName = strtok((char*)args, " ");

    if(!pName)
        return false;

    char* tail = strtok(NULL, "");
    if(!tail)
        return false;

    char* cId = extractKeyFromLink((char*)tail,"Htele");    // string or [name] Shift-click form |color|Htele:name|h[name]|h|r
    if(!cId)
        return false;

    std::string location = cId;

    std::string name = pName;

    normalizePlayerName(name);

    WorldDatabase.escape_string(location);
    QueryResult *result = WorldDatabase.PQuery("SELECT `position_x`,`position_y`,`position_z`,`orientation`,`map` FROM `game_tele` WHERE `name` = '%s'",location.c_str());
    if (!result)
    {
        SendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
        return true;
    }

    Field *fields = result->Fetch();
    float x = fields[0].GetFloat();
    float y = fields[1].GetFloat();
    float z = fields[2].GetFloat();
    float ort = fields[3].GetFloat();
    int mapid = fields[4].GetUInt16();
    delete result;

    if(!MapManager::IsValidMapCoord(mapid,x,y))
    {
        PSendSysMessage(LANG_INVALID_TARGET_COORD,x,y,mapid);
        return true;
    }

    Player *chr = objmgr.GetPlayer(name.c_str());
    if (chr)
    {

        if(chr->IsBeingTeleported()==true)
        {
            PSendSysMessage(LANG_IS_TELEPORTED, chr->GetName());
            return true;
        }

        if(chr->isInFlight())
        {
            PSendSysMessage(LANG_CHAR_IN_FLIGHT,chr->GetName());
            return true;
        }

        PSendSysMessage(LANG_TELEPORTING_TO, chr->GetName(),"", location.c_str());

        if (m_session->GetPlayer()->IsVisibleGloballyFor(chr))
        {
            WorldPacket data;
            char buf0[256];
            snprintf((char*)buf0,256,LANG_TELEPORTED_TO_BY, m_session->GetPlayer()->GetName());
            FillSystemMessageData(&data, m_session, buf0);
            chr->GetSession()->SendPacket( &data );
        }

        chr->SaveRecallPosition();

        chr->TeleportTo(mapid,x,y,z,chr->GetOrientation());
    }
    else if (uint64 guid = objmgr.GetPlayerGUIDByName(name.c_str()))
    {
        PSendSysMessage(LANG_TELEPORTING_TO, name.c_str(), LANG_OFFLINE, location.c_str());
        Player::SavePositionInDB(mapid,x,y,z,ort,MapManager::Instance().GetZoneId(mapid,x,y),guid);
    }
    else
        PSendSysMessage(LANG_NO_PLAYER, name.c_str());

    return true;
}

//Teleport group to given game_tele.entry
bool ChatHandler::HandleGroupTeleCommand(const char * args)
{
    if(!*args)
        return false;

    Player *player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        return true;
    }

    char* cId = extractKeyFromLink((char*)args,"Htele");    // string or [name] Shift-click form |color|Htele:name|h[name]|h|r
    if(!cId)
        return false;

    std::string location = cId;

    WorldDatabase.escape_string(location);
    QueryResult *result = WorldDatabase.PQuery("SELECT `position_x`,`position_y`,`position_z`,`orientation`,`map` FROM `game_tele` WHERE `name` = '%s'",location.c_str());
    if (!result)
    {
        SendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
        return true;
    }
    Field *fields = result->Fetch();
    float x = fields[0].GetFloat();
    float y = fields[1].GetFloat();
    float z = fields[2].GetFloat();
    float ort = fields[3].GetFloat();
    int mapid = fields[4].GetUInt16();
    delete result;

    if(!MapManager::IsValidMapCoord(mapid,x,y))
    {
        PSendSysMessage(LANG_INVALID_TARGET_COORD,x,y,mapid);
        return true;
    }

    Group *grp = player->GetGroup();

    if(!grp)
    {
        PSendSysMessage(LANG_NOT_IN_GROUP,player->GetName());
        return true;
    }

    for(GroupReference *itr = grp->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player *pl = itr->getSource();

        if(!pl || !pl->GetSession() )
            continue;

        if(pl->IsBeingTeleported())
        {
            PSendSysMessage(LANG_IS_TELEPORTED, pl->GetName());
            continue;
        }

        if(pl->isInFlight())
        {
            PSendSysMessage(LANG_CHAR_IN_FLIGHT,pl->GetName());
            continue;
        }

        PSendSysMessage(LANG_TELEPORTING_TO, pl->GetName(),"", location.c_str());

        if (m_session->GetPlayer() != pl && m_session->GetPlayer()->IsVisibleGloballyFor(pl))
        {
            WorldPacket data;
            char buf0[256];
            snprintf((char*)buf0,256,LANG_TELEPORTED_TO_BY, m_session->GetPlayer()->GetName());
            FillSystemMessageData(&data, m_session, buf0);
            pl->GetSession()->SendPacket( &data );
        }

        pl->SaveRecallPosition();
        pl->TeleportTo(mapid, x, y, z, ort);
    }

    return true;
}

//Summon group of player
bool ChatHandler::HandleGroupgoCommand(const char* args)
{
    if(!*args)
        return false;

    std::string name = args;
    normalizePlayerName(name);
    //WorldDatabase.escape_string(name);                          // prevent SQL injection - normal name don't must changed by this call

    Player *player = objmgr.GetPlayer(name.c_str());
    if (!player)
    {
        PSendSysMessage(LANG_NO_PLAYER, args);
        return true;
    }

    Group *grp = player->GetGroup();

    if(!grp)
    {
        PSendSysMessage(LANG_NOT_IN_GROUP,player->GetName());
        return true;
    }

    Map* gmMap = MapManager::Instance().GetMap(m_session->GetPlayer()->GetMapId(),m_session->GetPlayer());
    bool to_instance =  gmMap->Instanceable();

    // we are in instance, and can summon only player in our group with us as lead
    if ( to_instance && (
        !m_session->GetPlayer()->GetGroup() || (grp->GetLeaderGUID() != m_session->GetPlayer()->GetGUID()) ||
        (m_session->GetPlayer()->GetGroup()->GetLeaderGUID() != m_session->GetPlayer()->GetGUID()) ) )
        // the last check is a bit excessive, but let it be, just in case
    {
        SendSysMessage(LANG_CANNOT_SUMMON_TO_INST);
        return true;
    }

    for(GroupReference *itr = grp->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player *pl = itr->getSource();

        if(!pl || pl==m_session->GetPlayer() || !pl->GetSession() )
            continue;

        if(pl->IsBeingTeleported()==true)
        {
            PSendSysMessage(LANG_IS_TELEPORTED, pl->GetName());
            return true;
        }

        if(pl->isInFlight())
        {
            PSendSysMessage(LANG_CHAR_IN_FLIGHT,pl->GetName());
            return true;
        }

        if (to_instance)
        {
            Map* plMap = MapManager::Instance().GetMap(pl->GetMapId(),pl);

            if ( plMap->Instanceable() && plMap->GetInstanceId() != gmMap->GetInstanceId() )
            {
                // cannot summon from instance to instance
                PSendSysMessage(LANG_CANNOT_SUMMON_TO_INST,pl->GetName());
                return true;
            }
        }

        PSendSysMessage(LANG_SUMMONING, pl->GetName(),"");

        if (m_session->GetPlayer()->IsVisibleGloballyFor(pl))
        {
            char buf0[256];
            snprintf((char*)buf0,256,LANG_SUMMONED_BY, m_session->GetPlayer()->GetName());

            WorldPacket data;
            FillSystemMessageData(&data, m_session, buf0);
            pl->GetSession()->SendPacket( &data );
        }

        pl->SaveRecallPosition();

        // before GM
        float x,y,z;
        m_session->GetPlayer()->GetClosePoint(x,y,z,pl->GetObjectSize());
        pl->TeleportTo(m_session->GetPlayer()->GetMapId(),x,y,z,pl->GetOrientation());
    }

    return true;
}

//teleport at coordinates
bool ChatHandler::HandleGoXYCommand(const char* args)
{
    if(!*args)
        return false;

    Player* _player = m_session->GetPlayer();

    if(_player->isInFlight())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        return true;
    }

    char* px = strtok((char*)args, " ");
    char* py = strtok(NULL, " ");
    char* pmapid = strtok(NULL, " ");

    if (!px || !py)
        return false;

    float x = (float)atof(px);
    float y = (float)atof(py);
    uint32 mapid;
    if (pmapid)
        mapid = (uint32)atoi(pmapid);
    else mapid = _player->GetMapId();

    if(!MapManager::IsValidMapCoord(mapid,x,y))
    {
        PSendSysMessage(LANG_INVALID_TARGET_COORD,x,y,mapid);
        return true;
    }

    Map const *map = MapManager::Instance().GetBaseMap(mapid);
    float z = std::max(map->GetHeight(x, y, 0), map->GetWaterLevel(x, y));
    _player->SaveRecallPosition();
    _player->TeleportTo(mapid, x, y, z, _player->GetOrientation());

    return true;
}

//teleport at coordinates, including Z
bool ChatHandler::HandleGoXYZCommand(const char* args)
{
    if(!*args)
        return false;

    Player* _player = m_session->GetPlayer();

    if(_player->isInFlight())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        return true;
    }

    char* px = strtok((char*)args, " ");
    char* py = strtok(NULL, " ");
    char* pz = strtok(NULL, " ");
    char* pmapid = strtok(NULL, " ");

    if (!px || !py || !pz)
        return false;

    float x = (float)atof(px);
    float y = (float)atof(py);
    float z = (float)atof(pz);
    uint32 mapid;
    if (pmapid)
        mapid = (uint32)atoi(pmapid);
    else 
        mapid = _player->GetMapId();

    if(!MapManager::IsValidMapCoord(mapid,x,y))
    {
        PSendSysMessage(LANG_INVALID_TARGET_COORD,x,y,mapid);
        return true;
    }

    _player->SaveRecallPosition();
    _player->TeleportTo(mapid, x, y, z, _player->GetOrientation());

    return true;
}

//teleport at coordinates
bool ChatHandler::HandleGoZoneXYCommand(const char* args)
{
    if(!*args)
        return false;

    Player* _player = m_session->GetPlayer();

    if(_player->isInFlight())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        return true;
    }

    char* px = strtok((char*)args, " ");
    char* py = strtok(NULL, " ");
    char* tail = strtok(NULL,"");

    char* cAreaId = extractKeyFromLink(tail,"Harea");    // string or [name] Shift-click form |color|Harea:area_id|h[name]|h|r

    if (!px || !py)
        return false;

    float x = (float)atof(px);
    float y = (float)atof(py);
    uint32 areaid = cAreaId ? (uint32)atoi(cAreaId) : _player->GetZoneId();

    AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(areaid);

    if( x<0 || x>100 || y<0 || y>100 || !areaEntry )
    {
        PSendSysMessage(LANG_INVALID_ZONE_COORD,x,y,areaid);
        return true;
    }

    // update to parent zone if exist (client map show only zones without parents)
    AreaTableEntry const* zoneEntry = areaEntry->zone ? GetAreaEntryByAreaID(areaEntry->zone) : areaEntry;

    Map const *map = MapManager::Instance().GetBaseMap(zoneEntry->mapid);

    if(map->Instanceable())
    {
        PSendSysMessage(LANG_INVALID_ZONE_MAP,areaEntry->ID,areaEntry->area_name[sWorld.GetDBClang()],map->GetId(),map->GetMapName());
        return true;
    }

    Zone2MapCoordinates(x,y,zoneEntry->ID);

    if(!MapManager::IsValidMapCoord(zoneEntry->mapid,x,y))
    {
        PSendSysMessage(LANG_INVALID_TARGET_COORD,x,y,zoneEntry->mapid);
        return true;
    }

    float z = std::max(map->GetHeight(x, y, 0), map->GetWaterLevel(x, y));
    _player->SaveRecallPosition();
    _player->TeleportTo(zoneEntry->mapid, x, y, z, _player->GetOrientation());

    return true;
}

//teleport to grid
bool ChatHandler::HandleGoGridCommand(const char* args)
{
    if(!*args)    return false;
    Player* _player = m_session->GetPlayer();

    if(_player->isInFlight())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        return true;
    }

    char* px = strtok((char*)args, " ");
    char* py = strtok(NULL, " ");
    char* pmapid = strtok(NULL, " ");

    if (!px || !py)
        return false;

    float grid_x = (float)atof(px);
    float grid_y = (float)atof(py);
    uint32 mapid;
    if (pmapid)
        mapid = (uint32)atoi(pmapid);
    else mapid = _player->GetMapId();

    // center of grid
    float x = (grid_x-CENTER_GRID_ID+0.5f)*SIZE_OF_GRIDS;
    float y = (grid_y-CENTER_GRID_ID+0.5f)*SIZE_OF_GRIDS;

    if(!MapManager::IsValidMapCoord(mapid,x,y))
    {
        PSendSysMessage(LANG_INVALID_TARGET_COORD,x,y,mapid);
        return true;
    }

    Map const *map = MapManager::Instance().GetBaseMap(mapid);
    float z = std::max(map->GetHeight(x, y, 0), map->GetWaterLevel(x, y));
    _player->SaveRecallPosition();
    _player->TeleportTo(mapid, x, y, z, _player->GetOrientation());

    return true;
}

//Send notification in channel
bool ChatHandler::HandleSendChannelNotifyCommand(const char* args)
{
    if(!args)
        return false;

    const char *name = "test";
    uint8 code = atoi(args);

    WorldPacket data(SMSG_CHANNEL_NOTIFY, (1+10));
    data << code;                                           // notify type
    data << name;                                           // channel name
    data << uint32(0);
    data << uint32(0);
    m_session->SendPacket(&data);
    return true;
}

//Send notification in chat
bool ChatHandler::HandleSendChatMsgCommand(const char* args)
{
    if(!args)
        return false;

    const char *msg = "testtest";
    uint8 type = atoi(args);

    WorldPacket data(SMSG_MESSAGECHAT, 100);
    data << type;                                           // message type
    data << uint32(0);                                      // lang
    data << m_session->GetPlayer()->GetGUID();              // guid
    data << uint32(0);
    data << uint64(0);
    data << uint32(9);                                      // msg len
    data << msg;                                            // msg
    data << uint8(0);                                       // chat tag
    m_session->SendPacket(&data);
    return true;
}
