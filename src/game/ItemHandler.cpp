/* 
 * Copyright (C) 2005-2008 MaNGOS <http://www.mangosproject.org/>
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
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "Opcodes.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Item.h"
#include "UpdateData.h"
#include "ObjectAccessor.h"

void WorldSession::HandleSplitItemOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1+1+1+1+1);

    //sLog.outDebug("WORLD: CMSG_SPLIT_ITEM");
    uint8 srcbag, srcslot, dstbag, dstslot, count;

    recv_data >> srcbag >> srcslot >> dstbag >> dstslot >> count;
    //sLog.outDebug("STORAGE: receive srcbag = %u, srcslot = %u, dstbag = %u, dstslot = %u, count = %u", srcbag, srcslot, dstbag, dstslot, count);

    uint16 src = ( (srcbag << 8) | srcslot );
    uint16 dst = ( (dstbag << 8) | dstslot );

    if(src==dst)
        return;

    _player->SplitItem( src, dst, count );
}

void WorldSession::HandleSwapInvItemOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1+1);

    //sLog.outDebug("WORLD: CMSG_SWAP_INV_ITEM");
    uint8 srcslot, dstslot;

    recv_data >> srcslot >> dstslot;
    //sLog.outDebug("STORAGE: receive srcslot = %u, dstslot = %u", srcslot, dstslot);

    // prevent attempt swap same item to current position generated by client at special checting sequence
    if(srcslot==dstslot)
        return;

    uint16 src = ( (INVENTORY_SLOT_BAG_0 << 8) | srcslot );
    uint16 dst = ( (INVENTORY_SLOT_BAG_0 << 8) | dstslot );

    _player->SwapItem( src, dst );
}

void WorldSession::HandleSwapItem( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1+1+1+1);

    //sLog.outDebug("WORLD: CMSG_SWAP_ITEM");
    uint8 dstbag, dstslot, srcbag, srcslot;

    recv_data >> dstbag >> dstslot >> srcbag >> srcslot ;
    //sLog.outDebug("STORAGE: receive srcbag = %u, srcslot = %u, dstbag = %u, dstslot = %u", srcbag, srcslot, dstbag, dstslot);

    uint16 src = ( (srcbag << 8) | srcslot );
    uint16 dst = ( (dstbag << 8) | dstslot );

    // prevent attempt swap same item to current position generated by client at special checting sequence
    if(src==dst)
        return;

    _player->SwapItem( src, dst );
}

void WorldSession::HandleAutoEquipItemOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1+1);

    //sLog.outDebug("WORLD: CMSG_AUTOEQUIP_ITEM");
    uint8 srcbag, srcslot;

    recv_data >> srcbag >> srcslot;
    //sLog.outDebug("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item *pItem  = _player->GetItemByPos( srcbag, srcslot );
    if( pItem )
    {
        uint16 dest;

        ItemPrototype const *pProto = pItem->GetProto();
        bool not_swapable = pProto && pProto->InventoryType == INVTYPE_BAG;

        uint8 msg = _player->CanEquipItem( NULL_SLOT, dest, pItem, !not_swapable );

        if( msg == EQUIP_ERR_OK )
        {
            uint16 src = ( (srcbag << 8) | srcslot );
            if(dest==src)                                   // prevent equip in same slot
                return;

            msg = _player->CanUnequipItem( dest, !not_swapable );
        }

        if( msg == EQUIP_ERR_OK )
        {
            Item *pItem2 = _player->GetItemByPos( dest );
            if( pItem2 )
            {
                uint16 src = ((srcbag << 8) | srcslot);
                uint8 bag = dest >> 8;
                uint8 slot = dest & 255;
                _player->RemoveItem( bag, slot, false );
                _player->RemoveItem( srcbag, srcslot, false );
                if( _player->IsInventoryPos( srcbag, srcslot ) )
                    _player->StoreItem( src, pItem2, true);
                else if( _player->IsBankPos ( srcbag, srcslot ) )
                    _player->BankItem( src, pItem2, true);
                else if( _player->IsEquipmentPos ( srcbag, srcslot ) )
                    _player->EquipItem( src, pItem2, true);
                _player->EquipItem( dest, pItem, true );
                _player->AutoUnequipOffhandIfNeed();
            }
            else
            {
                _player->RemoveItem( srcbag, srcslot, true );
                _player->EquipItem( dest, pItem, true );
                _player->AutoUnequipOffhandIfNeed();
            }
        }
        else
            _player->SendEquipError( msg, pItem, NULL );
    }
}

void WorldSession::HandleDestroyItemOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1+1+1+1+1+1);

    //sLog.outDebug("WORLD: CMSG_DESTROYITEM");
    uint8 bag, slot, count, data1, data2, data3;

    recv_data >> bag >> slot >> count >> data1 >> data2 >> data3;
    //sLog.outDebug("STORAGE: receive bag = %u, slot = %u, count = %u", bag, slot, count);

    uint16 pos = (bag << 8) | slot;

    // prevent drop unequipable items (in combat, for example) and non-empty bags
    if(_player->IsEquipmentPos(pos) || _player->IsBagPos(pos))
    {
        uint8 msg = _player->CanUnequipItem( pos, false );
        if( msg != EQUIP_ERR_OK )
        {
            _player->SendEquipError( msg, _player->GetItemByPos(pos), NULL );
            return;
        }
    }

    Item *pItem  = _player->GetItemByPos( bag, slot );
    if(!pItem)
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL );
        return;
    }

    if(count)
    {
        uint32 i_count = count;
        _player->DestroyItemCount( pItem, i_count, true );
    }
    else
        _player->DestroyItem( bag, slot, true );
}

// Only _static_ data send in this packet !!!
void WorldSession::HandleItemQuerySingleOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data, 4);

    //sLog.outDebug("WORLD: CMSG_ITEM_QUERY_SINGLE");
    uint32 item;
    recv_data >> item;

    sLog.outDetail("STORAGE: Item Query = %u", item);

    ItemPrototype const *pProto = objmgr.GetItemPrototype( item );
    if( pProto )
    {
        std::string Name        = pProto->Name1;
        std::string Description = pProto->Description;

        int loc_idx = GetSessionLocaleIndex();
        if ( loc_idx >= 0 )
        {
            ItemLocale const *il = objmgr.GetItemLocale(pProto->ItemId);
            if (il)
            {
                if (il->Name.size() > loc_idx && !il->Name[loc_idx].empty())
                    Name = il->Name[loc_idx];
                if (il->Description.size() > loc_idx && !il->Description[loc_idx].empty())
                    Description = il->Description[loc_idx];
            }
        }
                                                            // guess size
        WorldPacket data( SMSG_ITEM_QUERY_SINGLE_RESPONSE, 600);
        data << pProto->ItemId;
        data << pProto->Class;
        data << pProto->SubClass;
        data << uint32(-1);                                 // new 2.0.3, not exist in wdb cache?
        data << Name;
        data << uint8(0x00);                                //pProto->Name2; // blizz not send name there, just uint8(0x00); <-- \0 = empty string = empty name...
        data << uint8(0x00);                                //pProto->Name3; // blizz not send name there, just uint8(0x00);
        data << uint8(0x00);                                //pProto->Name4; // blizz not send name there, just uint8(0x00);
        data << pProto->DisplayInfoID;
        data << pProto->Quality;
        data << pProto->Flags;
        data << pProto->BuyPrice;
        data << pProto->SellPrice;
        data << pProto->InventoryType;
        data << pProto->AllowableClass;
        data << pProto->AllowableRace;
        data << pProto->ItemLevel;
        data << pProto->RequiredLevel;
        data << pProto->RequiredSkill;
        data << pProto->RequiredSkillRank;
        data << pProto->RequiredSpell;
        data << pProto->RequiredHonorRank;
        data << pProto->RequiredCityRank;
        data << pProto->RequiredReputationFaction;
        data << pProto->RequiredReputationRank;
        data << pProto->MaxCount;
        data << pProto->Stackable;
        data << pProto->ContainerSlots;
        for(int i = 0; i < 10; i++)
        {
            data << pProto->ItemStat[i].ItemStatType;
            data << pProto->ItemStat[i].ItemStatValue;
        }
        for(int i = 0; i < 5; i++)
        {
            data << pProto->Damage[i].DamageMin;
            data << pProto->Damage[i].DamageMax;
            data << pProto->Damage[i].DamageType;
        }
        data << pProto->Armor;
        data << pProto->HolyRes;
        data << pProto->FireRes;
        data << pProto->NatureRes;
        data << pProto->FrostRes;
        data << pProto->ShadowRes;
        data << pProto->ArcaneRes;
        data << pProto->Delay;
        data << pProto->Ammo_type;

        data << (float)pProto->RangedModRange;
        for(int s = 0; s < 5; s++)
        {
            // send DBC data for cooldowns in same way as it used in Spell::SendSpellCooldown
            // use `item_template` or if not set then only use spell cooldowns
            SpellEntry const* spell = sSpellStore.LookupEntry(pProto->Spells[s].SpellId);
            if(spell)
            {
                bool db_data = pProto->Spells[s].SpellCooldown >= 0 || pProto->Spells[s].SpellCategoryCooldown >= 0;

                data << pProto->Spells[s].SpellId;
                data << pProto->Spells[s].SpellTrigger;
                data << uint32(-abs(pProto->Spells[s].SpellCharges));

                if(db_data)
                {
                    data << uint32(pProto->Spells[s].SpellCooldown);
                    data << uint32(pProto->Spells[s].SpellCategory);
                    data << uint32(pProto->Spells[s].SpellCategoryCooldown);
                }
                else
                {
                    data << uint32(spell->RecoveryTime);
                    data << uint32(spell->Category);
                    data << uint32(spell->CategoryRecoveryTime);
                }
            }
            else
            {
                data << uint32(0);
                data << uint32(0);
                data << uint32(0);
                data << uint32(-1);
                data << uint32(0);
                data << uint32(-1);
            }
        }
        data << pProto->Bonding;
        data << Description;
        data << pProto->PageText;
        data << pProto->LanguageID;
        data << pProto->PageMaterial;
        data << pProto->StartQuest;
        data << pProto->LockID;
        data << pProto->Material;
        data << pProto->Sheath;
        data << pProto->RandomProperty;
        data << pProto->RandomSuffix;
        data << pProto->Block;
        data << pProto->ItemSet;
        data << pProto->MaxDurability;
        data << pProto->Area;
        data << pProto->Map;                                // Added in 1.12.x & 2.0.1 client branch
        data << pProto->BagFamily;
        data << pProto->TotemCategory;
        for(int s = 0; s < 3; s++)
        {
            data << pProto->Socket[s].Color;
            data << pProto->Socket[s].Content;
        }
        data << pProto->socketBonus;
        data << pProto->GemProperties;
        data << pProto->ExtendedCost;
        data << pProto->RequiredArenaRank;
        data << pProto->RequiredDisenchantSkill;
        data << pProto->ArmorDamageModifier;
        SendPacket( &data );
    }
    else
    {
        sLog.outDebug(  "WORLD: CMSG_ITEM_QUERY_SINGLE - NO item INFO! (ENTRY: %u)", item );
        WorldPacket data( SMSG_ITEM_QUERY_SINGLE_RESPONSE, 4);
        data << uint32(item | 0x80000000);
        SendPacket( &data );
    }
}

void WorldSession::HandleReadItem( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1+1);

    //sLog.outDebug( "WORLD: CMSG_READ_ITEM");

    uint8 bag, slot;
    recv_data >> bag >> slot;

    //sLog.outDetail("STORAGE: Read bag = %u, slot = %u", bag, slot);
    Item *pItem = _player->GetItemByPos( bag, slot );

    if( pItem && pItem->GetProto()->PageText )
    {
        WorldPacket data;

        uint8 msg = _player->CanUseItem( pItem );
        if( msg == EQUIP_ERR_OK )
        {
            data.Initialize (SMSG_READ_ITEM_OK, 8);
            sLog.outDetail("STORAGE: Item page sent");
        }
        else
        {
            data.Initialize( SMSG_READ_ITEM_FAILED, 8 );
            sLog.outDetail("STORAGE: Unable to read item");
            _player->SendEquipError( msg, pItem, NULL );
        }
        data << pItem->GetGUID();
        SendPacket(&data);
    }
    else
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, NULL, NULL );
}

void WorldSession::HandlePageQuerySkippedOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,4+4+4);

    sLog.outDebug(  "WORLD: Received CMSG_PAGE_TEXT_QUERY" );

    uint32 itemid, guidlow, guidhigh;

    recv_data >> itemid >> guidlow >> guidhigh;

    sLog.outDetail( "Packet Info: itemid: %u guidlow: %u guidhigh: %u", itemid, guidlow, guidhigh );
}

void WorldSession::HandleSellItemOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,8+8+1);

    sLog.outDebug(  "WORLD: Received CMSG_SELL_ITEM" );
    uint64 vendorguid, itemguid;
    uint8 count;

    recv_data >> vendorguid >> itemguid >> count;

    if(!itemguid)
        return;

    Creature *pCreature = ObjectAccessor::GetNPCIfCanInteractWith(*_player, vendorguid,UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        sLog.outDebug( "WORLD: HandleSellItemOpcode - Unit (GUID: %u) not found or you can't interact with him.", uint32(GUID_LOPART(vendorguid)) );
        _player->SendSellError( SELL_ERR_CANT_FIND_VENDOR, NULL, itemguid, 0);
        return;
    }

    Item *pItem = _player->GetItemByGuid( itemguid );
    if( pItem )
    {
        // prevent sell not owner item
        if(_player->GetGUID()!=pItem->GetOwnerGUID())
        {
            _player->SendSellError( SELL_ERR_CANT_SELL_ITEM, pCreature, itemguid, 0);
            return;
        }

        // prevent sell non empty bag by drag-and-drop at vendor's item list
        if(pItem->IsBag() && !((Bag*)pItem)->IsEmpty())
        {
            _player->SendSellError( SELL_ERR_CANT_SELL_ITEM, pCreature, itemguid, 0);
            return;
        }

        // prevent sell currently looted item
        if(_player->GetLootGUID()==pItem->GetGUID())
        {
            _player->SendSellError( SELL_ERR_CANT_SELL_ITEM, pCreature, itemguid, 0);
            return;
        }

        // special case at auto sell (sell all)
        if(count==0)
        {
            count = pItem->GetCount();
        }
        else
            // prevent sell more items that exist in stack (possable only not from client)
        if(count > pItem->GetCount())
        {
            _player->SendSellError( SELL_ERR_CANT_SELL_ITEM, pCreature, itemguid, 0);
            return;
        }

        ItemPrototype const *pProto = pItem->GetProto();
        if( pProto )
        {
            if( pProto->SellPrice > 0 )
            {
                _player->ModifyMoney( pProto->SellPrice * count );

                if(count < pItem->GetCount())               // need split items
                {
                    pItem->SetCount( pItem->GetCount() - count );
                    if( _player->IsInWorld() )
                        pItem->SendUpdateToPlayer( _player );
                    pItem->SetState(ITEM_CHANGED, _player);

                    Item *pNewItem = _player->CreateItem( pItem->GetEntry(), count );
                    _player->AddItemToBuyBackSlot( pNewItem );
                    if( _player->IsInWorld() )
                        pNewItem->SendUpdateToPlayer( _player );
                }
                else
                {
                    _player->RemoveItem( pItem->GetBagSlot(), pItem->GetSlot(), true);
                    pItem->RemoveFromUpdateQueueOf(_player);
                    _player->AddItemToBuyBackSlot( pItem );
                }
            }
            else
                _player->SendSellError( SELL_ERR_CANT_SELL_ITEM, pCreature, itemguid, 0);
            return;
        }
    }
    _player->SendSellError( SELL_ERR_CANT_FIND_ITEM, pCreature, itemguid, 0);
    return;
}

void WorldSession::HandleBuybackItem(WorldPacket & recv_data)
{
    CHECK_PACKET_SIZE(recv_data,8+4);

    sLog.outDebug(  "WORLD: Received CMSG_BUYBACK_ITEM" );
    uint64 vendorguid;
    uint32 slot;

    recv_data >> vendorguid >> slot;

    Creature *pCreature = ObjectAccessor::GetNPCIfCanInteractWith(*_player, vendorguid,UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        sLog.outDebug( "WORLD: HandleBuybackItem - Unit (GUID: %u) not found or you can't interact with him.", uint32(GUID_LOPART(vendorguid)) );
        _player->SendSellError( SELL_ERR_CANT_FIND_VENDOR, NULL, 0, 0);
        return;
    }

    Item *pItem = _player->GetItemFromBuyBackSlot( slot );
    if( pItem )
    {
        uint32 price = _player->GetUInt32Value( PLAYER_FIELD_BUYBACK_PRICE_1 + slot - BUYBACK_SLOT_START );
        if( _player->GetMoney() < price )
        {
            _player->SendBuyError( BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, pItem->GetEntry(), 0);
            return;
        }
        uint16 dest;

        uint8 msg = _player->CanStoreItem( NULL_BAG, NULL_SLOT, dest, pItem, false );
        if( msg == EQUIP_ERR_OK )
        {
            _player->ModifyMoney( -(int32)price );
            _player->RemoveItemFromBuyBackSlot( slot, false );
            _player->StoreItem( dest, pItem, true );
        }
        else
            _player->SendEquipError( msg, pItem, NULL );
        return;
    }
    else
        _player->SendBuyError( BUY_ERR_CANT_FIND_ITEM, pCreature, 0, 0);
}

void WorldSession::HandleBuyItemInSlotOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,8+4+8+1+1);

    sLog.outDebug(  "WORLD: Received CMSG_BUY_ITEM_IN_SLOT" );
    uint64 vendorguid, bagguid;
    uint32 item;
    uint8 slot, count;

    recv_data >> vendorguid >> item >> bagguid >> slot >> count;

    if (GetPlayer()->BuyItemFromVendor(vendorguid,item,count,bagguid,slot))
        SendListInventory(vendorguid);                      // Item was limited in number you can buy, then send list to update count displayed
}

void WorldSession::HandleBuyItemOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,8+4+1+1);

    sLog.outDebug(  "WORLD: Received CMSG_BUY_ITEM" );
    uint64 vendorguid;
    uint32 item;
    uint8 count, unk1;

    recv_data >> vendorguid >> item >> count >> unk1;

    if (GetPlayer()->BuyItemFromVendor(vendorguid,item,count,NULL_BAG,NULL_SLOT))
        SendListInventory(vendorguid);                      // Item was limited in number you can buy, then send list to update count displayed
}

void WorldSession::HandleListInventoryOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,8);

    uint64 guid;

    recv_data >> guid;

    if(!GetPlayer()->isAlive())
        return;

    sLog.outDebug(  "WORLD: Recvd CMSG_LIST_INVENTORY" );

    SendListInventory( guid );
}

void WorldSession::SendListInventory( uint64 vendorguid )
{
    sLog.outDebug(  "WORLD: Sent SMSG_LIST_INVENTORY" );

    Creature *pCreature = ObjectAccessor::GetNPCIfCanInteractWith(*_player, vendorguid,UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        sLog.outDebug( "WORLD: SendListInventory - Unit (GUID: %u) not found or you can't interact with him.", uint32(GUID_LOPART(vendorguid)) );
        _player->SendSellError( SELL_ERR_CANT_FIND_VENDOR, NULL, 0, 0);
        return;
    }

    // load vendor items if not yet
    pCreature->LoadGoods();

    uint8 numitems = pCreature->GetItemCount();
    uint8 count = 0;
    uint32 ptime = time(NULL);
    uint32 diff;

    WorldPacket data( SMSG_LIST_INVENTORY, (8+1+numitems*7*4) );
    data << vendorguid;
    data << numitems;

    float discountMod = _player->GetReputationPriceDiscount(pCreature);

    ItemPrototype const *pProto;
    for(int i = 0; i < numitems; i++ )
    {
        CreatureItem* crItem = pCreature->GetItem(i);
        if( crItem )
        {
            pProto = objmgr.GetItemPrototype(crItem->id);
            if( pProto )
            {
                if((pProto->AllowableClass & _player->getClassMask()) == 0 && pProto->Bonding == BIND_WHEN_PICKED_UP && !_player->isGameMaster())
                    continue;
                count++;
                if( crItem->incrtime != 0 && (crItem->lastincr + crItem->incrtime <= ptime) )
                {
                    diff = uint32((ptime - crItem->lastincr)/crItem->incrtime);
                    if( (crItem->count + diff * pProto->BuyCount) <= crItem->maxcount )
                        crItem->count += diff * pProto->BuyCount;
                    else
                        crItem->count = crItem->maxcount;
                    crItem->lastincr = ptime;
                }
                data << uint32(count);
                data << crItem->id;
                data << pProto->DisplayInfoID;
                data << uint32(crItem->maxcount <= 0 ? 0xFFFFFFFF : crItem->count);

                uint32 price = pProto->BuyPrice;

                // reputation discount
                price = uint32(floor(pProto->BuyPrice * discountMod));

                data << price;
                data << pProto->MaxDurability;
                data << pProto->BuyCount;
            }
        }
    }

    if ( count == 0 || data.size() != 8 + 1 + size_t(count) * 7 * 4 )
        return;

    data.put<uint8>(8, count);
    SendPacket( &data );
}

void WorldSession::HandleAutoStoreBagItemOpcode( WorldPacket & recv_data )
{
    CHECK_PACKET_SIZE(recv_data,1+1+1);

    //sLog.outDebug("WORLD: CMSG_AUTOSTORE_BAG_ITEM");
    uint8 srcbag, srcslot, dstbag;

    recv_data >> srcbag >> srcslot >> dstbag;
    //sLog.outDebug("STORAGE: receive srcbag = %u, srcslot = %u, dstbag = %u", srcbag, srcslot, dstbag);

    Item *pItem = _player->GetItemByPos( srcbag, srcslot );
    if( pItem )
    {
        uint16 dest;
        uint8 msg = _player->CanStoreItem( dstbag, NULL_SLOT, dest, pItem, false );
        if( msg == EQUIP_ERR_OK )
        {
            uint16 src = ( (srcbag << 8) | srcslot );
            if(dest==src)                                   // prevent store in same slot
                return;

            _player->RemoveItem(srcbag, srcslot, true);
            _player->StoreItem( dest, pItem, true );
        }
        else
            _player->SendEquipError( msg, pItem, NULL );
    }
}

void WorldSession::HandleBuyBankSlotOpcode(WorldPacket& /*recvPacket*/)
{
    sLog.outDebug("WORLD: CMSG_BUY_BANK_SLOT");

    uint32 bank = _player->GetUInt32Value(PLAYER_BYTES_2);
    uint32 slot = (bank & 0x70000) >> 16;

    // next slot
    ++slot;

    sLog.outDetail("PLAYER: Buy bank bag slot, slot number = %u", slot);

    BankBagSlotPricesEntry const* slotEntry = sBankBagSlotPricesStore.LookupEntry(slot);

    if(!slotEntry)
        return;

    uint32 price = slotEntry->price;

    if (_player->GetMoney() >= price)
    {
        bank = (bank & ~0x70000) + (slot << 16);

        _player->SetUInt32Value(PLAYER_BYTES_2, bank);
        _player->ModifyMoney(-int32(price));
    }
}

