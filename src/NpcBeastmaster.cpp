/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "NpcBeastmaster.h"
#include "Config.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "StringFormat.h"
#include "StringConvert.h"
#include <map>
#include <vector>

namespace
{
    std::vector<uint32> HunterSpells = { 883, 982, 2641, 6991, 48990, 1002, 1462, 6197 };

    PetsStore pets;
    PetsStore exoticPets;
    PetsStore rarePets;
    PetsStore rareExoticPets;

    bool BeastMasterHunterOnly;
    bool BeastMasterAllowExotic;
    bool BeastMasterKeepPetHappy;
    uint32 BeastMasterMinLevel;
    bool BeastMasterHunterBeastMasteryRequired;

    enum PetGossip
    {
        PET_BEASTMASTER_HOWL = 9036,
        PET_PAGE_SIZE = 13,
        PET_PAGE_START_PETS = 501,
        PET_PAGE_START_EXOTIC_PETS = 601,
        PET_PAGE_START_RARE_PETS = 701,
        PET_PAGE_START_RARE_EXOTIC_PETS = 801,
        PET_PAGE_MAX = 901,
        PET_MAIN_MENU = 50,
        PET_REMOVE_SKILLS = 80,
        PET_GOSSIP_HELLO = 601026,
        PET_GOSSIP_BROWSE = 601027
    };

    // PetSpells
    constexpr auto PET_SPELL_CALL_PET = 883;
    constexpr auto PET_SPELL_TAME_BEAST = 13481;
    constexpr auto PET_SPELL_BEAST_MASTERY = 53270;
    constexpr auto PET_MAX_HAPPINESS = 1048000;
}

/*static*/ NpcBeastmaster* NpcBeastmaster::instance()
{
    static NpcBeastmaster instance;
    return &instance;
}

void NpcBeastmaster::LoadSystem(bool /*reload = false*/)
{
    BeastMasterHunterOnly = sConfigMgr->GetOption<bool>("BeastMaster.HunterOnly", true);
    BeastMasterAllowExotic = sConfigMgr->GetOption<bool>("BeastMaster.AllowExotic", false);
    BeastMasterKeepPetHappy = sConfigMgr->GetOption<bool>("BeastMaster.KeepPetHappy", false);
    BeastMasterMinLevel = sConfigMgr->GetOption<uint32>("BeastMaster.MinLevel", 10);
    BeastMasterHunterBeastMasteryRequired = sConfigMgr->GetOption<uint32>("BeastMaster.HunterBeastMasteryRequired", true);

    if (BeastMasterMinLevel > 80)
    {
        BeastMasterMinLevel = 10;
    }

    LoadPets(sConfigMgr->GetOption<std::string>("BeastMaster.Pets", ""), pets);
    LoadPets(sConfigMgr->GetOption<std::string>("BeastMaster.ExoticPets", ""), exoticPets);
    LoadPets(sConfigMgr->GetOption<std::string>("BeastMaster.RarePets", ""), rarePets);
    LoadPets(sConfigMgr->GetOption<std::string>("BeastMaster.RareExoticPets", ""), rareExoticPets);
}

