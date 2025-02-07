/*
 * Copyright 2010-2015 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "DebriefingState.h"
#include "CannotReequipState.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/TextButton.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "NoContainmentState.h"
#include "PromotionsState.h"
#include "../Resource/ResourcePack.h"
#include "../Ruleset/Ruleset.h"
#include "../Ruleset/RuleCountry.h"
#include "../Ruleset/RuleCraft.h"
#include "../Ruleset/RuleItem.h"
#include "../Ruleset/RuleRegion.h"
#include "../Ruleset/Armor.h"
#include "../Savegame/AlienBase.h"
#include "../Savegame/AlienMission.h"
#include "../Savegame/Base.h"
#include "../Savegame/BattleItem.h"
#include "../Savegame/Country.h"
#include "../Savegame/Craft.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/Region.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/SoldierDeath.h"
#include "../Savegame/MissionSite.h"
#include "../Savegame/Tile.h"
#include "../Savegame/Ufo.h"
#include "../Savegame/Vehicle.h"
#include "../Savegame/BaseFacility.h"
#include <sstream>
#include "../Menu/ErrorMessageState.h"
#include "../Menu/MainMenuState.h"
#include "../Interface/Cursor.h"
#include "../Engine/Options.h"
#include "../Basescape/ManageAlienContainmentState.h"
#include "../Engine/Screen.h"
#include "../Basescape/SellState.h"
#include "../Menu/SaveGameState.h"
#include "../Ruleset/AlienDeployment.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Debriefing screen.
 * @param game Pointer to the core game.
 */