void WorldSession::HandleAutoBankItemOpcode(WorldPacket& recvPacket)
{
    CHECK_PACKET_SIZE(recvPacket,1+1);

    sLog.outDebug("WORLD: CMSG_AUTOBANK_ITEM");
    uint8 srcbag, srcslot;

    recvPacket >> srcbag >> srcslot;
    sLog.outDebug("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item *pItem = _player->GetItemByPos( srcbag, srcslot );
    if( pItem )
    {
        uint16 dest;
        uint8 msg = _player->CanBankItem( NULL_BAG, NULL_SLOT, dest, pItem, false );
        if( msg == EQUIP_ERR_OK )
        {
            uint16 src = ( (srcbag << 8) | srcslot );
            if(dest==src)                                   // prevent store in same slot
                return;

            _player->RemoveItem(srcbag, srcslot, true);
            _player->BankItem( dest, pItem, true );
        }
        else
            _player->SendEquipError( msg, pItem, NULL );
    }
}

void WorldSession::HandleAutoStoreBankItemOpcode(WorldPacket& recvPacket)
{
    CHECK_PACKET_SIZE(recvPacket,1+1);

    sLog.outDebug("WORLD: CMSG_AUTOSTORE_BANK_ITEM");
    uint8 srcbag, srcslot;

    recvPacket >> srcbag >> srcslot;
    sLog.outDebug("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item *pItem = _player->GetItemByPos( srcbag, srcslot );
    if( pItem )
    {
        if(_player->IsBankPos(srcbag, srcslot))             // moving from bank to inventory
        {
            uint16 dest;
            uint8 msg = _player->CanStoreItem( NULL_BAG, NULL_SLOT, dest, pItem, false );
            if( msg == EQUIP_ERR_OK )
            {
                _player->RemoveItem(srcbag, srcslot, true);
                _player->StoreItem( dest, pItem, true );
            }
            else
                _player->SendEquipError( msg, pItem, NULL );
        }
        else                                                // moving from inventory to bank
        {
            uint16 dest;
            uint8 msg = _player->CanBankItem( NULL_BAG, NULL_SLOT, dest, pItem, false );
            if( msg == EQUIP_ERR_OK )
            {
                _player->RemoveItem(srcbag, srcslot, true);
                _player->BankItem( dest, pItem, true );
            }
            else
                _player->SendEquipError( msg, pItem, NULL );
        }
    }
}

void WorldSession::HandleSetAmmoOpcode(WorldPacket & recv_data)
{
    CHECK_PACKET_SIZE(recv_data,4);

    if(!GetPlayer()->isAlive())
    {
        GetPlayer()->SendEquipError( EQUIP_ERR_YOU_ARE_DEAD, NULL, NULL );
        return;
    }

    sLog.outDebug("WORLD: CMSG_SET_AMMO");
    uint32 item;

    recv_data >> item;

    if(!item)
        GetPlayer()->RemoveAmmo();
    else
        GetPlayer()->SetAmmo(item);
}

void WorldSession::SendEnchantmentLog(uint64 Target, uint64 Caster,uint32 ItemID,uint32 SpellID)
{
    WorldPacket data(SMSG_ENCHANTMENTLOG, (8+8+4+4+1));     // last check 2.0.10
    data << Target;
    data << Caster;
    data << ItemID;
    data << SpellID;
    data << uint8(0);
    SendPacket(&data);
}

void WorldSession::SendItemEnchantTimeUpdate(uint64 Playerguid, uint64 Itemguid,uint32 slot,uint32 Duration)
{
                                                            // last check 2.0.10
    WorldPacket data(SMSG_ITEM_ENCHANT_TIME_UPDATE, (8+4+4+8));
    data << uint64(Itemguid);
    data << uint32(slot);
    data << uint32(Duration);
    data << uint64(Playerguid);
    SendPacket(&data);
}

void WorldSession::HandleItemNameQueryOpcode(WorldPacket & recv_data)
{
    CHECK_PACKET_SIZE(recv_data,4);

    uint32 itemid;
    recv_data >> itemid;
    sLog.outDebug("WORLD: CMSG_ITEM_NAME_QUERY %u", itemid);
    ItemPrototype const *pProto = objmgr.GetItemPrototype( itemid );
    if( pProto )
    {
        std::string Name;
        Name = pProto->Name1;

        int loc_idx = GetSessionLocaleIndex();
        if (loc_idx >= 0)
        {
            ItemLocale const *il = objmgr.GetItemLocale(pProto->ItemId);
            if (il)
            {
                if (il->Name.size() > loc_idx && !il->Name[loc_idx].empty())
                    Name = il->Name[loc_idx];
            }
        }
                                                            // guess size
        WorldPacket data(SMSG_ITEM_NAME_QUERY_RESPONSE, (4+10));
        data << pProto->ItemId;
        data << Name;
        SendPacket(&data);
        return;
    }
    else
        sLog.outDebug("WORLD: CMSG_ITEM_NAME_QUERY for item %u failed (unknown item)", itemid);
}

void WorldSession::HandleWrapItemOpcode(WorldPacket& recv_data)
{
    CHECK_PACKET_SIZE(recv_data,1+1+1+1);

    sLog.outDebug("Received opcode CMSG_WRAP_ITEM");

    uint8 gift_bag, gift_slot, item_bag, item_slot;
    //recv_data.hexlike();

    recv_data >> gift_bag >> gift_slot;                     // paper
    recv_data >> item_bag >> item_slot;                     // item

    sLog.outDebug("WRAP: receive gift_bag = %u, gift_slot = %u, item_bag = %u, item_slot = %u", gift_bag, gift_slot, item_bag, item_slot);

    Item *gift = _player->GetItemByPos( gift_bag, gift_slot );
    if(!gift)
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, gift, NULL );
        return;
    }

    Item *item = _player->GetItemByPos( item_bag, item_slot );

    if( !item )
    {
        _player->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, item, NULL );
        return;
    }

    if(item==gift)                                          // not possable with pacjket from real client
    {
        _player->SendEquipError( EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if(item->IsEquipped())
    {
        _player->SendEquipError( EQUIP_ERR_EQUIPPED_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if(item->GetUInt64Value(ITEM_FIELD_GIFTCREATOR))        // HasFlag(ITEM_FIELD_FLAGS, ITEM_FLAGS_WRAPPED);
    {
        _player->SendEquipError( EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if(item->IsBag())
    {
        _player->SendEquipError( EQUIP_ERR_BAGS_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if(item->IsSoulBound())
    {
        _player->SendEquipError( EQUIP_ERR_BOUND_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    if(item->GetMaxStackCount() != 1)
    {
        _player->SendEquipError( EQUIP_ERR_STACKABLE_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    // maybe not correct check  (it is better than nothing)
    if(item->GetProto()->MaxCount>0)
    {
        _player->SendEquipError( EQUIP_ERR_UNIQUE_CANT_BE_WRAPPED, item, NULL );
        return;
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("INSERT INTO character_gifts VALUES ('%u', '%u', '%u', '%u')", GUID_LOPART(item->GetOwnerGUID()), item->GetGUIDLow(), item->GetEntry(), item->GetUInt32Value(ITEM_FIELD_FLAGS));
    item->SetUInt32Value(OBJECT_FIELD_ENTRY, gift->GetUInt32Value(OBJECT_FIELD_ENTRY));

    switch (item->GetEntry())
    {
        case 5042:  item->SetUInt32Value(OBJECT_FIELD_ENTRY,  5043); break;
        case 5048:  item->SetUInt32Value(OBJECT_FIELD_ENTRY,  5044); break;
        case 17303: item->SetUInt32Value(OBJECT_FIELD_ENTRY, 17302); break;
        case 17304: item->SetUInt32Value(OBJECT_FIELD_ENTRY, 17305); break;
        case 17307: item->SetUInt32Value(OBJECT_FIELD_ENTRY, 17308); break;
        case 21830: item->SetUInt32Value(OBJECT_FIELD_ENTRY, 21831); break;
    }
    item->SetUInt64Value(ITEM_FIELD_GIFTCREATOR, _player->GetGUID());
    item->SetUInt32Value(ITEM_FIELD_FLAGS, ITEM_FLAGS_WRAPPED);
    item->SetState(ITEM_CHANGED, _player);

    if(item->GetState()==ITEM_NEW)                          // save new item, to have alway for `character_gifts` record in `item_template`
    {
        // after save it will be impossible to remove the item from the queue
        item->RemoveFromUpdateQueueOf(_player);
        item->SaveToDB();
    }
    CharacterDatabase.CommitTransaction();

    uint32 count = 1;
    _player->DestroyItemCount(gift, count, true);
}

void WorldSession::HandleSocketOpcode(WorldPacket& recv_data)
{
    sLog.outDebug("WORLD: CMSG_SOCKET_ITEM");

    CHECK_PACKET_SIZE(recv_data,8*4);

    uint64 guids[4];
    uint32 GemEnchants[3], OldEnchants[3];
    Item *Gems[3];
    bool SocketBonusActivated, SocketBonusToBeActivated;

    for(int i = 0; i < 4; i++)
        recv_data >> guids[i];

    if(!guids[0])
        return;

    //cheat -> tried to socket same gem multiple times
    if((guids[1] && (guids[1] == guids[2] || guids[1] == guids[3])) || (guids[2] && (guids[2] == guids[3])))
        return;

    Item *itemTarget = _player->GetItemByGuid(guids[0]);
    if(!itemTarget)                                         //missing item to socket
        return;

    //this slot is excepted when applying / removing meta gem bonus
    uint8 slot = itemTarget->IsEquipped() ? itemTarget->GetSlot() : NULL_SLOT;

    for(int i = 0; i < 3; i++)
        Gems[i] = guids[i + 1] ? _player->GetItemByGuid(guids[i + 1]) : NULL;

    GemPropertiesEntry const *GemProps[3];
    for(int i = 0; i < 3; ++i)                              //get geminfo from dbc storage
    {
        GemProps[i] = (Gems[i]) ? sGemPropertiesStore.LookupEntry(Gems[i]->GetProto()->GemProperties) : NULL;
    }

    for(int i = 0; i < 3; ++i)                              //check for hack maybe
    {
        // tried to put gem in socket where no socket exists / tried to put normal gem in meta socket
        // tried to put meta gem in normal socket
        if( GemProps[i] && ( !itemTarget->GetProto()->Socket[i].Color ||
            itemTarget->GetProto()->Socket[i].Color == SOCKET_COLOR_META && GemProps[i]->color != SOCKET_COLOR_META ||
            itemTarget->GetProto()->Socket[i].Color != SOCKET_COLOR_META && GemProps[i]->color == SOCKET_COLOR_META ) )
            return;
    }

    for(int i = 0; i < 3; ++i)                              //get new and old enchantments
    {
        GemEnchants[i] = (GemProps[i]) ? GemProps[i]->spellitemenchantement : 0;
        OldEnchants[i] = itemTarget->GetEnchantmentId(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT+i));
    }

    // check unique-equipped conditions
    for(int i = 0; i < 3; ++i)
    {
        if (Gems[i] && (Gems[i]->GetProto()->Flags & ITEM_FLAGS_UNIQUE_EQUIPPED))
        {
            // for equipped item check all equipment for duplicate equipped gems
            if(itemTarget->IsEquipped())
            {
                if(GetPlayer()->GetItemOrItemWithGemEquipped(Gems[i]->GetEntry()))
                {
                    _player->SendEquipError( EQUIP_ERR_ITEM_UNIQUE_EQUIPABLE, itemTarget, NULL );
                    return;
                }
            }

            // continue check for case when attempt add 2 similar unique equipped gems in one item.
            for (int j = 0; j < 3; ++j)
            {
                if ((i != j) && (Gems[j]) && (Gems[i]->GetProto()->ItemId == Gems[j]->GetProto()->ItemId))
                {
                    _player->SendEquipError( EQUIP_ERR_ITEM_UNIQUE_EQUIPPABLE_SOCKETED, itemTarget, NULL );
                    return;
                }
            }
            for (int j = 0; j < 3; ++j)
            {
                if (OldEnchants[j])
                {
                    SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(OldEnchants[j]);
                    if(!enchantEntry)
                        continue;

                    if ((enchantEntry->GemID == Gems[i]->GetProto()->ItemId) && (i != j))
                    {
                        _player->SendEquipError( EQUIP_ERR_ITEM_UNIQUE_EQUIPPABLE_SOCKETED, itemTarget, NULL );
                        return;
                    }
                }
            }
        }
    }

    SocketBonusActivated = itemTarget->GemsFitSockets();    //save state of socketbonus
    _player->ToggleMetaGemsActive(slot, false);             //turn off all metagems (except for the target item)

    //if a meta gem is being equipped, all information has to be written to the item before testing if the conditions for the gem are met

    //remove ALL enchants
    for(uint32 enchant_slot = SOCK_ENCHANTMENT_SLOT; enchant_slot < SOCK_ENCHANTMENT_SLOT+3; ++enchant_slot)
        _player->ApplyEnchantment(itemTarget,EnchantmentSlot(enchant_slot),false);

    for(int i = 0; i < 3; ++i)
    {
        if(GemEnchants[i])
        {
            itemTarget->SetEnchantment(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT+i), GemEnchants[i],0,0);
            if(Item* guidItem = _player->GetItemByGuid(guids[i + 1]))
                _player->DestroyItem(guidItem->GetBagSlot(), guidItem->GetSlot(), true );
        }
    }

    for(uint32 enchant_slot = SOCK_ENCHANTMENT_SLOT; enchant_slot < SOCK_ENCHANTMENT_SLOT+3; ++enchant_slot)
        _player->ApplyEnchantment(itemTarget,EnchantmentSlot(enchant_slot),true);

    SocketBonusToBeActivated = itemTarget->GemsFitSockets();//current socketbonus state
    if(SocketBonusActivated ^ SocketBonusToBeActivated)     //if there was a change...
    {
        _player->ApplyEnchantment(itemTarget,BONUS_ENCHANTMENT_SLOT,false);
        itemTarget->SetEnchantment(BONUS_ENCHANTMENT_SLOT, (SocketBonusToBeActivated ? itemTarget->GetProto()->socketBonus : 0), 0, 0);
        _player->ApplyEnchantment(itemTarget,BONUS_ENCHANTMENT_SLOT,true);
        //it is not displayed, client has an inbuilt system to determine if the bonus is activated
    }

    _player->ToggleMetaGemsActive(slot, true);              //turn on all metagems (except for target item)
}

void WorldSession::HandleCancelTempItemEnchantmentOpcode(WorldPacket& recv_data)
{
    sLog.outDebug("WORLD: CMSG_CANCEL_TEMP_ITEM_ENCHANTMENT");

    CHECK_PACKET_SIZE(recv_data,4);

    uint32 eslot;

    recv_data >> eslot;

    // apply only to equipped item
    if(!Player::IsEquipmentPos(INVENTORY_SLOT_BAG_0,eslot))
        return;

    Item* item = GetPlayer()->GetItemByPos(INVENTORY_SLOT_BAG_0, eslot);

    if(!item)
        return;

    if(!item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
        return;

    GetPlayer()->ApplyEnchantment(item,TEMP_ENCHANTMENT_SLOT,false);
    item->ClearEnchantment(TEMP_ENCHANTMENT_SLOT);
}