void NpcBeastmaster::ShowMainMenu(Player* player, Creature* creature)
{
    // If enabled for Hunters only..
    if (BeastMasterHunterOnly)
    {
        if (player->getClass() != CLASS_HUNTER)
        {
            creature->Whisper("对不起，但宠物只属于猎人。", LANG_UNIVERSAL, player);
            return;
        }
    }

    // Check level requirement
    if (player->getLevel() < BeastMasterMinLevel && BeastMasterMinLevel != 0)
    {
        std::string messageExperience = Acore::StringFormatFmt("对不起，{}，但你必须达到{}级才能领养宠物。", player->GetName(), BeastMasterMinLevel);
        creature->Whisper(messageExperience.c_str(), LANG_UNIVERSAL, player);
        return;
    }

    ClearGossipMenuFor(player);

    // MAIN MENU
    AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "普通宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_PETS);
    AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "稀有宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_PETS);

    if (BeastMasterAllowExotic || player->HasSpell(PET_SPELL_BEAST_MASTERY) || player->HasTalent(PET_SPELL_BEAST_MASTERY, player->GetActiveSpec()))
    {
        if (player->getClass() != CLASS_HUNTER)
        {
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "异种宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_EXOTIC_PETS);
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "稀有异种宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_EXOTIC_PETS);
        }
        else if (!BeastMasterHunterBeastMasteryRequired || player->HasTalent(PET_SPELL_BEAST_MASTERY, player->GetActiveSpec()))
        {
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "异种宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_EXOTIC_PETS);
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "稀有异种宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_EXOTIC_PETS);
        }
    }

    // remove pet skills (not for hunters)
    //if (player->getClass() != CLASS_HUNTER && player->HasSpell(PET_SPELL_CALL_PET))
     //   AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "遗忘猎人技能", GOSSIP_SENDER_MAIN, PET_REMOVE_SKILLS);

    // Stables for hunters only - Doesn't seem to work for other classes
    if (player->getClass() == CLASS_HUNTER)
        AddGossipItemFor(player, GOSSIP_ICON_TAXI, "参观马厩", GOSSIP_SENDER_MAIN, GOSSIP_OPTION_STABLEPET);

    // Pet Food Vendor
    AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "购买宠物食物", GOSSIP_SENDER_MAIN, GOSSIP_OPTION_VENDOR);

    SendGossipMenuFor(player, PET_GOSSIP_HELLO, creature->GetGUID());

    // Howl
    player->PlayDirectSound(PET_BEASTMASTER_HOWL);
}