DebriefingState::DebriefingState() : _region(0), _country(0), _positiveScore(true), _noContainment(false), _manageContainment(false), _destroyBase(false)
{
	Options::baseXResolution = Options::baseXGeoscape;
	Options::baseYResolution = Options::baseYGeoscape;
	_game->getScreen()->resetDisplay(false);

	// Restore the cursor in case something weird happened
	_game->getCursor()->setVisible(true);
	_limitsEnforced = Options::storageLimitsEnforced ? 1 : 0;

	// Create objects
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(40, 12, 16, 180);
	_txtTitle = new Text(300, 17, 16, 8);
	_txtItem = new Text(180, 9, 16, 24);
	_txtQuantity = new Text(60, 9, 200, 24);
	_txtScore = new Text(55, 9, 270, 24);
	_txtRecovery = new Text(180, 9, 16, 60);
	_txtRating = new Text(200, 9, 64, 180);
	_lstStats = new TextList(290, 80, 16, 32);
	_lstRecovery = new TextList(290, 80, 16, 32);
	_lstTotal = new TextList(290, 9, 16, 12);

	// Set palette
	setInterface("debriefing");

	add(_window, "window", "debriefing");
	add(_btnOk, "button", "debriefing");
	add(_txtTitle, "heading", "debriefing");
	add(_txtItem, "text", "debriefing");
	add(_txtQuantity, "text", "debriefing");
	add(_txtScore, "text", "debriefing");
	add(_txtRecovery, "text", "debriefing");
	add(_txtRating, "text", "debriefing");
	add(_lstStats, "list", "debriefing");
	add(_lstRecovery, "list", "debriefing");
	add(_lstTotal, "totals", "debriefing");

	centerAllSurfaces();

	// Set up objects
	_window->setBackground(_game->getResourcePack()->getSurface("BACK01.SCR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&DebriefingState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&DebriefingState::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&DebriefingState::btnOkClick, Options::keyCancel);

	_txtTitle->setBig();

	_txtItem->setText(tr("STR_LIST_ITEM"));

	_txtQuantity->setText(tr("STR_QUANTITY_UC"));
	_txtQuantity->setAlign(ALIGN_RIGHT);

	_txtScore->setText(tr("STR_SCORE"));

	_lstStats->setColumns(3, 224, 30, 64);
	_lstStats->setDot(true);

	_lstRecovery->setColumns(3, 224, 30, 64);
	_lstRecovery->setDot(true);

	_lstTotal->setColumns(2, 254, 64);
	_lstTotal->setDot(true);

	prepareDebriefing();

	int total = 0, statsY = 0, recoveryY = 0;
	for (std::vector<DebriefingStat*>::iterator i = _stats.begin(); i != _stats.end(); ++i)
	{
		if ((*i)->qty == 0)
			continue;

		std::wostringstream ss, ss2;
		ss << L'\x01' << (*i)->qty << L'\x01';
		ss2 << L'\x01' << (*i)->score;
		total += (*i)->score;
		if ((*i)->recovery)
		{
			_lstRecovery->addRow(3, tr((*i)->item).c_str(), ss.str().c_str(), ss2.str().c_str());
			recoveryY += 8;
		}
		else
		{
			_lstStats->addRow(3, tr((*i)->item).c_str(), ss.str().c_str(), ss2.str().c_str());
			statsY += 8;
		}
	}
	std::wostringstream ss3;
	ss3 << total;
	_lstTotal->addRow(2, tr("STR_TOTAL_UC").c_str(), ss3.str().c_str());

	// add the points to our activity score
	if (_region)
	{
		_region->addActivityXcom(total);
	}
	if (_country)
	{
		_country->addActivityXcom(total);
	}

	if (recoveryY > 0)
	{
		_txtRecovery->setY(_lstStats->getY() + statsY + 5);
		_lstRecovery->setY(_txtRecovery->getY() + 8);
		_lstTotal->setY(_lstRecovery->getY() + recoveryY + 5);
	}
	else
	{
		_txtRecovery->setText(L"");
		_lstTotal->setY(_lstStats->getY() + statsY + 5);
	}

	// Calculate rating
	std::wstring rating;
	if (total <= -200)
	{
		rating = tr("STR_RATING_TERRIBLE");
	}
	else if (total <= 0)
	{
		rating = tr("STR_RATING_POOR");
	}
	else if (total <= 200)
	{
		rating = tr("STR_RATING_OK");
	}
	else if (total <= 500)
	{
		rating = tr("STR_RATING_GOOD");
	}
	else
	{
		rating = tr("STR_RATING_EXCELLENT");
	}
	_txtRating->setText(tr("STR_RATING").arg(rating));
	_positiveScore = (total > 0);
}

/**
 *
 */
DebriefingState::~DebriefingState()
{
	if (_game->isQuitting())
	{
		_game->getSavedGame()->setBattleGame(0);
	}
	for (std::vector<DebriefingStat*>::iterator i = _stats.begin(); i != _stats.end(); ++i)
	{
		delete *i;
	}
	for (std::map<int, RecoveryItem*>::iterator i = _recoveryStats.begin(); i != _recoveryStats.end(); ++i)
	{
		delete i->second;
	}
	_recoveryStats.clear();
	_rounds.clear();
}

void DebriefingState::init()
{
	State::init();
	if (_positiveScore)
	{
		_game->getResourcePack()->playMusic(ResourcePack::DEBRIEF_MUSIC_GOOD);
	}
	else
	{
		_game->getResourcePack()->playMusic(ResourcePack::DEBRIEF_MUSIC_BAD);
	}
}
/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void DebriefingState::btnOkClick(Action *)
{
	std::vector<Soldier*> participants;
	for (std::vector<BattleUnit*>::const_iterator i = _game->getSavedGame()->getSavedBattle()->getUnits()->begin();
		i != _game->getSavedGame()->getSavedBattle()->getUnits()->end(); ++i)
	{
		if ((*i)->getGeoscapeSoldier())
		{
			participants.push_back((*i)->getGeoscapeSoldier());
		}
	}
	_game->getSavedGame()->setBattleGame(0);
	_game->popState();
	if (_game->getSavedGame()->getMonthsPassed() == -1)
	{
		_game->setState(new MainMenuState);
	}
	else
	{
		if (!_destroyBase)
		{
			if (_game->getSavedGame()->handlePromotions(participants))
			{
				_game->pushState(new PromotionsState);
			}
			if (!_missingItems.empty())
			{
				_game->pushState(new CannotReequipState(_missingItems));
			}
			if (_noContainment)
			{
				_game->pushState(new NoContainmentState);
			}
			else if (_manageContainment)
			{
				_game->pushState(new ManageAlienContainmentState(_base, OPT_BATTLESCAPE));
				_game->pushState(new ErrorMessageState(tr("STR_CONTAINMENT_EXCEEDED").arg(_base->getName()).c_str(), _palette, _game->getRuleset()->getInterface("debriefing")->getElement("errorMessage")->color, "BACK01.SCR", _game->getRuleset()->getInterface("debriefing")->getElement("errorPalette")->color));
			}
			if (!_manageContainment && Options::storageLimitsEnforced && _base->storesOverfull())
			{
				_game->pushState(new SellState(_base, OPT_BATTLESCAPE));
				_game->pushState(new ErrorMessageState(tr("STR_STORAGE_EXCEEDED").arg(_base->getName()).c_str(), _palette, _game->getRuleset()->getInterface("debriefing")->getElement("errorMessage")->color, "BACK01.SCR", _game->getRuleset()->getInterface("debriefing")->getElement("errorPalette")->color));
			}
		}

		// Autosave after mission
		if (_game->getSavedGame()->isIronman())
		{
			_game->pushState(new SaveGameState(OPT_GEOSCAPE, SAVE_IRONMAN, _palette));
		}
		else if (Options::autosave)
		{
			_game->pushState(new SaveGameState(OPT_GEOSCAPE, SAVE_AUTO_GEOSCAPE, _palette));
		}
	}
}

/**
 * Adds to the debriefing stats.
 * @param name The untranslated name of the stat.
 * @param quantity The quantity to add.
 * @param score The score to add.
 */
void DebriefingState::addStat(const std::string &name, int quantity, int score)
{
	for (std::vector<DebriefingStat*>::iterator i = _stats.begin(); i != _stats.end(); ++i)
	{
		if ((*i)->item == name)
		{
			(*i)->qty = (*i)->qty + quantity;
			(*i)->score = (*i)->score + score;
			break;
		}
	}
}

/**
 * Clears the alien base from supply missions that use it.
 */
class ClearAlienBase: public std::unary_function<AlienMission *, void>
{
public:
	/// Remembers the base.
	ClearAlienBase(const AlienBase *base) : _base(base) { /* Empty by design. */ }
	/// Clears the base if required.
	void operator()(AlienMission *am) const;
private:
	const AlienBase *_base;
};

/**
 * Removes the association between the alien mission and the alien base,
 * if one existed.
 * @param am Pointer to the alien mission.
 */
void ClearAlienBase::operator()(AlienMission *am) const
{
	if (am->getAlienBase() == _base)
	{
		am->setAlienBase(0);
	}
}

/**
 * Prepares debriefing: gathers Aliens, Corpses, Artefacts, UFO Components.
 * Adds the items to the craft.
 * Also calculates the soldiers experience, and possible promotions.
 * If aborted, only the things on the exit area are recovered.
 */
void DebriefingState::prepareDebriefing()
{
	for (std::vector<std::string>::const_iterator i = _game->getRuleset()->getItemsList().begin(); i != _game->getRuleset()->getItemsList().end(); ++i)
	{
		if (_game->getRuleset()->getItem(*i)->getSpecialType() > 1)
		{
			RecoveryItem *item = new RecoveryItem();
			item->name = *i;
			item->value = _game->getRuleset()->getItem(*i)->getRecoveryPoints();
			_recoveryStats[_game->getRuleset()->getItem(*i)->getSpecialType()] = item;
		}
	}

	SavedGame *save = _game->getSavedGame();
	SavedBattleGame *battle = save->getSavedBattle();
	AlienDeployment *deployment = _game->getRuleset()->getDeployment(battle->getMissionType());

	bool aborted = battle->isAborted();
	bool success = !aborted || battle->allObjectivesDestroyed();
	Craft* craft = 0;
	std::vector<Craft*>::iterator craftIterator;
	Base* base = 0;
	std::string target;

	int playerInExitArea = 0; // if this stays 0 the craft is lost...
	int playersSurvived = 0; // if this stays 0 the craft is lost...
	int playersUnconscious = 0;


	_stats.push_back(new DebriefingStat("STR_ALIENS_KILLED", false));
	_stats.push_back(new DebriefingStat("STR_ALIEN_CORPSES_RECOVERED", false));
	_stats.push_back(new DebriefingStat("STR_LIVE_ALIENS_RECOVERED", false));
	_stats.push_back(new DebriefingStat("STR_ALIEN_ARTIFACTS_RECOVERED", false));

	std::string objectiveCompleteText, objectiveFailedText;
	int objectiveCompleteScore = 0, objectiveFailedScore = 0;
	if (deployment)
	{
		if (deployment->getObjectiveCompleteInfo(objectiveCompleteText, objectiveCompleteScore))
		{
			_stats.push_back(new DebriefingStat(objectiveCompleteText, false));
		}
		if (deployment->getObjectiveFailedInfo(objectiveFailedText, objectiveFailedScore))
		{
			_stats.push_back(new DebriefingStat(objectiveFailedText, false));
		}
	}

	_stats.push_back(new DebriefingStat("STR_CIVILIANS_KILLED_BY_ALIENS", false));
	_stats.push_back(new DebriefingStat("STR_CIVILIANS_KILLED_BY_XCOM_OPERATIVES", false));
	_stats.push_back(new DebriefingStat("STR_CIVILIANS_SAVED", false));
	_stats.push_back(new DebriefingStat("STR_XCOM_OPERATIVES_KILLED", false));
	//_stats.push_back(new DebriefingStat("STR_XCOM_OPERATIVES_RETIRED_THROUGH_INJURY", false));
	_stats.push_back(new DebriefingStat("STR_XCOM_OPERATIVES_MISSING_IN_ACTION", false));
	_stats.push_back(new DebriefingStat("STR_TANKS_DESTROYED", false));
	_stats.push_back(new DebriefingStat("STR_XCOM_CRAFT_LOST", false));

	for (std::map<int, RecoveryItem*>::const_iterator i = _recoveryStats.begin(); i != _recoveryStats.end(); ++i)
	{
		_stats.push_back(new DebriefingStat((*i).second->name, true));
	}

	_stats.push_back(new DebriefingStat(_game->getRuleset()->getAlienFuelName(), true));

	for (std::vector<Base*>::iterator i = save->getBases()->begin(); i != save->getBases()->end(); ++i)
	{
		// in case we have a craft - check which craft it is about
		for (std::vector<Craft*>::iterator j = (*i)->getCrafts()->begin(); j != (*i)->getCrafts()->end(); ++j)
		{
			if ((*j)->isInBattlescape())
			{
				for (std::vector<Region*>::iterator k = _game->getSavedGame()->getRegions()->begin(); k != _game->getSavedGame()->getRegions()->end(); ++k)
				{
					if ((*k)->getRules()->insideRegion((*j)->getLongitude(), (*j)->getLatitude()))
					{
						_region = (*k);
						break;
					}
				}
				for (std::vector<Country*>::iterator k = _game->getSavedGame()->getCountries()->begin(); k != _game->getSavedGame()->getCountries()->end(); ++k)
				{
					if ((*k)->getRules()->insideCountry((*j)->getLongitude(), (*j)->getLatitude()))
					{
						_country = (*k);
						break;
					}
				}
				craft = (*j);
				base = (*i);
				craftIterator = j;
				if (dynamic_cast<Ufo*>(craft->getDestination()))
				{
					target = "STR_UFO";
				}
				else if (dynamic_cast<AlienBase*>(craft->getDestination()))
				{
					target = "STR_ALIEN_BASE";
				}
				else if (dynamic_cast<MissionSite*>(craft->getDestination()))
				{
					target = "STR_MISSION_SITE";
				}
				craft->returnToBase();
				craft->setMissionComplete(true);
				craft->setInBattlescape(false);
			}
			else if ((*j)->getDestination() != 0)
			{
				Ufo* u = dynamic_cast<Ufo*>((*j)->getDestination());
				if (u != 0 && u->isInBattlescape())
				{
					(*j)->returnToBase();
				}
			}
		}
		// in case we DON'T have a craft (base defense)
		if ((*i)->isInBattlescape())
		{
			base = (*i);
			target = "STR_BASE";
			base->setInBattlescape(false);
			base->cleanupDefenses(false);
			for (std::vector<Region*>::iterator k = _game->getSavedGame()->getRegions()->begin(); k != _game->getSavedGame()->getRegions()->end(); ++k)
			{
				if ((*k)->getRules()->insideRegion(base->getLongitude(), base->getLatitude()))
				{
					_region = (*k);
					break;
				}
			}
			for (std::vector<Country*>::iterator k = _game->getSavedGame()->getCountries()->begin(); k != _game->getSavedGame()->getCountries()->end(); ++k)
			{
				if ((*k)->getRules()->insideCountry(base->getLongitude(), base->getLatitude()))
				{
					_country = (*k);
					break;
				}
			}
			if (aborted)
			{
				_destroyBase = true;
			}
			for (std::vector<BaseFacility*>::iterator k = base->getFacilities()->begin(); k != base->getFacilities()->end();)
			{
				// this facility was demolished
				if (battle->getModuleMap()[(*k)->getX()][(*k)->getY()].second == 0)
				{
					base->destroyFacility(k);
				}
				else
				{
					++k;
				}
			}
			// this may cause the base to become disjointed, destroy the disconnected parts.
			base->destroyDisconnectedFacilities();
		}
	}

	_base = base;

	// UFO crash/landing site disappears
	for (std::vector<Ufo*>::iterator i = save->getUfos()->begin(); i != save->getUfos()->end(); ++i)
	{
		if ((*i)->isInBattlescape())
		{
			_txtRecovery->setText(tr("STR_UFO_RECOVERY"));
			(*i)->setInBattlescape(false);
			if ((*i)->getStatus() == Ufo::LANDED && aborted)
			{
				 (*i)->setSecondsRemaining(5);
			}
			else if ((*i)->getStatus() == Ufo::CRASHED || !aborted)
			{
				delete *i;
				save->getUfos()->erase(i);
				break;
			}
		}
	}
	
	// mission site disappears (even when you abort)
	for (std::vector<MissionSite*>::iterator i = save->getMissionSites()->begin(); i != save->getMissionSites()->end(); ++i)
	{
		if ((*i)->isInBattlescape())
		{
			delete *i;
			save->getMissionSites()->erase(i);
			break;
		}
	}

	// lets see what happens with units

	// first, we evaluate how many surviving XCom units there are, and how many are conscious
	for (std::vector<BattleUnit*>::iterator j = battle->getUnits()->begin(); j != battle->getUnits()->end(); ++j)
	{
		if ((*j)->getOriginalFaction() == FACTION_PLAYER && (*j)->getStatus() != STATUS_DEAD)
		{
			if ((*j)->getStatus() == STATUS_UNCONSCIOUS || (*j)->getFaction() == FACTION_HOSTILE)
			{
				playersUnconscious++;
			}
			playersSurvived++;
		}
	}
	// if all our men are unconscious, the aliens get to have their way with them.
	if (playersUnconscious == playersSurvived)
	{
		playersSurvived = 0;
		for (std::vector<BattleUnit*>::iterator j = battle->getUnits()->begin(); j != battle->getUnits()->end(); ++j)
		{
			if ((*j)->getOriginalFaction() == FACTION_PLAYER && (*j)->getStatus() != STATUS_DEAD)
			{
				(*j)->instaKill();
			}
		}
	}

	// alien base disappears (if you didn't abort)
	for (std::vector<AlienBase*>::iterator i = save->getAlienBases()->begin(); i != save->getAlienBases()->end(); ++i)
	{
		if ((*i)->isInBattlescape())
		{
			_txtRecovery->setText(tr("STR_ALIEN_BASE_RECOVERY"));
			bool destroyAlienBase = true;

			if (aborted || playersSurvived == 0)
			{
				if (!battle->allObjectivesDestroyed())
					destroyAlienBase = false;
			}
			
			if (deployment && !deployment->getNextStage().empty())
			{
				destroyAlienBase = false;
			}
			success = destroyAlienBase;
			if (destroyAlienBase)
			{
				if (objectiveCompleteText != "")
				{
					addStat(objectiveCompleteText, 1, objectiveCompleteScore);
				}
				// Take care to remove supply missions for this base.
				std::for_each(save->getAlienMissions().begin(), save->getAlienMissions().end(),
							ClearAlienBase(*i));
				delete *i;
				save->getAlienBases()->erase(i);
				break;
			}
			else
			{
				(*i)->setInBattlescape(false);
				break;
			}
		}
	}

	// time to care for units.
	for (std::vector<BattleUnit*>::iterator j = battle->getUnits()->begin(); j != battle->getUnits()->end(); ++j)
	{
		UnitStatus status = (*j)->getStatus();
		UnitFaction faction = (*j)->getFaction();
		UnitFaction oldFaction = (*j)->getOriginalFaction();
		int value = (*j)->getValue();
		Soldier *soldier = save->getSoldier((*j)->getId());
		std::string type = (*j)->getType();
		if (!(*j)->getSpawnUnit().empty())
		{
			type = (*j)->getSpawnUnit();
		}

		if (!(*j)->getTile())
		{
			Position pos = (*j)->getPosition();
			if (pos == Position(-1, -1, -1))
			{
				for (std::vector<BattleItem*>::iterator k = battle->getItems()->begin(); k != battle->getItems()->end(); ++k)
				{
					if ((*k)->getUnit() && (*k)->getUnit() == *j)
					{
						if ((*k)->getOwner())
						{
							pos = (*k)->getOwner()->getPosition();
						}
						else if ((*k)->getTile())
						{
							pos = (*k)->getTile()->getPosition();
						}
					}
				}
			}
			(*j)->setTile(battle->getTile(pos));
		}

		if (status == STATUS_DEAD)
		{ // so this is a dead unit
			if (oldFaction == FACTION_HOSTILE && (*j)->killedBy() == FACTION_PLAYER)
			{
				addStat("STR_ALIENS_KILLED", 1, value);
			}
			else if (oldFaction == FACTION_PLAYER)
			{
				if (soldier != 0)
				{
					addStat("STR_XCOM_OPERATIVES_KILLED", 1, -value);
					for (std::vector<Soldier*>::iterator i = base->getSoldiers()->begin(); i != base->getSoldiers()->end(); ++i)
					{
						if ((*i) == soldier)
						{
							(*j)->updateGeoscapeStats(*i);
							SoldierDeath *death = new SoldierDeath();
							death->setTime(*save->getTime());
							(*i)->die(death);
							save->getDeadSoldiers()->push_back(*i);
							base->getSoldiers()->erase(i);
							break;
						}
					}
				}
				else
				{ // non soldier player = tank
					addStat("STR_TANKS_DESTROYED", 1, -value);
				}
			}
			else if (oldFaction == FACTION_NEUTRAL)
			{
				if ((*j)->killedBy() == FACTION_PLAYER)
					addStat("STR_CIVILIANS_KILLED_BY_XCOM_OPERATIVES", 1, -(*j)->getValue() - (2 * ((*j)->getValue() / 3)));
				else // if civilians happen to kill themselves XCOM shouldn't get penalty for it
					addStat("STR_CIVILIANS_KILLED_BY_ALIENS", 1, -(*j)->getValue());
			}
		}
		else
		{ // so this unit is not dead...
			if (oldFaction == FACTION_PLAYER)
			{
				if ((((*j)->isInExitArea() || (*j)->getStatus() == STATUS_IGNORE_ME) && (battle->getMissionType() != "STR_BASE_DEFENSE" || success)) || !aborted)
				{ // so game is not aborted or aborted and unit is on exit area
					(*j)->postMissionProcedures(save);
					playerInExitArea++;
					if (soldier != 0)
					{
						recoverItems((*j)->getInventory(), base);
						// calculate new statString
						soldier->calcStatString(_game->getRuleset()->getStatStrings(), (Options::psiStrengthEval && _game->getSavedGame()->isResearched(_game->getRuleset()->getPsiRequirements())));
					}
					else
					{ // non soldier player = tank
						base->getItems()->addItem(type);
						RuleItem *tankRule = _game->getRuleset()->getItem(type);
						if ((*j)->getItem("STR_RIGHT_HAND"))
						{
							BattleItem *ammoItem = (*j)->getItem("STR_RIGHT_HAND")->getAmmoItem();
							if (!tankRule->getCompatibleAmmo()->empty() && ammoItem != 0 && ammoItem->getAmmoQuantity() > 0)
							{
								int total = ammoItem->getAmmoQuantity();

								if (tankRule->getClipSize()) // meaning this tank can store multiple clips
								{
									total /= ammoItem->getRules()->getClipSize();
								}

								base->getItems()->addItem(tankRule->getCompatibleAmmo()->front(), total);
							}
						}
						if ((*j)->getItem("STR_LEFT_HAND"))
						{
							RuleItem *secondaryRule = (*j)->getItem("STR_LEFT_HAND")->getRules();
							BattleItem *ammoItem = (*j)->getItem("STR_LEFT_HAND")->getAmmoItem();
							if (!secondaryRule->getCompatibleAmmo()->empty() && ammoItem != 0 && ammoItem->getAmmoQuantity() > 0)
							{
								int total = ammoItem->getAmmoQuantity();

								if (secondaryRule->getClipSize()) // meaning this tank can store multiple clips
								{
									total /= ammoItem->getRules()->getClipSize();
								}

								base->getItems()->addItem(secondaryRule->getCompatibleAmmo()->front(), total);
							}
						}
					}
				}
				else
				{ // so game is aborted and unit is not on exit area
					addStat("STR_XCOM_OPERATIVES_MISSING_IN_ACTION", 1, -value);
					if (soldier != 0)
					{
						for (std::vector<Soldier*>::iterator i = base->getSoldiers()->begin(); i != base->getSoldiers()->end(); ++i)
						{
							if ((*i) == soldier)
							{
								(*j)->updateGeoscapeStats(*i);
								SoldierDeath *death = new SoldierDeath();
								death->setTime(*save->getTime());
								(*i)->die(death);
								save->getDeadSoldiers()->push_back(*i);
								base->getSoldiers()->erase(i);
								break;
							}
						}
					}
				}
			}
			else if (oldFaction == FACTION_HOSTILE && (!aborted || (*j)->isInExitArea())
				// mind controlled units may as well count as unconscious
				&& faction == FACTION_PLAYER && !(*j)->isOut())
			{
				for (std::vector<BattleItem*>::iterator k = (*j)->getInventory()->begin(); k != (*j)->getInventory()->end(); ++k)
				{
					if (!(*k)->getRules()->isFixed())
					{
						(*j)->getTile()->addItem(*k, _game->getRuleset()->getInventory("STR_GROUND"));
					}
				}
				recoverAlien(*j, base);
			}
			else if (oldFaction == FACTION_NEUTRAL)
			{
				// if mission fails, all civilians die
				if (aborted || playersSurvived == 0)
				{
					addStat("STR_CIVILIANS_KILLED_BY_ALIENS", 1, -(*j)->getValue());
				}
				else
				{
					addStat("STR_CIVILIANS_SAVED", 1, (*j)->getValue());
				}
			}
		}
	}

	if (craft != 0 && ((playerInExitArea == 0 && aborted) || (playersSurvived == 0)))
	{
		addStat("STR_XCOM_CRAFT_LOST", 1, -craft->getRules()->getScore());
		for (std::vector<Soldier*>::iterator i = base->getSoldiers()->begin(); i != base->getSoldiers()->end();)
		{
			if ((*i)->getCraft() == craft)
			{
				delete (*i);
				i = base->getSoldiers()->erase(i);
			}
			else
			{
				++i;
			}
		}
		// Since this is not a base defense mission, we can safely erase the craft,
		// without worrying it's vehicles' destructor calling double (on base defense missions
		// all vehicle object in the craft is also referenced by base->getVehicles() !!)
		delete craft;
		craft = 0; // To avoid a crash down there!!
		base->getCrafts()->erase(craftIterator);
		_txtTitle->setText(tr("STR_CRAFT_IS_LOST"));
		return;
	}
	if (aborted && target == "STR_BASE" && !base->getCrafts()->empty())
	{
		for (std::vector<Craft*>::iterator i = base->getCrafts()->begin(); i != base->getCrafts()->end(); ++i)
		{
			addStat("STR_XCOM_CRAFT_LOST", 1, -(*i)->getRules()->getScore());
		}
	}
	if ((!aborted || success) && playersSurvived > 0) 	// RECOVER UFO : run through all tiles to recover UFO components and items
	{
		if (target == "STR_BASE")
		{
			_txtTitle->setText(tr("STR_BASE_IS_SAVED"));
		}
		else if (target == "STR_UFO")
		{
			_txtTitle->setText(tr("STR_UFO_IS_RECOVERED")); 
		}
		else if (target == "STR_ALIEN_BASE")
		{
			_txtTitle->setText(tr("STR_ALIEN_BASE_DESTROYED"));
		}
		else
		{
			_txtTitle->setText(tr("STR_ALIENS_DEFEATED"));
			if (objectiveCompleteText != "")
			{
				addStat(objectiveCompleteText, 1, objectiveCompleteScore);
			}
		}

		if (!aborted)
		{
			// if this was a 2-stage mission, and we didn't abort (ie: we have time to clean up)
			// we can recover items from the earlier stages as well
			recoverItems(battle->getConditionalRecoveredItems(), base);

			for (int i = 0; i < battle->getMapSizeXYZ(); ++i)
			{
				// get recoverable map data objects from the battlescape map
				for (int part = 0; part < 4; ++part)
				{
					if (battle->getTiles()[i]->getMapData(part))
					{
						size_t specialType = battle->getTiles()[i]->getMapData(part)->getSpecialType();
						if (_recoveryStats.find(specialType) != _recoveryStats.end())
						{
							addStat(_recoveryStats[specialType]->name, 1, _recoveryStats[specialType]->value);
						}
					}
				}
				// recover items from the floor
				recoverItems(battle->getTiles()[i]->getInventory(), base);
			}
		}
		else
		{
			for (int i = 0; i < battle->getMapSizeXYZ(); ++i)
			{
				if (battle->getTiles()[i]->getMapData(O_FLOOR) && (battle->getTiles()[i]->getMapData(O_FLOOR)->getSpecialType() == START_POINT))
					recoverItems(battle->getTiles()[i]->getInventory(), base);
			}
		}
	}
	else
	{
		if (target == "STR_BASE")
		{
			_txtTitle->setText(tr("STR_BASE_IS_LOST"));
			_destroyBase = true;
		}
		else if (target == "STR_UFO")
		{
			_txtTitle->setText(tr("STR_UFO_IS_NOT_RECOVERED"));
		}
		else if (target == "STR_ALIEN_BASE")
		{
			_txtTitle->setText(tr("STR_ALIEN_BASE_STILL_INTACT"));
		}
		else
		{
			_txtTitle->setText(tr("STR_TERROR_CONTINUES"));
			if (objectiveFailedText != "")
			{
				addStat(objectiveFailedText, 1, objectiveFailedScore);
			}
		}

		if (playersSurvived > 0 && !_destroyBase)
		{
			// recover items from the craft floor
			for (int i = 0; i < battle->getMapSizeXYZ(); ++i)
			{
				if (battle->getTiles()[i]->getMapData(O_FLOOR) && (battle->getTiles()[i]->getMapData(O_FLOOR)->getSpecialType() == START_POINT))
					recoverItems(battle->getTiles()[i]->getInventory(), base);
			}
		}
	}

	// calculate the clips for each type based on the recovered rounds.
	for (std::map<RuleItem*, int>::const_iterator i = _rounds.begin(); i != _rounds.end(); ++i)
	{
		int total_clips = i->second / i->first->getClipSize();
		if (total_clips > 0)
			base->getItems()->addItem(i->first->getType(), total_clips);
	}

	// recover all our goodies
	if (playersSurvived > 0)
	{
		int aadivider = (target == "STR_ALIEN_BASE") ? 150 : 10;
		for (std::vector<DebriefingStat*>::iterator i = _stats.begin(); i != _stats.end(); ++i)
		{
			// alien alloys recovery values are divided by 10 or divided by 150 in case of an alien base
			if ((*i)->item == _recoveryStats[ALIEN_ALLOYS]->name)
			{
				(*i)->qty = (*i)->qty / aadivider;
				(*i)->score = (*i)->score / aadivider;
			}

			// recoverable battlescape tiles are now converted to items and put in base inventory
			if ((*i)->recovery && (*i)->qty > 0)
			{
				base->getItems()->addItem((*i)->item, (*i)->qty);
			}
		}

		// assuming this was a multi-stage mission,
		// recover everything that was in the craft in the previous stage
		recoverItems(battle->getGuaranteedRecoveredItems(), base);
	}

	// reequip craft after a non-base-defense mission (of course only if it's not lost already (that case craft=0))
	if (craft)
	{
		reequipCraft(base, craft, true);
	}

	if (target == "STR_BASE")
	{
		if (!_destroyBase)
		{
			// reequip crafts (only those on the base) after a base defense mission
			for (std::vector<Craft*>::iterator c = base->getCrafts()->begin(); c != base->getCrafts()->end(); ++c)
			{
				if ((*c)->getStatus() != "STR_OUT")
					reequipCraft(base, *c, false);
			}
			// Clear base->getVehicles() objects, they aren't needed anymore.
			for (std::vector<Vehicle*>::iterator i = base->getVehicles()->begin(); i != base->getVehicles()->end(); ++i)
				delete (*i);
			base->getVehicles()->clear();
		}
		else if (_game->getSavedGame()->getMonthsPassed() != -1)
		{
			for (std::vector<Base*>::iterator i = _game->getSavedGame()->getBases()->begin(); i != _game->getSavedGame()->getBases()->end(); ++i)
			{
				if ((*i) == base)
				{

					delete (*i);
					_game->getSavedGame()->getBases()->erase(i);
					break;
				}
			}
		}

		if (_region)
		{
			AlienMission* am = _game->getSavedGame()->findAlienMission(_region->getRules()->getType(), OBJECTIVE_RETALIATION);
			for (std::vector<Ufo*>::iterator i = _game->getSavedGame()->getUfos()->begin(); i != _game->getSavedGame()->getUfos()->end();)
			{
				if ((*i)->getMission() == am)
				{
					delete *i;
					i = _game->getSavedGame()->getUfos()->erase(i);
				}
				else
				{
					++i;
				}
			}
			for (std::vector<AlienMission*>::iterator i = _game->getSavedGame()->getAlienMissions().begin();
				i != _game->getSavedGame()->getAlienMissions().end(); ++i)
			{
				if ((AlienMission*)(*i) == am)
				{
					delete (*i);
					_game->getSavedGame()->getAlienMissions().erase(i);
					break;
				}
			}
		}
	}
}

/**
 * Reequips a craft after a mission.
 * @param base Base to reequip from.
 * @param craft Craft to reequip.
 * @param vehicleItemsCanBeDestroyed Whether we can destroy the vehicles on the craft.
 */
void DebriefingState::reequipCraft(Base *base, Craft *craft, bool vehicleItemsCanBeDestroyed)
{
	std::map<std::string, int> craftItems = *craft->getItems()->getContents();
	for (std::map<std::string, int>::iterator i = craftItems.begin(); i != craftItems.end(); ++i)
	{
		int qty = base->getItems()->getItem(i->first);
		if (qty >= i->second)
		{
			base->getItems()->removeItem(i->first, i->second);
		}
		else
		{
			int missing = i->second - qty;
			base->getItems()->removeItem(i->first, qty);
			craft->getItems()->removeItem(i->first, missing);
			ReequipStat stat = {i->first, missing, craft->getName(_game->getLanguage())};
			_missingItems.push_back(stat);
		}
	}

	// Now let's see the vehicles
	ItemContainer craftVehicles;
	for (std::vector<Vehicle*>::iterator i = craft->getVehicles()->begin(); i != craft->getVehicles()->end(); ++i)
		craftVehicles.addItem((*i)->getRules()->getType());
	// Now we know how many vehicles (separated by types) we have to read
	// Erase the current vehicles, because we have to reAdd them (cause we want to redistribute their ammo)
	if (vehicleItemsCanBeDestroyed)
		for (std::vector<Vehicle*>::iterator i = craft->getVehicles()->begin(); i != craft->getVehicles()->end(); ++i)
			delete (*i);
	craft->getVehicles()->clear();
	// Ok, now read those vehicles
	for (std::map<std::string, int>::iterator i = craftVehicles.getContents()->begin(); i != craftVehicles.getContents()->end(); ++i)
	{
		int qty = base->getItems()->getItem(i->first);
		RuleItem *tankRule = _game->getRuleset()->getItem(i->first);
		int size = 4;
		if (_game->getRuleset()->getUnit(tankRule->getType()))
		{
			size = _game->getRuleset()->getArmor(_game->getRuleset()->getUnit(tankRule->getType())->getArmor())->getSize();
			size *= size;
		}
		int canBeAdded = std::min(qty, i->second);
		if (qty < i->second)
		{ // missing tanks
			int missing = i->second - qty;
			ReequipStat stat = {i->first, missing, craft->getName(_game->getLanguage())};
			_missingItems.push_back(stat);
		}
		if (tankRule->getCompatibleAmmo()->empty())
		{ // so this tank does NOT require ammo
			for (int j = 0; j < canBeAdded; ++j)
				craft->getVehicles()->push_back(new Vehicle(tankRule, tankRule->getClipSize(), size));
			base->getItems()->removeItem(i->first, canBeAdded);
		}
		else
		{ // so this tank requires ammo
			RuleItem *ammo = _game->getRuleset()->getItem(tankRule->getCompatibleAmmo()->front());
			int ammoPerVehicle = ammo->getClipSize();
			int baqty = base->getItems()->getItem(ammo->getType()); // Ammo Quantity for this vehicle-type on the base
			if (baqty < i->second * ammoPerVehicle)
			{ // missing ammo
				int missing = (i->second * ammoPerVehicle) - baqty;
				ReequipStat stat = {ammo->getType(), missing, craft->getName(_game->getLanguage())};
				_missingItems.push_back(stat);
			}
			canBeAdded = std::min(canBeAdded, baqty / ammoPerVehicle);
			if (canBeAdded > 0)
			{
				for (int j = 0; j < canBeAdded; ++j)
				{
					craft->getVehicles()->push_back(new Vehicle(tankRule, ammoPerVehicle, size));
					base->getItems()->removeItem(ammo->getType(), ammoPerVehicle);
				}
				base->getItems()->removeItem(i->first, canBeAdded);
			}
		}
	}
}

/**
 * Recovers items from the battlescape.
 *
 * Converts the battlescape inventory into a geoscape itemcontainer.
 * @param from Items recovered from the battlescape.
 * @param base Base to add items to.
 */
void DebriefingState::recoverItems(std::vector<BattleItem*> *from, Base *base)
{
	for (std::vector<BattleItem*>::iterator it = from->begin(); it != from->end(); ++it)
	{
		if ((*it)->getRules()->getName() == _game->getRuleset()->getAlienFuelName())
		{
			// special case of an item counted as a stat
			addStat(_game->getRuleset()->getAlienFuelName(), _game->getRuleset()->getAlienFuelQuantity(), 5);
		}
		else
		{
			if ((*it)->getRules()->getRecoveryPoints() && !(*it)->getXCOMProperty())
			{
				if ((*it)->getRules()->getBattleType() == BT_CORPSE && (*it)->getUnit()->getStatus() == STATUS_DEAD)
				{
					addStat("STR_ALIEN_CORPSES_RECOVERED", 1, (*it)->getUnit()->getValue());
					base->getItems()->addItem((*it)->getUnit()->getArmor()->getCorpseGeoscape(), 1);
				}
				else if ((*it)->getRules()->getBattleType() == BT_CORPSE)
				{
					// it's unconscious
					if ((*it)->getUnit()->getStatus() == STATUS_UNCONSCIOUS ||
						// or it's in timeout because it's unconscious from the previous stage
						// units can be in timeout and alive, and we assume they flee.
						((*it)->getUnit()->getStatus() == STATUS_IGNORE_ME &&
						(*it)->getUnit()->getHealth() > 0 &&
						(*it)->getUnit()->getHealth() < (*it)->getUnit()->getStunlevel()))
					{
						if ((*it)->getUnit()->getOriginalFaction() == FACTION_HOSTILE)
						{
							recoverAlien((*it)->getUnit(), base);
						}
						else if ((*it)->getUnit()->getOriginalFaction() == FACTION_NEUTRAL)
						{
							addStat("STR_CIVILIANS_SAVED", 1, (*it)->getUnit()->getValue());
						}
					}
				}
				// only "recover" unresearched items
				else if (!_game->getSavedGame()->isResearched((*it)->getRules()->getType()))
				{
					addStat("STR_ALIEN_ARTIFACTS_RECOVERED", 1, (*it)->getRules()->getRecoveryPoints());
				}
			}

			// put items back in the base
			if (!(*it)->getRules()->isFixed() && (*it)->getRules()->isRecoverable())
			{
				switch ((*it)->getRules()->getBattleType())
				{
					case BT_CORPSE:
						break;
					case BT_AMMO:
						// It's a clip, count any rounds left.
						_rounds[(*it)->getRules()] += (*it)->getAmmoQuantity();
						break;
					case BT_FIREARM:
					case BT_MELEE:
						// It's a weapon, count any rounds left in the clip.
						{
							BattleItem *clip = (*it)->getAmmoItem();
							if (clip && clip->getRules()->getClipSize() > 0 && clip != *it)
							{
								_rounds[clip->getRules()] += clip->getAmmoQuantity();
							}
						}
						// Fall-through, to recover the weapon itself.
					default:
						base->getItems()->addItem((*it)->getRules()->getType(), 1);
				}
			}
		}
	}
}

/**
 * Recovers a live alien from the battlescape.
 * @param from Battle unit to recover.
 * @param base Base to add items to.
 */
void DebriefingState::recoverAlien(BattleUnit *from, Base *base)
{
	std::string type = from->getType();
	if (base->getAvailableContainment() == 0)
	{
		_noContainment = true;
		addStat("STR_ALIEN_CORPSES_RECOVERED", 1, from->getValue());

		std::string corpseItem = from->getArmor()->getCorpseGeoscape();
		if (!from->getSpawnUnit().empty())
		{
			corpseItem = _game->getRuleset()->getArmor(_game->getRuleset()->getUnit(from->getSpawnUnit())->getArmor())->getCorpseGeoscape();
		}
		base->getItems()->addItem(corpseItem, 1);
	}
	else
	{
		RuleResearch *research = _game->getRuleset()->getResearch(type);
		if (research != 0 && !_game->getSavedGame()->isResearched(type))
		{
			// more points if it's not researched
			addStat("STR_LIVE_ALIENS_RECOVERED", 1, from->getValue() * 2);
		}
		else
		{
			// 10 points for recovery
			addStat("STR_LIVE_ALIENS_RECOVERED", 1, 10);
		}

		base->getItems()->addItem(type, 1);
		_manageContainment = base->getAvailableContainment() - (base->getUsedContainment() * _limitsEnforced) < 0;
	}
}

}