void NpcBeastmaster::GossipSelect(Player* player, Creature* creature, uint32 action)
{
    ClearGossipMenuFor(player);

    if (action == PET_MAIN_MENU)
    {
        // MAIN MENU
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "普通宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_PETS);
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "稀有宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_PETS);

        if (BeastMasterAllowExotic || player->HasSpell(PET_SPELL_BEAST_MASTERY) || player->HasTalent(PET_SPELL_BEAST_MASTERY, player->GetActiveSpec()))
        {
            if (player->getClass() != CLASS_HUNTER)
            {
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "异种宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_EXOTIC_PETS);
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "稀有异种宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_EXOTIC_PETS);
            }
            else if (!BeastMasterHunterBeastMasteryRequired || player->HasTalent(PET_SPELL_BEAST_MASTERY, player->GetActiveSpec()))
            {
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "异种宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_EXOTIC_PETS);
                AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "稀有异种宠物", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_EXOTIC_PETS);
            }
        }

        // remove pet skills (not for hunters)
        //  if (player->getClass() != CLASS_HUNTER && player->HasSpell(PET_SPELL_CALL_PET))
        //    AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "遗忘猎人技能", GOSSIP_SENDER_MAIN, PET_REMOVE_SKILLS);

        // Stables for hunters only - Doesn't seem to work for other classes
        if (player->getClass() == CLASS_HUNTER)
            AddGossipItemFor(player, GOSSIP_ICON_TAXI, "参观马厩", GOSSIP_SENDER_MAIN, GOSSIP_OPTION_STABLEPET);

        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "购买宠物食物", GOSSIP_SENDER_MAIN, GOSSIP_OPTION_VENDOR);
        SendGossipMenuFor(player, PET_GOSSIP_HELLO, creature->GetGUID());
    }
    else if (action >= PET_PAGE_START_PETS && action < PET_PAGE_START_EXOTIC_PETS)
    {
        // PETS
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "返回", GOSSIP_SENDER_MAIN, PET_MAIN_MENU);
        int page = action - PET_PAGE_START_PETS + 1;
        int maxPage = pets.size() / PET_PAGE_SIZE + (pets.size() % PET_PAGE_SIZE != 0);

        if (page > 1)
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "上一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_PETS + page - 2);

        if (page < maxPage)
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "下一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_PETS + page);

        AddPetsToGossip(player, pets, page);
        SendGossipMenuFor(player, PET_GOSSIP_BROWSE, creature->GetGUID());
    }
    else if (action >= PET_PAGE_START_EXOTIC_PETS && action < PET_PAGE_START_RARE_PETS)
    {
        // EXOTIC BEASTS
        // Teach Beast Mastery or Spirit Beasts won't work properly
        if (!(player->HasSpell(PET_SPELL_BEAST_MASTERY) || player->HasTalent(PET_SPELL_BEAST_MASTERY, player->GetActiveSpec())))
        {
            player->addSpell(PET_SPELL_BEAST_MASTERY, SPEC_MASK_ALL, false);
            std::ostringstream messageLearn;
            messageLearn << "我已经教你了兽王的技巧， " << player->GetName() << ".";
            creature->Whisper(messageLearn.str().c_str(), LANG_UNIVERSAL, player);
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "返回", GOSSIP_SENDER_MAIN, PET_MAIN_MENU);
        int page = action - PET_PAGE_START_EXOTIC_PETS + 1;
        int maxPage = exoticPets.size() / PET_PAGE_SIZE + (exoticPets.size() % PET_PAGE_SIZE != 0);

        if (page > 1)
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "上一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_EXOTIC_PETS + page - 2);

        if (page < maxPage)
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "下一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_EXOTIC_PETS + page);

        AddPetsToGossip(player, exoticPets, page);
        SendGossipMenuFor(player, PET_GOSSIP_BROWSE, creature->GetGUID());
    }
    else if (action >= PET_PAGE_START_RARE_PETS && action < PET_PAGE_START_RARE_EXOTIC_PETS)
    {
        // RARE PETS
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "返回", GOSSIP_SENDER_MAIN, PET_MAIN_MENU);
        int page = action - PET_PAGE_START_RARE_PETS + 1;
        int maxPage = rarePets.size() / PET_PAGE_SIZE + (rarePets.size() % PET_PAGE_SIZE != 0);

        if (page > 1)
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "上一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_PETS + page - 2);

        if (page < maxPage)
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "下一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_PETS + page);

        AddPetsToGossip(player, rarePets, page);
        SendGossipMenuFor(player, PET_GOSSIP_BROWSE, creature->GetGUID());
    }
    else if (action >= PET_PAGE_START_RARE_EXOTIC_PETS && action < PET_PAGE_MAX)
    {
        // RARE EXOTIC BEASTS
        // Teach Beast Mastery or Spirit Beasts won't work properly
        if (!(player->HasSpell(PET_SPELL_BEAST_MASTERY) || player->HasTalent(PET_SPELL_BEAST_MASTERY, player->GetActiveSpec())))
        {
            player->addSpell(PET_SPELL_BEAST_MASTERY, SPEC_MASK_ALL, false);
            std::ostringstream messageLearn;
            messageLearn << "我已经教你了兽王的技巧，" << player->GetName() << ".";
            creature->Whisper(messageLearn.str().c_str(), LANG_UNIVERSAL, player);
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "返回", GOSSIP_SENDER_MAIN, PET_MAIN_MENU);
        int page = action - PET_PAGE_START_RARE_EXOTIC_PETS + 1;
        int maxPage = rareExoticPets.size() / PET_PAGE_SIZE + (rareExoticPets.size() % PET_PAGE_SIZE != 0);

        if (page > 1)
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "上一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_EXOTIC_PETS + page - 2);

        if (page < maxPage)
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "下一页", GOSSIP_SENDER_MAIN, PET_PAGE_START_RARE_EXOTIC_PETS + page);

        AddPetsToGossip(player, rareExoticPets, page);
        SendGossipMenuFor(player, PET_GOSSIP_BROWSE, creature->GetGUID());
    }
    else if (action == PET_REMOVE_SKILLS)
    {
        // remove pet and granted skills
        for (std::size_t i = 0; i < HunterSpells.size(); ++i)
            player->removeSpell(HunterSpells[i], SPEC_MASK_ALL, false);

        player->removeSpell(PET_SPELL_BEAST_MASTERY, SPEC_MASK_ALL, false);
        CloseGossipMenuFor(player);
    }
    else if (action == GOSSIP_OPTION_STABLEPET)
    {
        // STABLE
        player->GetSession()->SendStablePet(creature->GetGUID());
    }
    else if (action == GOSSIP_OPTION_VENDOR)
    {
        // VENDOR
        player->GetSession()->SendListInventory(creature->GetGUID());
    }

    // BEASTS
    if (action >= PET_PAGE_MAX)
        CreatePet(player, creature, action);
}

void NpcBeastmaster::CreatePet(Player* player, Creature* creature, uint32 action)
{
    // Check if player already has a pet
    if (player->IsExistPet())
    {
        creature->Whisper("首先，你必须放弃或者寄养你现在的宠物！", LANG_UNIVERSAL, player);
        CloseGossipMenuFor(player);
        return;
    }

    // Create tamed creature
    Pet* pet = player->CreatePet(action - PET_PAGE_MAX, player->getClass() == CLASS_HUNTER ? PET_SPELL_TAME_BEAST : PET_SPELL_CALL_PET);
    if (!pet)
    {
        creature->Whisper("首先，你必须放弃或者寄养你现在的宠物！", LANG_UNIVERSAL, player);
        return;
    }

    // Set Pet Happiness
    pet->SetPower(POWER_HAPPINESS, PET_MAX_HAPPINESS);

    // Learn Hunter Abilities (only for non-hunters)
    if (player->getClass() != CLASS_HUNTER)
    {
        // Assume player has already learned the spells if they have Call Pet
        if (!player->HasSpell(PET_SPELL_CALL_PET))
        {
            for (auto const& _spell : HunterSpells)
                if (!player->HasSpell(_spell))
                    player->learnSpell(_spell);
        }
    }

    // Farewell
    std::string messageAdopt = Acore::StringFormatFmt("好选择，{}！好好照顾你的{}，你将永远不会独自面对你的敌人。", player->GetName(), pet->GetName());
    creature->Whisper(messageAdopt.c_str(), LANG_UNIVERSAL, player);
    CloseGossipMenuFor(player);
}

void NpcBeastmaster::AddPetsToGossip(Player* player, PetsStore const& petsStore, uint32 page)
{
    uint32 count = 1;

    for (auto const& [petName, petEntry] : petsStore)
    {
        if (count > (page - 1) * PET_PAGE_SIZE && count <= page * PET_PAGE_SIZE)
            AddGossipItemFor(player, GOSSIP_ICON_VENDOR, petName, GOSSIP_SENDER_MAIN, petEntry + PET_PAGE_MAX);

        count++;
    }
}

void NpcBeastmaster::LoadPets(std::string pets, PetsStore& petMap)
{
    petMap.clear();

    std::string delimitedValue;
    std::stringstream petsStringStream;
    std::string petName;
    int count = 0;

    petsStringStream.str(pets);

    while (std::getline(petsStringStream, delimitedValue, ','))
    {
        if (count % 2 == 0)
            petName = delimitedValue;
        else
            petMap[petName] = *Acore::StringTo<uint32>(delimitedValue);

        count++;
    }
}

void NpcBeastmaster::PlayerUpdate(Player* player)
{
    if (BeastMasterKeepPetHappy && player->GetPet())
    {
        Pet* pet = player->GetPet();

        if (pet->getPetType() == HUNTER_PET)
        {
            pet->SetPower(POWER_HAPPINESS, PET_MAX_HAPPINESS);
        }
    }
}
