/*****************************************************************************
** FEATS.C                                                                  **
** Source code for the Gates of Krynn Feats System.                         **
** Initial code by Paladine (Stephen Squires)                               **
** Created Thursday, September 5, 2002                                      **
**                                                                          **
*****************************************************************************/

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
//#include "deities.h"
#include "utils.h"
#include "comm.h"
#include "spells.h"
#include "handler.h"
#include "db.h"
#include "constants.h"
#include "interpreter.h"
#include "dg_scripts.h"
#include "modify.h"
#include "feats.h"
#include "class.h"
#include "mud_event.h"

#undef COMPILE_D20_FEATS

/* Local Functions */
void list_class_feats(struct char_data *ch);
void assign_feats(void);
void feato(int featnum, char *name, int in_game, int can_learn, int can_stack, int feat_type, char *prerequisites, char *description);

void list_feats(struct char_data *ch, char *arg, int list_type); 

int compare_feats(const void *x, const void *y);
void sort_feats(void);	
int find_feat_num(char *name);

/*  Prerequisite definition procedures */
void feat_prereq_attribute(int featnum, int attribute, int value);
void feat_prereq_class_level(int featnum, int cl, int level);
void feat_prereq_feat(int featnum, int pfeatnum, int ranks);
void feat_prereq_ability(int featnum, int ability, int ranks);
void feat_prereq_spellcasting(int featnum, int casting_type, int prep_type, int circle);
void feat_prereq_race(int featnum, int race);

void load_weapons(void);
void load_armor(void);

int has_combat_feat(struct char_data *ch, int i, int j);
int has_feat(struct char_data *ch, int featnum);
/*
void set_feat(struct char_data *ch, int i, int j);
void display_levelup_weapons(struct char_data *ch);
int has_weapon_feat(struct char_data *ch, int i, int j);
int has_weapon_feat_full(struct char_data *ch, int i, int j, int display);
*/

/* Global Variables and Structures */
struct feat_info feat_list[NUM_FEATS];
int feat_sort_info[MAX_FEATS];
char buf3[MAX_STRING_LENGTH];
char buf4[MAX_STRING_LENGTH];
struct armor_table armor_list[NUM_SPEC_ARMOR_TYPES];
struct weapon_table weapon_list[NUM_WEAPON_TYPES];
const char *weapon_type[NUM_WEAPON_TYPES];

/* External variables and structures */
/*
extern int level_feats[][6];
extern int spell_sort_info[SKILL_TABLE_SIZE+1];
extern struct spell_info_type spell_info[];
extern const int *class_bonus_feats[];
extern char *weapon_damage_types[];
*/
/* External functions*/
/*
int count_metamagic_feats(struct char_data *ch);
int find_armor_type(int specType);
*/

void feato(int featnum, char *name, int in_game, int can_learn, int can_stack, int feat_type, char *prerequisites, char *description)
{
  feat_list[featnum].name = name;
  feat_list[featnum].in_game = in_game;
  feat_list[featnum].can_learn = can_learn;
  feat_list[featnum].can_stack = can_stack;
  feat_list[featnum].feat_type = feat_type;
  feat_list[featnum].prerequisites = prerequisites;
  feat_list[featnum].description = description;
  feat_list[featnum].prerequisite_list = NULL;
}

void epicfeat(int featnum)
{
  feat_list[featnum].epic = TRUE;
}

void combatfeat(int featnum)
{
  feat_list[featnum].combat_feat = TRUE;
}

void dailyfeat(int featnum, event_id event)
{
  feat_list[featnum].event = event;
}

struct feat_prerequisite* create_prerequisite(int prereq_type, int val1, int val2, int val3) {
  struct feat_prerequisite *prereq = NULL; 
  CREATE(prereq, struct feat_prerequisite, 1);
  prereq->prerequisite_type =  prereq_type;
  prereq->values[0] = val1;
  prereq->values[1] = val2;
  prereq->values[2] = val3;

  return prereq;
}

/*  The following procedures are used to define feat prerequisites.
 *  These prerequisites are automatically checked, if they exist. 
 *  Dynamically assigning prerequisites also allows us to create 
 *  dynamic 'help' and easier to read presentations of feat lists. */

void feat_prereq_attribute(int featnum, int attribute, int value) {
  struct feat_prerequisite *prereq = NULL;
  char buf[80];
 
  const char* attribute_abbr[7] = { "None",
                                   "Str",
                                   "Dex",
                                   "Int",
                                   "Wis",
                                   "Con",
                                   "Cha"
                                 };
 
  prereq = create_prerequisite(FEAT_PREREQ_ATTRIBUTE, attribute, value, 0);

  /* Generate the description. */  
  sprintf(buf, "%s : %d", attribute_abbr[attribute], value);
  prereq->description = strdup(buf);

  /*  Link it up. */
  prereq->next = feat_list[featnum].prerequisite_list;
  feat_list[featnum].prerequisite_list = prereq;
}

void feat_prereq_class_level(int featnum, int cl, int level) {
  struct feat_prerequisite *prereq = NULL;
  char buf[80];
  
  prereq = create_prerequisite(FEAT_PREREQ_CLASS_LEVEL, cl, level, 0);

  /* Generate the description. */ 
  sprintf(buf, "%s level %d", pc_class_types[cl], level);
  prereq->description = strdup(buf);

  /*   Link it up. */
  prereq->next = feat_list[featnum].prerequisite_list;
  feat_list[featnum].prerequisite_list = prereq;
}

void feat_prereq_feat(int featnum, int feat, int ranks) {
  struct feat_prerequisite *prereq = NULL;
  char buf[80];

  prereq = create_prerequisite(FEAT_PREREQ_FEAT, feat, ranks, 0);

  /* Generate the description. */
  if (ranks > 1)
    sprintf(buf, "%s (%d ranks)", feat_list[feat].name, ranks);
  else
    sprintf(buf, "%s", feat_list[feat].name);

  prereq->description = strdup(buf);

  /*   Link it up. */
  prereq->next = feat_list[featnum].prerequisite_list;
  feat_list[featnum].prerequisite_list = prereq;
}

void feat_prereq_cfeat(int featnum, int feat) {
  struct feat_prerequisite *prereq = NULL;
  char buf[80];

  prereq = create_prerequisite(FEAT_PREREQ_CFEAT, feat, 0, 0);

  sprintf(buf, "%s (in same weapon)", feat_list[feat].name);
  prereq->description = strdup(buf);

  /*   Link it up. */
  prereq->next = feat_list[featnum].prerequisite_list;
  feat_list[featnum].prerequisite_list = prereq;
}

void feat_prereq_ability(int featnum, int ability, int ranks) {
  struct feat_prerequisite *prereq = NULL;
  char buf[80];

  prereq = create_prerequisite(FEAT_PREREQ_ABILITY, ability, ranks, 0);

  sprintf(buf, "%d ranks in %s", ranks, ability_names[ability]);
  prereq->description = strdup(buf);

  /*   Link it up. */
  prereq->next = feat_list[featnum].prerequisite_list;
  feat_list[featnum].prerequisite_list = prereq;
}

void feat_prereq_spellcasting(int featnum, int casting_type, int prep_type, int circle) {
  struct feat_prerequisite *prereq = NULL;
  char buf[80];

  const char *casting_types[4] = { "None",
                                   "Arcane",
                                   "Divine",
                                   "Any" 
                                 };

  const char *spell_preparation_types[4] = { "None",
                                             "Prepared",
                                             "Spontaneous",
                                             "Any"
                                           };

  prereq = create_prerequisite(FEAT_PREREQ_SPELLCASTING, casting_type, prep_type, circle);

  sprintf(buf, "Ability to cast %s %s spells", casting_types[casting_type], spell_preparation_types[prep_type]);
  prereq->description = strdup(buf);

  /*   Link it up. */
  prereq->next = feat_list[featnum].prerequisite_list;
  feat_list[featnum].prerequisite_list = prereq;
}

void feat_prereq_race(int featnum, int race) {
  struct feat_prerequisite *prereq = NULL;
  char buf[80];

  prereq = create_prerequisite(FEAT_PREREQ_RACE, race, 0, 0);

  sprintf(buf, "Race : %s", pc_race_types[race]);
  prereq->description = strdup(buf);

  /*   Link it up. */
  prereq->next = feat_list[featnum].prerequisite_list;
  feat_list[featnum].prerequisite_list = prereq;
}

void feat_prereq_bab(int featnum, int bab) {
  struct feat_prerequisite *prereq = NULL;
  char buf[80];

  prereq = create_prerequisite(FEAT_PREREQ_BAB, bab, 0, 0);

  sprintf(buf, "BAB +%d", bab);
  prereq->description = strdup(buf);

  /* Link it up */ 
  prereq->next = feat_list[featnum].prerequisite_list;
  feat_list[featnum].prerequisite_list = prereq;
}

void feat_prereq_weapon_proficiency(int featnum) {
  struct feat_prerequisite *prereq = NULL;
  char buf[80];

  prereq = create_prerequisite(FEAT_PREREQ_WEAPON_PROFICIENCY, 0, 0, 0);

  sprintf(buf, "Proficiency in same weapon");
  prereq->description = strdup(buf);

  /*  Link it up */
  prereq->next = feat_list[featnum].prerequisite_list;
  feat_list[featnum].prerequisite_list = prereq;
}

void free_feats(void)
{
  /* Nothing to do right now */
}

void assign_feats(void)
{

  int i;

  // Initialize the list of feats.

  for (i = 0; i < NUM_FEATS; i++) {
    feat_list[i].name = "Unused Feat";
    feat_list[i].in_game = FALSE;
    feat_list[i].can_learn = FALSE;
    feat_list[i].can_stack = FALSE;
    feat_list[i].feat_type = FEAT_TYPE_NONE;
    feat_list[i].prerequisites = "ask staff";
    feat_list[i].description = "ask staff";
    feat_list[i].epic = FALSE;
    feat_list[i].combat_feat = FALSE;
    feat_list[i].prerequisite_list = NULL;
  }

/* 
 * Below are the various feat initializations : 
 *   First parameter is the feat number, defined in structs.h
 *   Second parameter is the displayed name of the feat
 *   Third parameter defines whether or not the feat is in the game or not, and thus can be learned and displayed
 *   Fourth parameter defines whether or not the feat can be learned through a trainer or whether it is
 *     a feat given automatically to certain classes or races.
 *   Fifth parameter determines wether the feat can stack with itself.
 *   Sixth parameter determines the feat type, for organization in the selection menu
 *   Seventh parameter defines the displayable prerequisites.
 *   Eighth parameter is the description of the feat
 *
 *
 */

/* Combat feats */
feato(FEAT_POWER_ATTACK,"power attack",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","subtract a number from hit and add to dam.  If 2H weapon add 2x dam instead");
feat_prereq_attribute(FEAT_POWER_ATTACK, AB_STR, 13);
  
feato(FEAT_WEAPON_FOCUS,"weapon focus",TRUE,TRUE,TRUE,FEAT_TYPE_COMBAT,"!UNDEFINED!","+1 to hit rolls for selected weapon");
feat_prereq_bab(FEAT_WEAPON_FOCUS, 1);
feat_prereq_weapon_proficiency(FEAT_WEAPON_FOCUS);

feato(FEAT_GREATER_WEAPON_FOCUS,"greater weapon focus",TRUE,TRUE,TRUE,FEAT_TYPE_COMBAT,"!UNDEFINED!","+1 to hit rolls with weapon");
feat_prereq_cfeat(FEAT_GREATER_WEAPON_FOCUS, FEAT_WEAPON_FOCUS);
feat_prereq_weapon_proficiency(FEAT_GREATER_WEAPON_FOCUS);
feat_prereq_class_level(FEAT_GREATER_WEAPON_FOCUS, CLASS_WARRIOR, 8);

feato(FEAT_SPIRITED_CHARGE,"spirited charge",TRUE,FALSE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","When mounted and using the charge action, you deal double damage with a melee weapon (or triple damage with a lance).");
feat_prereq_ability(FEAT_SPIRITED_CHARGE, ABILITY_RIDE, 1);
feat_prereq_feat(FEAT_SPIRITED_CHARGE, FEAT_MOUNTED_COMBAT, 1);
feat_prereq_feat(FEAT_SPIRITED_CHARGE, FEAT_RIDE_BY_ATTACK, 1);

feato(FEAT_BLIND_FIGHT,"blind fighting",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","when fighting blind, retain dex bonus to AC and deny enemy +2 attack bonus for invisibility or other concealment.");

feato(FEAT_CLEAVE,"cleave",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","extra initial attack against opponent after killing another opponent in same room");
feat_prereq_attribute(FEAT_CLEAVE, AB_STR, 13);
feat_prereq_feat(FEAT_CLEAVE, FEAT_POWER_ATTACK, 1);

feato(FEAT_COMBAT_EXPERTISE,"combat expertise",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","When active, take -5 penalty to attack roll and gain a +5 dodge bonus to your AC");
feat_prereq_attribute(FEAT_COMBAT_EXPERTISE, AB_INT, 13);

feato(FEAT_COMBAT_REFLEXES,"combat reflexes",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","can make a number of attacks of opportunity equal to dex bonus");

feato(FEAT_DODGE,"dodge",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","+1 dodge bonus to ac");
feat_prereq_attribute(FEAT_DODGE, AB_DEX, 13);

feato(FEAT_MOBILITY,"mobility",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","+4 dodge ac bonus against attacks of opportunity");
feat_prereq_attribute(FEAT_MOBILITY, AB_DEX, 13);
feat_prereq_feat(FEAT_MOBILITY, FEAT_DODGE, 1);

feato(FEAT_EPIC_PROWESS,"epic prowess",TRUE,TRUE,TRUE,FEAT_TYPE_COMBAT,"!UNDEFINED!","+1 to all attacks per rank");

feato(FEAT_IMPROVED_CRITICAL,"improved critical",TRUE,TRUE,TRUE,FEAT_TYPE_COMBAT,"!UNDEFINED!","doubled critical threat rating for weapon chosen");
feat_prereq_weapon_proficiency(FEAT_IMPROVED_CRITICAL);
feat_prereq_bab(FEAT_IMPROVED_CRITICAL, 8);

feato(FEAT_IMPROVED_INITIATIVE,"improved initiative",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","+4 to initiative checks to see who attacks first each round");

feato(FEAT_IMPROVED_SHIELD_BASH,"improved shield bash",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","retain your shield's AC bonus when you shield punch");
feat_prereq_feat(FEAT_IMPROVED_SHIELD_BASH, FEAT_ARMOR_PROFICIENCY_SHIELD, 1);

feato(FEAT_IMPROVED_TRIP,"improved trip",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","no attack of opportunity when tripping, +4 to trip check, attack immediately on successful trip.");
feat_prereq_attribute(FEAT_IMPROVED_TRIP, AB_INT, 13);
feat_prereq_feat(FEAT_IMPROVED_TRIP, FEAT_COMBAT_EXPERTISE, 1);

feato(FEAT_IMPROVED_UNARMED_STRIKE,"improved unarmed strike",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","unarmed attacks do not provoke attacks of opportunity, and do 1d6 damage");

feato(FEAT_POINT_BLANK_SHOT,"point blank shot",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","+1 to hit and dam rolls with ranged weapons in the same room");


feato(FEAT_RAPID_SHOT,"rapid shot",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","can make extra attack per round with ranged weapon at -2 to all attacks");
feat_prereq_attribute(FEAT_RAPID_SHOT, AB_DEX, 13);
feat_prereq_feat(FEAT_RAPID_SHOT, FEAT_POINT_BLANK_SHOT, 1);

feato(FEAT_SHIELD_CHARGE,"shield charge",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","make a trip attack when you bash with your shield");
feat_prereq_bab(FEAT_SHIELD_CHARGE, 3);
feat_prereq_feat(FEAT_SHIELD_CHARGE, FEAT_IMPROVED_SHIELD_BASH, 1);

feato(FEAT_SHIELD_SLAM,"shield slam",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","Daze an opponent of any size by slamming them with your shield.");
feat_prereq_bab(FEAT_SHIELD_SLAM, 6);
feat_prereq_feat(FEAT_SHIELD_SLAM, FEAT_SHIELD_CHARGE, 1);
feat_prereq_feat(FEAT_SHIELD_SLAM, FEAT_IMPROVED_SHIELD_BASH, 1);

feato(FEAT_SPRING_ATTACK,"spring attack",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","free attack of opportunity against combat abilities (ie. kick,Trip)");
feat_prereq_bab(FEAT_SPRING_ATTACK, 4);
feat_prereq_attribute(FEAT_SPRING_ATTACK, AB_DEX, 13);
feat_prereq_feat(FEAT_SPRING_ATTACK, FEAT_DODGE, 1);
feat_prereq_feat(FEAT_SPRING_ATTACK, FEAT_MOBILITY, 1);

feato(FEAT_STUNNING_FIST,"stunning fist",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","may make unarmed attack to stun opponent for one round");
feat_prereq_attribute(FEAT_STUNNING_FIST, AB_DEX, 13);
feat_prereq_attribute(FEAT_STUNNING_FIST, AB_WIS, 13);
feat_prereq_feat(FEAT_STUNNING_FIST, FEAT_IMPROVED_UNARMED_STRIKE, 1);
feat_prereq_bab(FEAT_STUNNING_FIST, 8);

feato(FEAT_TWO_WEAPON_FIGHTING,"two weapon fighting",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","attacks with offhand weapons done at reduced penalties");
feat_prereq_attribute(FEAT_TWO_WEAPON_FIGHTING, AB_DEX, 15);

feato(FEAT_WEAPON_FINESSE,"weapon finesse",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","use dex for hit roll of weapons smaller than wielder or rapier, whip, spiked chain");
feat_prereq_bab(FEAT_WEAPON_FINESSE, 1);

feato(FEAT_WHIRLWIND_ATTACK,"whirlwind attack",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","allows you to attack everyone in the room or everyone you are fighting (with contain) as a standard action");
feat_prereq_attribute(FEAT_WHIRLWIND_ATTACK, AB_DEX, 13);
feat_prereq_attribute(FEAT_WHIRLWIND_ATTACK, AB_INT, 13);
feat_prereq_feat(FEAT_WHIRLWIND_ATTACK, FEAT_COMBAT_EXPERTISE, 1);
feat_prereq_feat(FEAT_WHIRLWIND_ATTACK, FEAT_DODGE, 1);
feat_prereq_feat(FEAT_WHIRLWIND_ATTACK, FEAT_MOBILITY, 1);
feat_prereq_feat(FEAT_WHIRLWIND_ATTACK, FEAT_SPRING_ATTACK, 1);
feat_prereq_bab(FEAT_WHIRLWIND_ATTACK, 4);

feato(FEAT_GREAT_CLEAVE,"great cleave",TRUE,TRUE,FALSE,FEAT_TYPE_COMBAT,"!UNDEFINED!","ask staff");
feat_prereq_feat(FEAT_GREAT_CLEAVE, FEAT_CLEAVE, 1);
feat_prereq_feat(FEAT_GREAT_CLEAVE, FEAT_POWER_ATTACK, 1);
feat_prereq_attribute(FEAT_GREAT_CLEAVE, AB_STR, 13);
feat_prereq_bab(FEAT_GREAT_CLEAVE, 4);

feato(FEAT_WEAPON_SPECIALIZATION, "weapon specialization", TRUE, TRUE, TRUE, FEAT_TYPE_COMBAT, "!UNDEFINED!", "+2 to dam rolls with weapon");
feat_prereq_weapon_proficiency(FEAT_WEAPON_SPECIALIZATION);
feat_prereq_cfeat(FEAT_WEAPON_SPECIALIZATION, FEAT_WEAPON_FOCUS);
feat_prereq_class_level(FEAT_WEAPON_SPECIALIZATION, CLASS_WARRIOR, 4);

feato(FEAT_GREATER_WEAPON_SPECIALIZATION,"greater weapon specialization",FALSE,TRUE,TRUE,FEAT_TYPE_COMBAT,"!UNDEFINED!","additional +2 dam with weapon (stacks)");
feat_prereq_weapon_proficiency(FEAT_GREATER_WEAPON_SPECIALIZATION);
feat_prereq_cfeat(FEAT_GREATER_WEAPON_SPECIALIZATION, FEAT_WEAPON_FOCUS);
feat_prereq_cfeat(FEAT_GREATER_WEAPON_SPECIALIZATION, FEAT_GREATER_WEAPON_FOCUS);
feat_prereq_cfeat(FEAT_GREATER_WEAPON_SPECIALIZATION, FEAT_WEAPON_SPECIALIZATION);
feat_prereq_class_level(FEAT_GREATER_WEAPON_SPECIALIZATION, CLASS_WARRIOR, 12);


/* General feats */

feato(FEAT_ABLE_LEARNER,"able learner",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+1 to all skills");
feato(FEAT_ACROBATIC,"acrobatic",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to jump and tumble skill checks");
feato(FEAT_AGILE,"agile",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to balance and escape artist skill checks");
feato(FEAT_ALERTNESS,"alertness",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED","+2 to spot and listen skill checks ");
feato(FEAT_ANIMAL_AFFINITY,"animal affinity",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to handle animal and ride skill checks");
feato(FEAT_ARMOR_PROFICIENCY_HEAVY,"heavy armor proficiency",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","allows unpenalized use of heavy armor ");
feat_prereq_feat(FEAT_ARMOR_PROFICIENCY_HEAVY, FEAT_ARMOR_PROFICIENCY_LIGHT, 1);
feat_prereq_feat(FEAT_ARMOR_PROFICIENCY_HEAVY, FEAT_ARMOR_PROFICIENCY_MEDIUM, 1);

feato(FEAT_ARMOR_PROFICIENCY_LIGHT,"light armor proficiency",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","allows unpenalized use of light armor ");
feato(FEAT_ARMOR_PROFICIENCY_MEDIUM,"medium armor proficiency",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","allows unpenalized use of medium armor ");
feat_prereq_feat(FEAT_ARMOR_PROFICIENCY_MEDIUM, FEAT_ARMOR_PROFICIENCY_LIGHT, 1);

feato(FEAT_ARMOR_PROFICIENCY_TOWER_SHIELD,"tower shield proficiency",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","can use tower shields without penalties");

feato(FEAT_ARMOR_SKIN,"armor skin",TRUE,TRUE,TRUE,FEAT_TYPE_GENERAL,"!UNDEFINED!","Increases natural armor by 1"); /* Epic */
feato(FEAT_ATHLETIC,"athletic",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to swim and climb skill checks");
feato(FEAT_DECEITFUL,"deceitful",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to disguise and forgery skill checks");
feato(FEAT_DEFT_HANDS,"deft hands",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to sleight of hand and use rope skill checks");
feato(FEAT_DILIGENT,"diligent",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 bonus to appraise and decipher script skill checks");
feato(FEAT_EPIC_TOUGHNESS,"epic toughness",TRUE,TRUE,TRUE,FEAT_TYPE_GENERAL,"!UNDEFINED!","You gain +1 hp per level"); /* Epic */
feato(FEAT_EXOTIC_WEAPON_PROFICIENCY,"exotic weapon proficiency",TRUE,TRUE,TRUE,FEAT_TYPE_GENERAL,"!UNDEFINED!","can use exotic weapon of type chosen without penalties");
feat_prereq_bab(FEAT_EXOTIC_WEAPON_PROFICIENCY, 1);

feato(FEAT_EXTRA_TURNING,"extra turning",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","2 extra turn attempts per day");
feat_prereq_feat(FEAT_EXTRA_TURNING, FEAT_TURN_UNDEAD, 1);

feato(FEAT_GREAT_FORTITUDE,"great fortitude",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to all fortitude saving throw checks");

feato(FEAT_IMPROVED_SPELL_RESISTANCE,"improved spell resistance",TRUE,TRUE,TRUE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to spell resistance"); /* Epic */
feat_prereq_feat(FEAT_IMPROVED_SPELL_RESISTANCE, FEAT_DIAMOND_SOUL, 1);

feato(FEAT_INVESTIGATOR,"investigator",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to lore and search checks");
feato(FEAT_IRON_WILL,"iron will",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to all willpower saving throw checks");
feato(FEAT_LIGHTNING_REFLEXES,"lightning reflexes",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to all reflex saving throw checks");
feato(FEAT_MAGICAL_APTITUDE,"magical aptitude",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to spellcraft and use magical device skill checks");
feato(FEAT_NEGOTIATOR,"negotiator",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to diplomacy and sense motive skills");
feato(FEAT_NIMBLE_FINGERS,"nimble fingers",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to open lock and disable device skill checks");
feato(FEAT_PERSUASIVE,"persuasive",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to bluff and intimidate skill checks");
feato(FEAT_SELF_SUFFICIENT,"self sufficient",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to heal and survival skill checks");
feato(FEAT_SIMPLE_WEAPON_PROFICIENCY,"simple weapon proficiency",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","may use all simple weappons");
feato(FEAT_STEALTHY,"stealthy",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+2 to hide and move silently skill checks");
feato(FEAT_TOUGHNESS,"toughness",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","+1 hp per level, +(level) hp upon taking");
feato(FEAT_ARMOR_PROFICIENCY_SHIELD,"shield armor proficiency",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","able to use bucklers, light and heavy shields without penalty ");
feato(FEAT_MARTIAL_WEAPON_PROFICIENCY,"martial weapon proficiency",TRUE,TRUE,FALSE,FEAT_TYPE_GENERAL,"!UNDEFINED!","able to use all martial weapons");

/* Spellcasting feats */

feato(FEAT_GREATER_SPELL_PENETRATION,"greater spell penetration",TRUE,TRUE,FALSE,FEAT_TYPE_SPELLCASTING,"!UNDEFINED!","+2 to caster level checks to defeat spell resistance");
feat_prereq_feat(FEAT_GREATER_SPELL_PENETRATION, FEAT_SPELL_PENETRATION, 1);

feato(FEAT_SPELL_PENETRATION,"spell penetration",TRUE,TRUE,FALSE,FEAT_TYPE_SPELLCASTING,"!UNDEFINED!","+2 bonus on caster level checks to defeat spell resistance");

/* Crafting feats */

feato(FEAT_DRACONIC_CRAFTING,"draconic crafting",TRUE,FALSE,FALSE,FEAT_TYPE_CRAFT,"!UNDEFINED!","All magical items created gain higher bonuses w/o increasing level");
feato(FEAT_DWARVEN_CRAFTING,"dwarven crafting",TRUE,FALSE,FALSE,FEAT_TYPE_CRAFT,"!UNDEFINED!","All weapons and armor made have higher bonuses");
feato(FEAT_ELVEN_CRAFTING,"elven crafting",TRUE,FALSE,FALSE,FEAT_TYPE_CRAFT,"!UNDEFINED!","All equipment made is 50% weight and uses 50% materials");
feato(FEAT_FAST_CRAFTER,"fast crafter",TRUE,FALSE,FALSE,FEAT_TYPE_CRAFT,"!UNDEFINED","Reduces crafting time");


/* Disabled Feats */

feato(FEAT_ACROBATIC_CHARGE,"acrobatic charge",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"none","can charge in situations when others cannot");
feato(FEAT_CANNY_DEFENSE,"canny defense",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"none","add int bonus (max class level) to ac when useing one light weapon and no shield");
feato(FEAT_ELABORATE_PARRY,"elaborate parry",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"none","when fighting defensively or total defense, gains +1 dodge ac per class level");
feato(FEAT_FAR_SHOT,"far shot",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"ask staff","ask staff");
feato(FEAT_GREATER_FLURRY,"greater flurry",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"none","extra unarmed attack when using flurry of blows at -5 penalty");
feato(FEAT_GREATER_RAGE,"greater rage",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"none","+6 to str and con when raging");
feato(FEAT_IMPROVED_DISARM,"improved disarm",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"ask staff","ask staff ");
feato(FEAT_IMPROVED_GRAPPLE,"improved grapple",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"ask staff","ask staff");
feato(FEAT_IMPROVED_OVERRUN,"improved overrun",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"ask staff","ask staff");
feato(FEAT_IMPROVED_PRECISE_SHOT,"improved precise shot",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"ranger level 11","+1 to hit on all ranged attacks");
feato(FEAT_IMPROVED_UNCANNY_DODGE,"improved uncanny dodge",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"none","cannot be flanked (or sneak attacked");
feato(FEAT_MANYSHOT,"manyshot",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"ranger level 6","extra ranged attack when rapid shot turned on");
feato(FEAT_MIGHTY_RAGE,"mighty rage",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"str 21, con 21, greater rage, rage 5/day","+8 str and con and +4 to will saves when raging");
feato(FEAT_MOUNTED_ARCHERY,"mounted archery",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"ask staff","ask staff");
feato(FEAT_PRECISE_STRIKE,"precise strike",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"none","+1d6 damage when using only one weapon and no shield");
feato(FEAT_QUICK_DRAW,"quick draw",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"ask staff","ask staff");
feato(FEAT_RAPID_RELOAD,"rapid reload",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"ask staff","ask staff");
feato(FEAT_RIDE_BY_ATTACK,"ride by attack",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"ask staff","ask staff");
feato(FEAT_SHOT_ON_THE_RUN,"shot on the run",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"ask staff","ask staff");
feato(FEAT_TIRELESS_RAGE,"tireless rage",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"none","no fatigue after raging");
feato(FEAT_WEAPON_TOUCH,"weapon touch",FALSE,FALSE,FALSE,FEAT_TYPE_COMBAT,"ask staff","ask staff");
feato(FEAT_WEAPON_MASTERY,"weapon mastery",FALSE,FALSE,TRUE,FEAT_TYPE_COMBAT,"proficiency, weapon focus, weapon specialization in specific weapon, Base Attack Bonus +8","+2 to hit and damage with that weapon");
feato(FEAT_COMBAT_CHALLENGE,"combat challenge",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"5 ranks in diplomacy, intimidate or bluff","allows you to make a mob focus their attention on you");
feato(FEAT_DEFLECT_ARROWS,"deflect arrows",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"dex 13, improved unarmed strike","can deflect one ranged attack per round ");
feato(FEAT_EPIC_COMBAT_CHALLENGE,"epic combat challenge",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"20 ranks in diplomacy, intimidate or bluff, greater combat challenge","as improved combat challenge, but both regular challenges and challenge all are minor actions");
feato(FEAT_EPIC_DODGE,"epic dodge",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"dex 25, dodge,Tumble 30, improved evasion, defensive roll","automatically dodge first attack against you each round");
feato(FEAT_EXTEND_RAGE,"extend rage",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"rage","Each of the uses of your rage or frenzy ability lasts an additional 5 rounds beyond its normal duration.");
feato(FEAT_GREAT_SMITING,"great smiting",FALSE,TRUE,TRUE,FEAT_TYPE_COMBAT,"epic level","For each rank in this feat you add your level in damage to all smite attacks");
feato(FEAT_GREATER_COMBAT_CHALLENGE,"greater combat challenge",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"15 ranks in diplomacy, intimidate or bluff, improved combat challenge","as improved combat challenge, but regular challenge is a minor action & challenge all is a move action");
feato(FEAT_GREATER_TWO_WEAPON_FIGHTING,"greater two weapon fighting",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"dex 19, base attack bonus 11+,Two weapon & improved two weapon fighting","gives an additional offhand weapon attack");
feato(FEAT_IMPROVED_COMBAT_CHALLENGE,"improved combat challenge",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"10 ranks in diplomacy, intimidate or bluff, combat challenge","allows you to make all mobs focus their attention on you");
feato(FEAT_IMPROVED_FEINT,"improved feint",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"int 13, combat expertise","can feint and make one attack per round (or sneak attack if they have it)");
feato(FEAT_IMPROVED_NATURAL_WEAPON,"improved natural weapons",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"natural weapon or improved unarmed strike","increase damage dice by one category for natural weapons");
feato(FEAT_IMPROVED_SNEAK_ATTACK,"improved sneak attack",FALSE,TRUE,TRUE,FEAT_TYPE_COMBAT,"sneak attack 1d6 or more","each rank gives +5% chance per attack, per rank to be a sneak attack.");
feato(FEAT_IMPROVED_TAUNTING,"improved taunting",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"ask staff","ask staff");
feato(FEAT_IMPROVED_TWO_WEAPON_FIGHTING,"improved two weapon fighting",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"dex 17,Two weapon fighting, base attack bonus of +6 or more","extra attack with offhand weapon at -5 penalty");
feato(FEAT_IMPROVED_WEAPON_FINESSE,"improved weapon finesse",FALSE,TRUE,TRUE,FEAT_TYPE_COMBAT,"weapon finesse, weapon focus, base attack bonus of 4+","add dex bonus to damage instead of str for light weapons");
feato(FEAT_KNOCKDOWN,"knockdown",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"improved trip","when active, any melee attack that deals 10 damage or more invokes a free automatic trip attempt against your target");
feato(FEAT_MOUNTED_COMBAT,"mounted combat",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"ride rank 1","once per round rider may negate a hit against him with a successful ride vs attack roll check");
feato(FEAT_PERFECT_TWO_WEAPON_FIGHTING,"perfect two weapon fighting",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"dex 25, greater two weapon fighting","Extra attack with offhand weapon");
feato(FEAT_POWER_CRITICAL,"power critical",FALSE,TRUE,TRUE,FEAT_TYPE_COMBAT,"weapon focus in chosen weapon, base attack bonus +4 or higher","+4 to rolls to confirm critical hits.");
feato(FEAT_PRECISE_SHOT,"precise shot",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"point blank shot","You may shoot in melee without the standard -4 to hit penalty");
feato(FEAT_ROBILARS_GAMBIT,"robilars gambit",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"combat reflexes, base attack bonus +12","when active enemies gain +4 to hit and damage against you, but all melee attacks invoke an attack of opportunity from you.");
feato(FEAT_SELF_CONCEALMENT,"self concealment",FALSE,TRUE,TRUE,FEAT_TYPE_COMBAT,"stealth 30 ranks, dex 30,Tumble 30 ranks","10% miss chance for attacks against you per rank");
feato(FEAT_SNEAK_ATTACK,"sneak attack",FALSE,TRUE,TRUE,FEAT_TYPE_COMBAT,"as epic feat: sneak attack +8d6","+1d6 to damage when flanking");
feato(FEAT_SNEAK_ATTACK_OF_OPPORTUNITY,"sneak attack of opportunity",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"sneak attack +8d6, opportunist feat","makes all opportunity attacks sneak attacks");
feato(FEAT_SWARM_OF_ARROWS,"swarm of arrows",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"dex 23, point blank shot, rapid shot, weapon focus","allows you to make a single ranged attack against everyone in range.");
feato(FEAT_TWO_WEAPON_DEFENSE,"two weapon defense",FALSE,TRUE,FALSE,FEAT_TYPE_COMBAT,"dex 15,Two weapon fighting","when wielding two weapons receive +1 shield ac bonus");
feato(FEAT_WEAPON_FLURRY,"weapon flurry",FALSE,TRUE,TRUE,FEAT_TYPE_COMBAT,"proficiency, weapon focus, weapon specialization, weapon mastery in specific weapon, base attack bonus +14","2nd attack at -5 to hit with standard action or extra attack at full bonus with full round action");
feato(FEAT_WEAPON_SUPREMACY,"weapon supremacy",FALSE,TRUE,TRUE,FEAT_TYPE_COMBAT,"proficiency, weapon focus, weapon specialization, weapon master, greater weapon focus, greater weapon specialization in specific weapon,Fighter level 18","+4 to resist disarm, ignore grapples, add +5 to hit roll when miss by 5 or less, can take 10 on attack rolls, +1 bonus to AC when wielding weapon");
feato(FEAT_BRANDING,"branding",FALSE,FALSE,FALSE,FEAT_TYPE_CRAFT,"Artisan level 3","All items made carry the artisan's brand");
feato(FEAT_BREW_POTION,"brew potion",FALSE,FALSE,FALSE,FEAT_TYPE_CRAFT,"3rd level caster","can create magical potions ");
feato(FEAT_CRAFT_MAGICAL_ARMS_AND_ARMOR,"craft magical arms and armor",FALSE,FALSE,FALSE,FEAT_TYPE_CRAFT,"5th level caster","can create magical weapons and armor ");
feato(FEAT_CRAFT_ROD,"craft rod",FALSE,FALSE,FALSE,FEAT_TYPE_CRAFT,"9th level caster","can crate magical rods");
feato(FEAT_CRAFT_STAFF,"craft staff",FALSE,FALSE,FALSE,FEAT_TYPE_CRAFT,"12th level caster","can create magical staves ");
feato(FEAT_CRAFT_WAND,"craft wand",FALSE,FALSE,FALSE,FEAT_TYPE_CRAFT,"5th level caster","can create magical wands ");
feato(FEAT_CRAFT_WONDEROUS_ITEM,"craft wonderous item",FALSE,FALSE,FALSE,FEAT_TYPE_CRAFT,"3rd level caster","can crate miscellaneous magical items ");
feato(FEAT_FORGE_RING,"forge ring",FALSE,FALSE,FALSE,FEAT_TYPE_CRAFT,"ask staff","ask staff ");
feato(FEAT_MASTERWORK_CRAFTING,"masterwork crafting",FALSE,FALSE,FALSE,FEAT_TYPE_CRAFT,"Artisan level 6","All equipment made is masterwork");
feato(FEAT_SCRIBE_SCROLL,"scribe scroll",FALSE,FALSE,FALSE,FEAT_TYPE_CRAFT,"1st level caster","can scribe spells from memory onto scrolls");
feato(FEAT_DIVINE_MIGHT,"divine might",FALSE,TRUE,FALSE,FEAT_TYPE_DIVINE,"turn undead, power attack, cha 13, str 13","Add cha bonus to damage for number of rounds equal to cha bonus");
feato(FEAT_DIVINE_SHIELD,"divine shield",FALSE,TRUE,FALSE,FEAT_TYPE_DIVINE,"turn undead, power attack, cha 13, str 13","Add cha bonus to armor class for number of rounds equal to cha bonus");
feato(FEAT_DIVINE_VENGEANCE,"divine vengeance",FALSE,TRUE,FALSE,FEAT_TYPE_DIVINE,"turn undead, extra turning","Add 2d6 damage against undead for number of rounds equal to cha bonus");
feato(FEAT_EXCEPTIONAL_TURNING,"exceptional turning",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"sun cleric domain","+1d10 hit dice of undead turned");
feato(FEAT_HEROIC_INITIATIVE,"heroic initiative",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"none","bonus to initiative checks");
feato(FEAT_IMPROVED_BULL_RUSH,"improved bull rush",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_IMPROVED_REACTION,"improved reaction",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"none","+2 bonus to initiative checks (+4 at 8th class level)");
feato(FEAT_IMPROVED_SUNDER,"improved sunder",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_IMPROVED_TURNING,"improved turning",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_INDOMITABLE_WILL,"indomitable will",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_NATURAL_ARMOR_INCREASE,"natural armor increase",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_SUNDER,"sunder",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_TRACK,"track",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"none","use survival skill to track others");
feato(FEAT_WEAPON_PROFICIENCY_DRUID,"weapon proficiency - druids",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_WEAPON_PROFICIENCY_ELF,"weapon proficiency - elves",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_WEAPON_PROFICIENCY_MONK,"weapon proficiency - monks",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_WEAPON_PROFICIENCY_ROGUE,"weapon proficiency - rogues",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_WEAPON_PROFICIENCY_WIZARD,"weapon proficiency - wizards",FALSE,FALSE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_ARMOR_SPECIALIZATION_HEAVY,"armor specialization (heavy)",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"proficient with heavy armor, Base Attack Bonus +12","DR 2/- when wearing heavy armor");
feato(FEAT_ARMOR_SPECIALIZATION_LIGHT,"armor specialization (light)",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"proficient with light armor, Base Attack Bonus +12","DR 2/- when wearing light armor");
feato(FEAT_ARMOR_SPECIALIZATION_MEDIUM,"armor specialization (medium)",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"proficient with medium armor, Base Attack Bonus +12","DR 2/- when wearing medium armor");
feato(FEAT_DAMAGE_REDUCTION,"damage reduction",FALSE,TRUE,TRUE,FEAT_TYPE_GENERAL,"none","1/- damage reduction per rank of feat, 3/- for epic");
feato(FEAT_DIEHARD,"diehard",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"endurance","will stay alive and conscious until -10 hp or lower");
feato(FEAT_DIVINE_GRACE,"divine grace",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"none","charisma bonus added to all saving throw checks");
feato(FEAT_ENDURANCE,"endurance",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"none","+4 to con and skill checks made to resist fatigue and 1 extra move point per level ");
feato(FEAT_ENERGY_RESISTANCE,"energy resistance",FALSE,TRUE,TRUE,FEAT_TYPE_GENERAL,"none","reduces all energy related damage by 3 per rank");
feato(FEAT_EPIC_SKILL_FOCUS,"epic skill focus",FALSE,TRUE,TRUE,FEAT_TYPE_GENERAL,"20 ranks in the skill","+10 in chosen skill");
feato(FEAT_EXTRA_MUSIC,"extra music",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"bard level 1","4 extra bard music uses per day");
feato(FEAT_EXTRA_RAGE,"extra rage",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_FAST_HEALING,"fast healing",FALSE,TRUE,TRUE,FEAT_TYPE_GENERAL,"epic level, con 25","Heals 3 hp per rank each combat round if fighting otherwise every 6 seconds");
feato(FEAT_GREAT_CHARISMA,"great charisma",FALSE,TRUE,TRUE,FEAT_TYPE_GENERAL,"epic level","Increases Wisdom by 1");
feato(FEAT_GREAT_CONSTITUTION,"great constitution",FALSE,TRUE,TRUE,FEAT_TYPE_GENERAL,"epic level","Increases Constitution by 1");
feato(FEAT_GREAT_DEXTERITY,"great dexterity",FALSE,TRUE,TRUE,FEAT_TYPE_GENERAL,"epic level","Increases Dexterity by 1");
feato(FEAT_GREAT_INTELLIGENCE,"great intelligence",FALSE,TRUE,TRUE,FEAT_TYPE_GENERAL,"epic level","Increases Intelligence by 1");
feato(FEAT_GREAT_STRENGTH,"great strength",FALSE,TRUE,TRUE,FEAT_TYPE_GENERAL,"epic level","Increases Strength by 1");
feato(FEAT_GREAT_WISDOM,"great wisdom",FALSE,TRUE,TRUE,FEAT_TYPE_GENERAL,"epic level","Increases Wisdom by 1");
feato(FEAT_HONORBOUND,"honorbound",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"none","+2 to saving throws against fear or compulsion effects, +2 to sense motive checks");
feato(FEAT_IMPROVED_INSTIGATION,"improved instigation",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_IMPROVED_INTIMIDATION,"improved intimidation",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_LEADERSHIP,"leadership",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"level 6 character","can have more and higher level followers, group members get extra exp on kills and hit/ac bonuses");
feato(FEAT_LINGERING_SONG,"lingering song",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"bard level 1","5 extra rounds for bard songs");
feato(FEAT_MONKEY_GRIP,"monkey grip",FALSE,TRUE,TRUE,FEAT_TYPE_GENERAL,"none","can wield weapons one size larger than wielder in one hand with -2 to attacks.");
feato(FEAT_POWERFUL_SNEAK,"powerful sneak",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"rogue talent","opt to take -2 to attacks and treat all sneak attack dice rolls of 1 as a 2");
feato(FEAT_RUN,"run",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_SKILL_FOCUS,"skill focus",FALSE,TRUE,TRUE,FEAT_TYPE_GENERAL,"none","+3 in chosen skill");
feato(FEAT_STEADFAST_DETERMINATION,"steadfast determination",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"endurance feat","allows you to use your con bonus instead of your wis bonus for will saves");
feato(FEAT_WEAPON_PROFICIENCY_BASTARD_SWORD,"weapon proficiency - bastard sword",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"ask staff","ask staff");
feato(FEAT_BLINDSENSE,"blindsense",FALSE,FALSE,FALSE,FEAT_TYPE_INNATE_ABILITY,"ask staff","ask staff");
feato(FEAT_BREATH_WEAPON,"breath weapon",FALSE,FALSE,FALSE,FEAT_TYPE_INNATE_ABILITY,"ask staff","ask staff");
feato(FEAT_CHARISMA_BOOST,"charisma boost",FALSE,FALSE,FALSE,FEAT_TYPE_INNATE_ABILITY,"ask staff","ask staff");
feato(FEAT_CLAWS_AND_BITE,"claws and bite",FALSE,FALSE,FALSE,FEAT_TYPE_INNATE_ABILITY,"ask staff","ask staff");
feato(FEAT_CONSTITUTION_BOOST,"constitution boost",FALSE,FALSE,FALSE,FEAT_TYPE_INNATE_ABILITY,"ask staff","ask staff");
feato(FEAT_INTELLIGENCE_BOOST,"intelligence boost",FALSE,FALSE,FALSE,FEAT_TYPE_INNATE_ABILITY,"ask staff","ask staff");
feato(FEAT_SLEEP_PARALYSIS_IMMUNITY,"sleep & paralysis immunity",FALSE,FALSE,FALSE,FEAT_TYPE_INNATE_ABILITY,"ask staff","ask staff");
feato(FEAT_STRENGTH_BOOST,"strength boost",FALSE,FALSE,FALSE,FEAT_TYPE_INNATE_ABILITY,"ask staff","ask staff");
feato(FEAT_TRAMPLE,"trample",FALSE,FALSE,FALSE,FEAT_TYPE_INNATE_ABILITY,"ask staff","ask staff");
feato(FEAT_WINGS,"wings",FALSE,FALSE,FALSE,FEAT_TYPE_INNATE_ABILITY,"ask staff","ask staff");
feato(FEAT_ENLARGE_SPELL,"enlarge spell",FALSE,FALSE,FALSE,FEAT_TYPE_METAMAGIC,"ask staff","ask staff ");
feato(FEAT_HEIGHTEN_SPELL,"heighten spell",FALSE,FALSE,FALSE,FEAT_TYPE_METAMAGIC,"ask staff","ask staff ");
feato(FEAT_SILENT_SPELL,"silent spell",FALSE,FALSE,FALSE,FEAT_TYPE_METAMAGIC,"ask staff","ask staff");
feato(FEAT_STILL_SPELL,"still spell",FALSE,FALSE,FALSE,FEAT_TYPE_METAMAGIC,"ask staff","ask staff");
feato(FEAT_WIDEN_SPELL,"widen spell",FALSE,FALSE,FALSE,FEAT_TYPE_METAMAGIC,"ask staff","ask staff");
feato(FEAT_AUTOMATIC_QUICKEN_SPELL,"automatic quicken spell",FALSE,TRUE,TRUE,FEAT_TYPE_METAMAGIC,"epic level, spellcraft 30 ranks, ability to cast level 9 arcane or divine spells","You can cast level 0, 1, 2 & 3 spells automatically as if quickened.  Every addition rank increases the max spell level by 3.");
feato(FEAT_EMPOWER_SPELL,"empower spell",FALSE,TRUE,FALSE,FEAT_TYPE_METAMAGIC,"1st level caster","all variable numerical effects of a spell are increased by one half ");
feato(FEAT_EMPOWERED_MAGIC,"empowered magic",FALSE,TRUE,FALSE,FEAT_TYPE_METAMAGIC,"spellcaster level 1","+1 to all spell dcs");
feato(FEAT_ENHANCE_SPELL,"increase spell damage",FALSE,TRUE,FALSE,FEAT_TYPE_METAMAGIC,"epic spellcaster","increase max number of damage dice for certain damage based spell by 5");
feato(FEAT_EXTEND_SPELL,"extend spell",FALSE,TRUE,FALSE,FEAT_TYPE_METAMAGIC,"can cast spells","durations of spells are 50% longer when enabled ");
feato(FEAT_INTENSIFY_SPELL,"intensify spell",FALSE,TRUE,FALSE,FEAT_TYPE_METAMAGIC,"empower spell, maximize spell, spellcraft 30 ranks, ability ro cast lvl 9 arcane or divine spells","maximizes damage/healing and then doubles it.");
feato(FEAT_MAXIMIZE_SPELL,"maximize spell",FALSE,TRUE,FALSE,FEAT_TYPE_METAMAGIC,"can cast spells","all spells cast while maximised enabled do maximum effect.");
feato(FEAT_QUICKEN_SPELL,"quicken spell",FALSE,TRUE,FALSE,FEAT_TYPE_METAMAGIC,"can cast spells","allows you to cast spell as a move action instead of standard action");
feato(FEAT_ESCHEW_MATERIALS,"eschew materials",FALSE,FALSE,FALSE,FEAT_TYPE_SPELLCASTING,"ask staff","ask staff");
feato(FEAT_GREATER_SPELL_FOCUS,"greater spell focus",FALSE,FALSE,TRUE,FEAT_TYPE_SPELLCASTING,"ask staff","ask staff");
feato(FEAT_IMPROVED_COUNTERSPELL,"improved counterspell",FALSE,FALSE,FALSE,FEAT_TYPE_SPELLCASTING,"ask staff","ask staff");
feato(FEAT_IMPROVED_FAMILIAR,"improved familiar",FALSE,FALSE,FALSE,FEAT_TYPE_SPELLCASTING,"ask staff","ask staff");
feato(FEAT_SPELL_MASTERY,"spell mastery",FALSE,FALSE,FALSE,FEAT_TYPE_SPELLCASTING,"ask staff","ask staff");
feato(FEAT_AUGMENT_SUMMONING,"augment summoning",FALSE,TRUE,FALSE,FEAT_TYPE_SPELLCASTING,"none","gives all creatures you have from summoning spells +4 to strength and constitution");
feato(FEAT_COMBAT_CASTING,"combat casting",FALSE,TRUE,FALSE,FEAT_TYPE_SPELLCASTING,"none","+4 to concentration checks made in combat or when grappled ");
feato(FEAT_ENHANCED_SPELL_DAMAGE,"enhanced spell damage",FALSE,TRUE,FALSE,FEAT_TYPE_SPELLCASTING,"spellcaster level 1","+1 spell damage per die rolled");
feato(FEAT_EPIC_SPELLCASTING,"epic spellcasting",FALSE,TRUE,FALSE,FEAT_TYPE_SPELLCASTING,"lore 24, spellcraft 24","allows you to cast epic spells");
feato(FEAT_FASTER_MEMORIZATION,"faster memorization",FALSE,TRUE,FALSE,FEAT_TYPE_SPELLCASTING,"memorization based spellcaster level 1","decreases spell memorization time");
feato(FEAT_SPELL_FOCUS,"spell focus",FALSE,TRUE,TRUE,FEAT_TYPE_SPELLCASTING,"1st level caster","+1 to all spell dcs for all spells in school/domain");
feato(FEAT_FAST_HEALER,"fast healer",FALSE,TRUE,FALSE,FEAT_TYPE_GENERAL,"Can only be taken at level 1","+2 hp healed per round");
feato(FEAT_NATURAL_SPELL,"natural spell",FALSE,TRUE,FALSE,FEAT_TYPE_WILD,"wis 13+, ability to wild shape","allows casting of spells while wild shaped.");
feato(FEAT_LAST_FEAT,"do not take me",FALSE,FALSE,FALSE,FEAT_TYPE_NONE,"placeholder feat","placeholder feat");

/* Class ability feats */
/* Paladin */
feato(FEAT_AURA_OF_COURAGE,"aura of courage",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Immunity to fear attacks, +4 bonus to fear saves for group members");
feato(FEAT_SMITE_EVIL,"smite evil",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","add level to hit roll and charisma bonus to damage");
feato(FEAT_DETECT_EVIL,"detect evil",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","able to detect evil alignments");
feato(FEAT_TURN_UNDEAD,"turn undead",TRUE,TRUE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","can cause fear in or destroy undead based on class level and charisma bonus");
feato(FEAT_AURA_OF_GOOD,"aura of good",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","+10 ac to all group members");
feato(FEAT_DIVINE_HEALTH,"divine health",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","immune to disease");
feato(FEAT_LAYHANDS,"lay on hands",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Powerful divine healing ability usable a limited number of times a day");
feato(FEAT_REMOVE_DISEASE,"remove disease",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","can cure diseases");
feato(FEAT_CALL_MOUNT,"call mount",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Allows you to call a paladin mount");

/* Rogue */
feato(FEAT_CRIPPLING_STRIKE,"crippling strike",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Chance to do 2 strength damage with a sneak attack.");
feato(FEAT_IMPROVED_EVASION,"improved evasion",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","as evasion but half damage of failed save");
feato(FEAT_EVASION,"evasion",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","on successful reflex save no damage from spells and effects");
feato(FEAT_TRAPFINDING,"trapfinding",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","can use the Search skill to locate difficult to find traps");
feato(FEAT_DEFENSIVE_ROLL,"defensive roll",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","can roll reflex save vs damage dealt when hp is to be reduced below 0 to take half damage instead");
feato(FEAT_SLIPPERY_MIND,"slippery mind",TRUE,TRUE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","extra chance for will saves");

/* Rogue/Barbarian */
feato(FEAT_UNCANNY_DODGE,"uncanny dodge",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","retains dex bonus when flat footed or against invis opponents");

/* Ranger */
feato(FEAT_FAVORED_ENEMY_AVAILABLE,"available favored enemy choice(s)",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","can choose an enemy type as a favored enemy");
feato(FEAT_FAVORED_ENEMY,"favored enemy",TRUE,FALSE,TRUE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Gain bonuses when fighting against a particular type of enemy");
feato(FEAT_CAMOUFLAGE,"camouflage",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Can use the Hide skill in any sort of natural terrain, even if the terrain doesn't grant cover or concealment.");
feato(FEAT_COMBAT_STYLE,"combat style",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Either Rapid Shot or Two Weapon Fighting, depending on the chosen combat style.");
feato(FEAT_IMPROVED_COMBAT_STYLE,"improved combat style",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Either Manyshot or Improved Two Weapon Fighting, depending on the chosen combat style.");
feato(FEAT_COMBAT_STYLE_MASTERY,"combat style master",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Either Improved Precise Shot or Greater Two Weapon Fighting, depending on the chosen combat style.");
feato(FEAT_HIDE_IN_PLAIN_SIGHT,"hide in plain sight",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","While in any sort of natural terrain, can use the Hide skill without displaying a message.");
feato(FEAT_SWIFT_TRACKER,"swift tracker",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","No penalty while autotracking.");

/* Ranger/Druid */
feato(FEAT_ANIMAL_COMPANION,"animal companion",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Can call a loyal companion animal that accompanies the adventurer.");
feato(FEAT_WILD_EMPATHY,"wild empathy",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","The adventurer can improve the attitude of an animal.");
feato(FEAT_WOODLAND_STRIDE,"woodland stride",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Reduced movement penalty when moving through woodland areas.");

/* Druid */
feato(FEAT_NATURE_SENSE,"nature sense",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","+2 to lore and survival skills");
feato(FEAT_RESIST_NATURES_LURE,"resist nature's lure",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","+4 to spells and spell like abilities from fey creatures");
feato(FEAT_THOUSAND_FACES,"a thousand faces",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"1UNDEFINED!","Can alter one's physical appearance, giving +10 to disguise checks.");
feato(FEAT_TRACKLESS_STEP,"trackless step",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Do not leave tracks in natural areas (including the wilderness.)");
feato(FEAT_WILD_SHAPE,"wild shape",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Gain the ability to shapechange into a selection of animals with unique abilities.");
feato(FEAT_WILD_SHAPE_ELEMENTAL,"wild shape (elemental)",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Can assume elemental form.");
feato(FEAT_WILD_SHAPE_HUGE,"wild shape (huge)",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Can assume the shape of a huge animal.");
feato(FEAT_WILD_SHAPE_HUGE_ELEMENTAL,"wild shape (huge elemental)",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Can assume the shape of a huge elemental.");
feato(FEAT_WILD_SHAPE_LARGE,"wild shape (large)",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Can assume the shape of a large animal.");
feato(FEAT_WILD_SHAPE_PLANT,"wild shape (plant)",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Can assume plant-like forms.");
feato(FEAT_WILD_SHAPE_TINY,"wild shape (tiny)",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Can assume the shape of tiny animals.");

/* Druid/Monk */
feato(FEAT_TIMELESS_BODY,"timeless body",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","immune to negative aging effects");

/* Monk */
feato(FEAT_UNARMED_STRIKE,"unarmed strike",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Unarmed attacks are considered to be weapons.");
feato(FEAT_KI_STRIKE,"ki strike",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","unarmed attack considered a magical weapon");
feato(FEAT_STILL_MIND,"still mind",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","+2 bonus on saving throws vs. Enchantments");
feato(FEAT_WHOLENESS_OF_BODY,"wholeness of body",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","can heal class level*2 hp to self");
feato(FEAT_SLOW_FALL,"slow fall",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","no damage for falling 1 room/feat rank");
feato(FEAT_ABUNDANT_STEP,"abundant step",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","magically move between tight spaces, as the spell dimension door");
feato(FEAT_DIAMOND_BODY,"diamond body",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","immune to disease");
feato(FEAT_DIAMOND_SOUL,"diamond soul",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","spell resistance equal to class level + 10");
feato(FEAT_EMPTY_BODY,"empty body",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","50% concealment for 1 round/monk level per day");
feato(FEAT_FLURRY_OF_BLOWS,"flurry of blows",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","extra attack when fighting unarmed at -2 to all attacks");
feato(FEAT_PERFECT_SELF,"perfect self",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Gain 10/magic damage reduction");
feato(FEAT_PURITY_OF_BODY,"purity of body",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","immune to poison");
feato(FEAT_QUIVERING_PALM,"quivering palm",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","chance to kill on strike with unarmed attack");
feato(FEAT_TONGUE_OF_THE_SUN_AND_MOON,"tongue of the sun and the moon",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","can speak any language");

/* Bard */
feato(FEAT_BARDIC_KNOWLEDGE,"bardic knowledge",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","+Int modifier bonus on knowledge checks.");
feato(FEAT_BARDIC_MUSIC,"bardic music",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Use Perform skill to create various magical effects.");
feato(FEAT_COUNTERSONG,"countersong",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Boost group members' resistance to sonic attacks.");
feato(FEAT_FASCINATE,"fascinate",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Fascinate one opponent plus one additional  for every three bard levels beyond first.");
feato(FEAT_INSPIRE_COMPETENCE,"inspire competence",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Group members gain a +2 competence bonus on skills.");
feato(FEAT_INSPIRE_GREATNESS,"inspire greatness",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Grant allies better fighting capability.");
feato(FEAT_INSPIRE_HEROICS,"inspire heroics",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Grant allies a +4 morale bonus on saving throws and a +4 dodge bonus to AC.");
feato(FEAT_MASS_SUGGESTION,"mass suggestion",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Compel a group of opponents to perform an action.");
feato(FEAT_SONG_OF_FREEDOM,"song of freedom",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Break an enchantment on a single target other than yourself.");
feato(FEAT_SUGGESTION,"suggestion",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Compel a single opponent to perform an action.");
feato(FEAT_INSPIRE_COURAGE,"inspire courage",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","Bolster group members against fear attacks and improve their combat ability.");

/* Berserker */
feato(FEAT_RAGE,"rage",TRUE,FALSE,TRUE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","+4 bonus to con and str for several rounds");

/* Sorcerer/Wizard */
feato(FEAT_SUMMON_FAMILIAR,"summon familiar",TRUE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"!UNDEFINED!","summon a magical pet");

/* Disabled/Unimplemented */
feato(FEAT_ENHANCED_MOBILITY,"enhanced mobility",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_GRACE,"grace",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_VENOM_IMMUNITY,"venom immunity",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_HONORABLE_WILL,"honorable will",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_ANIMATE_DEAD,"animate dead",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"death master level 2","allows innate use of animate dead spell 3x per day.");
feato(FEAT_ARMORED_MOBILITY,"armored mobility",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"none","heavy armor is treated as medium armor");
feato(FEAT_ARMORED_SPELLCASTING,"armored spellcasting",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_AURA_OF_EVIL,"aura of evil",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_AURA_OF_TERROR,"aura of terror",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_BONE_ARMOR,"bone armor",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"death master level 1","allows creation of bone armor and 10% arcane spell failure reduction in bone armor per rank.");
feato(FEAT_COSMIC_UNDERSTANDING,"cosmic understanding",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_CRIPPLING_CRITICAL,"crippling critical",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"duelist level 10","allows your criticals to have random additional effects");
feato(FEAT_CROWN_OF_KNIGHTHOOD,"crown of knighthood",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_DAMAGE_REDUCTION_FS,"damage reduction",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"favored soul level 20","reduces damage by 10 unless dealt by cold iron weapon");
feato(FEAT_DARK_BLESSING,"dark blessing",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_DEATH_ATTACK,"death attack",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Assassin level 1","Chance to kill a target with sneak attack or Paralysis after 3 rounds of hidden study.");
feato(FEAT_DEFENSIVE_STANCE,"defensive stance",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Dwarven Defender level 1","Allows you to fight defensively with bonuses to ac and stats.");
feato(FEAT_DEITY_WEAPON_PROFICIENCY,"deity's weapon proficiency",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"favored soul level 1","allows you to use the weapon of your deity");
feato(FEAT_DEMORALIZE,"demoralize",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_DETECT_GOOD,"detect good",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_DISCERN_LIES,"discern lies",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_DIVINE_BOND,"divine bond",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"paladin level 5","bonuses to attack and damage rolls when active");
feato(FEAT_DIVINER,"diviner",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_DRAGON_APOTHEOSIS,"dragon apotheosis",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_DRAGON_MOUNT_BOOST,"dragon mount boost",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"dragon rider prestige class","gives +18 hp, +10 ac, +1 hit and +1 damage per rank in the feat");
feato(FEAT_DRAGON_MOUNT_BREATH,"dragon mount breath",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"dragon rider prestige class","allows you to use your dragon mount's breath weapon once per rank, per 10 minutes.");
feato(FEAT_ELEMENTAL_IMMUNITY,"elemental immunity",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_ENHANCE_ARROW_ALIGNED,"enhance arrow (aligned)",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"arcane archer level 10","+1d6 holy/unholy damage with bows against different aligned creatures.");
feato(FEAT_ENHANCE_ARROW_DISTANCE,"enhance arrow (distance)",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"arcane archer level 6","doubles range increment on weapon.");
feato(FEAT_ENHANCE_ARROW_ELEMENTAL,"enhance arrow (elemental)",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"arcane archer level 4","+1d6 elemental damage with bows");
feato(FEAT_ENHANCE_ARROW_ELEMENTAL_BURST,"enhance arrow (elemental burst)",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"arcane archer level 8","+2d10 on critical hits with bows");
feato(FEAT_ENHANCE_ARROW_MAGIC,"enhance arrow (magic)",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"arcane archer level 1","+1 to hit and damage with bows per rank");
feato(FEAT_ESSENCE_OF_UNDEATH,"essence of undeath",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"death master","gives immunity to poison, disease, sneak attack and critical hits");
feato(FEAT_FAST_MOVEMENT,"fast movement",FALSE,FALSE,TRUE,FEAT_TYPE_CLASS_ABILITY,"none","10ft bonus to speed in light or medium armor");
feato(FEAT_FAVOR_OF_DARKNESS,"favor of darkness",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_FINAL_STAND,"final stand",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_HASTE,"haste",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"favored soul level 17","can cast haste 3x per day");
feato(FEAT_INCREASED_MULTIPLIER,"increased multiplier",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Weapon Master level 3","Weapons of choice have +1 to their critical multiplier");
feato(FEAT_KI_CRITICAL,"ki critical",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Weapon Master level 7","Weapons of choice have +1 to threat range per rank");
feato(FEAT_KI_DAMAGE,"ki damage",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Weapon Master level 1","Weapons of Choice have 5 percent chance to deal max damage");
feato(FEAT_KNIGHTHOODS_FLOWER,"knighthood's flower",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_KNIGHTLY_COURAGE,"knightly courage",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"none","bonus to fear checks");
feato(FEAT_LEADERSHIP_BONUS,"improved leadership",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_LEARNED_CRAFTER,"learned crafter",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Artisan level 1","Artisan gains exp for crafting items and harvesting");
feato(FEAT_MIGHT_OF_HONOR,"might of honor",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_MOBILE_DEFENSE,"mobile defense",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Dwarven Defender level 8","Allows one to move while in defensive stance");
feato(FEAT_NO_RETREAT,"no retreat",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"duelist level 9","allows you to gain an attack of opportunity against retreating opponents");
feato(FEAT_ONE_THOUGHT,"one thought",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_PARRY,"parry",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"duelist level 2","allows you to parry incoming attacks");
feato(FEAT_POISON_SAVE_BONUS, "poison save bonus",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Assassin level 2","Bonus to all saves against poison.");
feato(FEAT_POISON_USE,"poison use",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Assassin level 1","Trained use in poisons without risk of poisoning self.");
feato(FEAT_PROFICIENT_CRAFTER,"proficient crafter",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Artisan level 2","Increases all crafting skills");
feato(FEAT_PROFICIENT_HARVESTER,"proficient harvester",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Artisan level 4","Increases all harvesting skills");
feato(FEAT_RALLYING_CRY,"rallying cry",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_READ_OMENS,"read omens",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_READ_PORTENTS,"read portents",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_RIPOSTE,"riposte",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"duelist level 5","allows you to gain an attack of opportunity after a successful parry");
feato(FEAT_SACRED_FLAMES,"sacred flames",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"sacred fist level 5","allows you to use innate 'flame weapon' 3 times per 10 minutes");
feato(FEAT_SCAVENGE,"scavenge",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Artisan level 5","Can find materials on corpses");
feato(FEAT_SMITE_GOOD,"smite good",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_SOUL_OF_KNIGHTHOOD,"soul of knighthood",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_STRENGTH_OF_HONOR,"strength of honor",FALSE,FALSE,TRUE,FEAT_TYPE_CLASS_ABILITY,"none","+4 to strength for several rounds");
feato(FEAT_SUMMON_GREATER_UNDEAD,"summon greater undead",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"death master","allows innate use of summon greater undead spell 3x per day");
feato(FEAT_SUMMON_UNDEAD,"summon undead",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"death master","allows innate use of summon undead spell 3x per day");
feato(FEAT_SUPERIOR_WEAPON_FOCUS,"superior weapon focus",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Weapon Master level 5","Weapons of choice have +1 to hit");
feato(FEAT_TOUCH_OF_UNDEATH,"touch of undeath",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"death master","allows for paralytic or instant death touch");
feato(FEAT_TRAP_SENSE,"trap sense",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_UNBREAKABLE_WILL,"unbreakable will",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_UNDEAD_FAMILIAR,"undead familiar",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"death master level 3","allows for undead familiars");
feato(FEAT_WEAPON_OF_CHOICE,"weapons of choice",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Weapon Master level 1","All weapons with weapon focus gain special abilities");
feato(FEAT_WISDOM_OF_THE_MEASURE,"wisdom of the measure",FALSE,FALSE,FALSE,FEAT_TYPE_CLASS_ABILITY,"ask staff","ask staff");
feato(FEAT_BLEEDING_ATTACK,"bleeding attack",FALSE,TRUE,FALSE,FEAT_TYPE_CLASS_ABILITY,"rogue talent","causes bleed damage on living targets who are hit by sneak attack.");
feato(FEAT_OPPORTUNIST,"opportunist",FALSE,TRUE,FALSE,FEAT_TYPE_CLASS_ABILITY,"Rogue level 10","once per round the rogue may make an attack of opportunity against a foe an ally just struck");

/* End Class ability Feats */

/* Racial ability feats */
/* Crystal Dwarf */
feato(FEAT_CRYSTAL_BODY,"crystal body",TRUE,FALSE,FALSE,FEAT_TYPE_INNATE_ABILITY,"!UNDEFINED!","Allows you to harden your crystal-like body for a short time. (Damage reduction 3/-)");
feato(FEAT_CRYSTAL_FIST,"crystal fist",TRUE,FALSE,FALSE,FEAT_TYPE_INNATE_ABILITY,"!UNDEFINED!","Allows you to innately grow jagged and sharp crystals on your arms and legs to enhance damage in melee. (+3 damage)");

/* Various */
feato(FEAT_DARKVISION,"darkvision",TRUE,FALSE,FALSE,FEAT_TYPE_INNATE_ABILITY,"!UNDEFINED!","ask staff");
feato(FEAT_LOW_LIGHT_VISION,"low light vision",TRUE,FALSE,FALSE,FEAT_TYPE_INNATE_ABILITY,"!UNDEFINED!","can see in the dark outside only");

/* End Racial ability feats */

feato(FEAT_LAST_FEAT, "do not take me", FALSE, FALSE, FALSE, FEAT_TYPE_NONE, "placeholder feat", "placeholder feat");

combatfeat(FEAT_IMPROVED_CRITICAL);
combatfeat(FEAT_WEAPON_FINESSE);
combatfeat(FEAT_WEAPON_FOCUS);
combatfeat(FEAT_WEAPON_SPECIALIZATION);
combatfeat(FEAT_GREATER_WEAPON_FOCUS);
combatfeat(FEAT_GREATER_WEAPON_SPECIALIZATION);
combatfeat(FEAT_IMPROVED_WEAPON_FINESSE);
combatfeat(FEAT_MONKEY_GRIP);
combatfeat(FEAT_POWER_CRITICAL);
combatfeat(FEAT_WEAPON_MASTERY);
combatfeat(FEAT_WEAPON_FLURRY);
combatfeat(FEAT_WEAPON_SUPREMACY);

epicfeat(FEAT_EPIC_PROWESS);
epicfeat(FEAT_SWARM_OF_ARROWS);
epicfeat(FEAT_SELF_CONCEALMENT);
epicfeat(FEAT_INTENSIFY_SPELL);
epicfeat(FEAT_EPIC_SPELLCASTING);
epicfeat(FEAT_EPIC_SKILL_FOCUS);
epicfeat(FEAT_GREAT_STRENGTH);
epicfeat(FEAT_GREAT_CONSTITUTION);
epicfeat(FEAT_GREAT_DEXTERITY);
epicfeat(FEAT_GREAT_WISDOM);
epicfeat(FEAT_GREAT_INTELLIGENCE);
epicfeat(FEAT_GREAT_CHARISMA);
epicfeat(FEAT_DAMAGE_REDUCTION);
epicfeat(FEAT_ARMOR_SKIN);
epicfeat(FEAT_FAST_HEALING);
epicfeat(FEAT_ENHANCE_SPELL);
epicfeat(FEAT_GREAT_SMITING);
epicfeat(FEAT_PERFECT_TWO_WEAPON_FIGHTING);
epicfeat(FEAT_SNEAK_ATTACK);
epicfeat(FEAT_SNEAK_ATTACK_OF_OPPORTUNITY);
epicfeat(FEAT_EPIC_DODGE);
epicfeat(FEAT_EPIC_COMBAT_CHALLENGE);
epicfeat(FEAT_EPIC_TOUGHNESS);
epicfeat(FEAT_AUTOMATIC_QUICKEN_SPELL);
epicfeat(FEAT_LAST_FEAT);

  dailyfeat(FEAT_STUNNING_FIST, eSTUNNINGFIST);
  dailyfeat(FEAT_LAYHANDS, eLAYONHANDS);
  dailyfeat(FEAT_RAGE, eRAGE);
  dailyfeat(FEAT_SMITE_EVIL, eSMITE);
  dailyfeat(FEAT_TURN_UNDEAD, eTURN_UNDEAD);
  dailyfeat(FEAT_WILD_SHAPE, eWILD_SHAPE);

}

/* bool meets_prerequisite(ch, prereq)
 * Check to see if ch meets the provided feat prerequisite. */
bool meets_prerequisite(struct char_data *ch, struct feat_prerequisite *prereq, int iarg) {

  switch (prereq->prerequisite_type) {
    case FEAT_PREREQ_NONE:
      /* This is a NON-prereq. */
      break;
    case FEAT_PREREQ_ATTRIBUTE:
      switch (prereq->values[0]) {
        case AB_STR:
          if (ch->real_abils.str < prereq->values[1])
            return FALSE;
          break;
        case AB_DEX:
          if (ch->real_abils.dex < prereq->values[1])
            return FALSE;
          break;
        case AB_CON:
          if (ch->real_abils.con < prereq->values[1])
            return FALSE;
          break;
        case AB_WIS:
          if (ch->real_abils.wis < prereq->values[1])
            return FALSE;
          break;
        case AB_INT:
          if (ch->real_abils.intel < prereq->values[1])
            return FALSE;
          break;
        case AB_CHA:
          if (ch->real_abils.cha < prereq->values[1])
            return FALSE;
          break;
        default:
          log("SYSERR: meets_prerequisite() - Bad Attribute prerequisite %d", prereq->values[0]);
          return FALSE;
      }
    case FEAT_PREREQ_CLASS_LEVEL:
      if (CLASS_LEVEL(ch, prereq->values[0]) < prereq->values[1])
        return FALSE;
      break;
    case FEAT_PREREQ_FEAT:
      if (HAS_FEAT(ch, prereq->values[0]) < prereq->values[1])
        return FALSE;
      break;
    case FEAT_PREREQ_ABILITY:
      if (GET_ABILITY(ch, prereq->values[0]) < prereq->values[1])
        return FALSE;
      break;
    case FEAT_PREREQ_SPELLCASTING:
      switch (prereq->values[0]) {
        case CASTING_TYPE_NONE:
          if (IS_SPELLCASTER(ch))
            return FALSE;
          break;
        case CASTING_TYPE_ARCANE:
          if(!(IS_WIZARD(ch) ||
               IS_SORCERER(ch) ||
               IS_BARD(ch)))
            return FALSE;
/* This stuff is all messed up - fix. */
          if (prereq->values[2] > 0) {
            if (!(comp_slots(ch, CLASS_WIZARD, prereq->values[2]) == 0 ||
                  comp_slots(ch, CLASS_SORCERER, prereq->values[2]) == 0 ||
                  comp_slots(ch, CLASS_BARD, prereq->values[2]) == 0))
              return FALSE;
          }
          break;
        case CASTING_TYPE_DIVINE:
          if(!(IS_CLERIC(ch) ||
               IS_DRUID(ch) ||
               IS_PALADIN(ch) ||
               IS_RANGER(ch)))
            return FALSE;
         if (prereq->values[2] > 0) {
            if (!(comp_slots(ch, CLASS_CLERIC, prereq->values[2]) == 0 ||
                  comp_slots(ch, CLASS_PALADIN, prereq->values[2]) == 0 ||
                  comp_slots(ch, CLASS_DRUID, prereq->values[2]) == 0 ||
                  comp_slots(ch, CLASS_RANGER, prereq->values[2]) == 0))
              return FALSE;
          }
          break;
        case CASTING_TYPE_ANY:
          if(!IS_SPELLCASTER(ch))
            return FALSE;
          if (prereq->values[2] > 0) {
            if (!(comp_slots(ch, CLASS_WIZARD, prereq->values[2]) == 0 ||
                  comp_slots(ch, CLASS_SORCERER, prereq->values[2]) == 0 ||
                  comp_slots(ch, CLASS_BARD, prereq->values[2]) == 0 ||
                  comp_slots(ch, CLASS_CLERIC, prereq->values[2]) == 0 ||
                  comp_slots(ch, CLASS_PALADIN, prereq->values[2]) == 0 ||
                  comp_slots(ch, CLASS_DRUID, prereq->values[2]) == 0 ||
                  comp_slots(ch, CLASS_RANGER, prereq->values[2]) == 0))
             return FALSE;
          }
          break;
        default:
          log("SYSERR: meets_prerequisite() - Bad Casting Type prerequisite %d", prereq->values[0]);
          return FALSE;
      }

      switch (prereq->values[1]) {
        case PREP_TYPE_NONE:
          return FALSE;
        case PREP_TYPE_PREPARED:
          if (!IS_MEM_BASED_CASTER(ch))
            return FALSE;
          break;
        case PREP_TYPE_SPONTANEOUS:
          if (IS_MEM_BASED_CASTER(ch))
            return FALSE;
          break;
        case PREP_TYPE_ANY:
          break;
        default:
          log("SYSERR: meets_prerequisite() - Bad Preparation type prerequisite %d", prereq->values[1]);
          return FALSE;
      }
    case FEAT_PREREQ_RACE:
      if (!IS_NPC(ch) && GET_RACE(ch) != prereq->values[0])
        return FALSE;
      break;
    case FEAT_PREREQ_BAB:
      if (BAB(ch) < prereq->values[0])
        return FALSE;
      break;
    case FEAT_PREREQ_CFEAT:
      /*  SPECIAL CASE - You must have a feat, and it must be the cfeat for the chosen weapon. */
      if (iarg && !has_combat_feat(ch, feat_to_cfeat(prereq->values[0]), iarg))
        return FALSE;
      break;
    case FEAT_PREREQ_WEAPON_PROFICIENCY:
      if (iarg && !is_proficient_with_weapon(ch, iarg))
        return FALSE;
      break;
    default:
      log("SYSERR: meets_prerequisite() - Bad prerequisite type %d", prereq->prerequisite_type);
      return FALSE;
  }

  return TRUE;
}

// The follwing function is used to check if the character satisfies the various prerequisite(s) (if any)
// of a feat in order to learn it.
int feat_is_available(struct char_data *ch, int featnum, int iarg, char *sarg)
{
  struct feat_prerequisite *prereq = NULL;

  if (featnum > NUM_FEATS)
    return FALSE;

  if (feat_list[featnum].epic == TRUE && !IS_EPIC(ch))
    return FALSE;

  if (has_feat(ch, featnum) && !feat_list[featnum].can_stack)
    return FALSE;

  if (feat_list[featnum].prerequisite_list != NULL) {
    /*  This feat has prerequisites. Traverse the list and check. */
    for(prereq = feat_list[featnum].prerequisite_list; prereq != NULL; prereq = prereq->next) {    
      if(meets_prerequisite(ch, prereq, iarg) == FALSE)
        return FALSE;
    }
  } else {

  switch (featnum) {

  case FEAT_AUTOMATIC_QUICKEN_SPELL:
    if (GET_ABILITY(ch, ABILITY_SPELLCRAFT) < 30)
      return FALSE;
    if (comp_slots(ch, CLASS_SORCERER, 9) > 0)
      return TRUE;
    if (comp_slots(ch, CLASS_WIZARD, 9) > 0)
      return TRUE;
    if (comp_slots(ch, CLASS_CLERIC, 9) > 0)
      return TRUE;
    if (comp_slots(ch, CLASS_DRUID, 9) > 0)
      return TRUE;
    return FALSE;

  case FEAT_INTENSIFY_SPELL:
    if (!has_feat(ch, FEAT_MAXIMIZE_SPELL))
      return FALSE;
    if (!has_feat(ch, FEAT_EMPOWER_SPELL))
      return FALSE;
    if (GET_ABILITY(ch, ABILITY_SPELLCRAFT) < 30)
      return FALSE;
    if (comp_slots(ch, CLASS_SORCERER, 9) > 0)
      return TRUE;
    if (comp_slots(ch, CLASS_WIZARD, 9) > 0)
      return TRUE; 
    if (comp_slots(ch, CLASS_CLERIC, 9) > 0)
      return TRUE; 
    if (comp_slots(ch, CLASS_DRUID, 9) > 0)
      return TRUE;
    return FALSE;

  case FEAT_SWARM_OF_ARROWS:
    if (ch->real_abils.dex < 23)
      return FALSE;
    if (!has_feat(ch, FEAT_POINT_BLANK_SHOT))
      return FALSE;
    if (!has_feat(ch, FEAT_RAPID_SHOT))
      return FALSE;
    if (has_feat(ch, FEAT_WEAPON_FOCUS)) /* Need to check for BOW... */    
      return FALSE;
    return TRUE;


  case FEAT_FAST_HEALING:
    if (ch->real_abils.con < 25)
      return FALSE;
    return TRUE;    

  case FEAT_SELF_CONCEALMENT:
    if (GET_ABILITY(ch, ABILITY_HIDE) < 30)
      return FALSE;
    if (GET_ABILITY(ch, ABILITY_TUMBLE) < 30)
      return FALSE;
    if (ch->real_abils.dex < 30)
      return FALSE;
    return TRUE;

  case FEAT_ARMOR_SKIN:
    return TRUE;    

  case FEAT_COMBAT_CHALLENGE:
    if (GET_ABILITY(ch, ABILITY_DIPLOMACY) < 5 &&
        GET_ABILITY(ch, ABILITY_INTIMIDATE) < 5 &&
        GET_ABILITY(ch, ABILITY_BLUFF) < 5)
      return false;
    return true;

  case FEAT_BLEEDING_ATTACK:
  case FEAT_POWERFUL_SNEAK:
    if (CLASS_LEVEL(ch, CLASS_ROGUE) > 1)
      return TRUE;
    return FALSE;

  case FEAT_IMPROVED_COMBAT_CHALLENGE:
    if (GET_ABILITY(ch, ABILITY_DIPLOMACY) < 10 &&
        GET_ABILITY(ch, ABILITY_INTIMIDATE) < 10 &&
        GET_ABILITY(ch, ABILITY_BLUFF) < 10)
      return false;
    if (!has_feat(ch, FEAT_COMBAT_CHALLENGE))
      return false;
    return true;

  case FEAT_GREATER_COMBAT_CHALLENGE:
    if (GET_ABILITY(ch, ABILITY_DIPLOMACY) < 15 &&
        GET_ABILITY(ch, ABILITY_INTIMIDATE) < 15 &&
        GET_ABILITY(ch, ABILITY_BLUFF) < 15)
      return false;
    if (!has_feat(ch, FEAT_IMPROVED_COMBAT_CHALLENGE))
      return false;
    return true;

  case FEAT_EPIC_PROWESS:
    if (has_feat(ch, FEAT_EPIC_PROWESS) >= 5)
      return FALSE;
    return TRUE;

  case FEAT_EPIC_COMBAT_CHALLENGE:
    if (GET_ABILITY(ch, ABILITY_DIPLOMACY) < 20 &&
        GET_ABILITY(ch, ABILITY_INTIMIDATE) < 20 &&
        GET_ABILITY(ch, ABILITY_BLUFF) < 20)
      return false;
    if (!has_feat(ch, FEAT_GREATER_COMBAT_CHALLENGE))
      return false;
    return true;

  case FEAT_NATURAL_SPELL:
      if (ch->real_abils.wis < 13)
          return false;
      if (!has_feat(ch, FEAT_WILD_SHAPE))
          return false;
      return true;

  case FEAT_EPIC_DODGE:
    if (ch->real_abils.dex >= 25 && has_feat(ch, FEAT_DODGE) && has_feat(ch, FEAT_DEFENSIVE_ROLL) && GET_ABILITY(ch, ABILITY_TUMBLE) >= 30)
      return TRUE;
    return FALSE;

  case FEAT_IMPROVED_SNEAK_ATTACK:
    if (has_feat(ch, FEAT_SNEAK_ATTACK) >= 8)
      return TRUE;
    return FALSE;

  case FEAT_SNEAK_ATTACK:
    if (has_feat(ch, FEAT_SNEAK_ATTACK) < 8)
      return FALSE;
    return TRUE;

  case FEAT_SNEAK_ATTACK_OF_OPPORTUNITY:
    if (has_feat(ch, FEAT_SNEAK_ATTACK) < 8)
      return FALSE;
    if (!has_feat(ch, FEAT_OPPORTUNIST))
      return FALSE;
    return TRUE;

  case FEAT_STEADFAST_DETERMINATION:
    if (!has_feat(ch, FEAT_ENDURANCE))
      return FALSE;
    return TRUE;

  case FEAT_GREAT_SMITING:
    if (ch->real_abils.cha >= 25 && has_feat(ch, FEAT_SMITE_EVIL))
      return TRUE;
    return FALSE;

  case FEAT_DIVINE_MIGHT:
  case FEAT_DIVINE_SHIELD:
    if (has_feat(ch, FEAT_TURN_UNDEAD) && has_feat(ch, FEAT_POWER_ATTACK) &&
        ch->real_abils.cha >= 13 && ch->real_abils.str >= 13)
      return TRUE;
    return FALSE;

  case FEAT_DIVINE_VENGEANCE:
    if (has_feat(ch, FEAT_TURN_UNDEAD) && has_feat(ch, FEAT_EXTRA_TURNING))
      return TRUE;
    return FALSE;

  case FEAT_IMPROVED_EVASION:
  case FEAT_CRIPPLING_STRIKE:
  case FEAT_DEFENSIVE_ROLL:
  case FEAT_OPPORTUNIST:
    if (CLASS_LEVEL(ch, CLASS_ROGUE) < 10)
      return FALSE;
    return TRUE;

  case FEAT_EMPOWERED_MAGIC:
  case FEAT_ENHANCED_SPELL_DAMAGE:
    if (IS_SPELLCASTER(ch))
      return TRUE;
    return FALSE;

  case FEAT_AUGMENT_SUMMONING:
    if (has_feat(ch, FEAT_SPELL_FOCUS) && HAS_SCHOOL_FEAT(ch, feat_to_sfeat(FEAT_SPELL_FOCUS), CONJURATION))
      return true;
    return false;

  case FEAT_FASTER_MEMORIZATION:
    if (IS_MEM_BASED_CASTER(ch))
      return TRUE;
    return FALSE;

  case FEAT_DAMAGE_REDUCTION:
/*    if (ch->real_abils.con < 21)
      return FALSE;
    return TRUE;
*/
    return false;

  case FEAT_MONKEY_GRIP:
/*    if (!iarg)
      return TRUE;
    if (!is_proficient_with_weapon(ch, iarg))
      return FALSE;
    return TRUE;
*/
    return false;

  case FEAT_LAST_FEAT:
    return FALSE;

  case FEAT_SLIPPERY_MIND:
    if (CLASS_LEVEL(ch, CLASS_ROGUE) >= 11)
      return TRUE;
    return FALSE;

  case FEAT_LINGERING_SONG:
  case FEAT_EXTRA_MUSIC:
    if (CLASS_LEVEL(ch, CLASS_BARD) > 0)
      return TRUE;
    return FALSE;

  case FEAT_EXTEND_RAGE:
  case FEAT_EXTRA_RAGE:
    if (has_feat(ch, FEAT_RAGE))
      return TRUE;
    return FALSE;

  case FEAT_ABLE_LEARNER:
    return TRUE;

  case FEAT_FAVORED_ENEMY:
    if (has_feat(ch, FEAT_FAVORED_ENEMY_AVAILABLE))
      return TRUE;
    return FALSE;

  case FEAT_IMPROVED_INTIMIDATION:
  	if (GET_ABILITY(ch, ABILITY_INTIMIDATE) < 10)
  		return FALSE;
  	return TRUE;
  	
  case FEAT_IMPROVED_INSTIGATION:
  	if (GET_ABILITY(ch, ABILITY_DIPLOMACY) < 10)
  		return FALSE;
  	return TRUE;
  	
  case FEAT_IMPROVED_TAUNTING:
  	if (GET_ABILITY(ch, ABILITY_BLUFF) < 10)
  		return FALSE;
  	return TRUE;  	  	

  case FEAT_TWO_WEAPON_DEFENSE:
  	if (!has_feat(ch, FEAT_TWO_WEAPON_FIGHTING))
  		return FALSE;
  	if (ch->real_abils.dex < 15)
  		return FALSE;
  	return TRUE;

  case FEAT_IMPROVED_FEINT:
    if (!has_feat(ch, FEAT_COMBAT_EXPERTISE))
      return false;
    return true;
    	

  case FEAT_AURA_OF_GOOD:
    if (CLASS_LEVEL(ch, CLASS_PALADIN))
      return true;
    return false;

  case FEAT_DETECT_EVIL:
    if (CLASS_LEVEL(ch, CLASS_PALADIN))
      return true;
    return false;

  case FEAT_SMITE_EVIL:
    if (CLASS_LEVEL(ch, CLASS_PALADIN))
      return true;
    return false;

  case FEAT_DIVINE_GRACE:
    if (CLASS_LEVEL(ch, CLASS_PALADIN) > 1)
      return true;
    return false;

  case FEAT_LAYHANDS:
    if (CLASS_LEVEL(ch, CLASS_PALADIN) > 1)
      return true;
    return false;

  case FEAT_AURA_OF_COURAGE:
    if (CLASS_LEVEL(ch, CLASS_PALADIN) > 2)
      return true;
    return false;

  case FEAT_DIVINE_HEALTH:
    if (CLASS_LEVEL(ch, CLASS_PALADIN) > 2)
      return true;
    return false;

  case FEAT_TURN_UNDEAD:
    if (CLASS_LEVEL(ch, CLASS_PALADIN) > 3 || CLASS_LEVEL(ch, CLASS_CLERIC))
      return true;
    return false;

  case FEAT_REMOVE_DISEASE:
    if (CLASS_LEVEL(ch, CLASS_PALADIN) > 5)
      return true;
    return false;

  case FEAT_ARMOR_PROFICIENCY_HEAVY:
    if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_MEDIUM))
      return TRUE;
    return FALSE;

  case FEAT_ARMOR_PROFICIENCY_MEDIUM:
    if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_LIGHT))
      return TRUE;
    return FALSE;

  case FEAT_DODGE:
    if (ch->real_abils.dex >= 13)
      return TRUE;
    return FALSE;

  case FEAT_MOBILITY:
    if (has_feat(ch, FEAT_DODGE))
      return TRUE;
    return FALSE;

  case FEAT_WEAPON_PROFICIENCY_BASTARD_SWORD:
    if (BAB(ch) >= 1)
      return TRUE;
    return FALSE;

  case FEAT_IMPROVED_DISARM:
    if (has_feat(ch, FEAT_COMBAT_EXPERTISE))
      return TRUE;
    return FALSE;

  case FEAT_IMPROVED_TRIP:
    if (has_feat(ch, FEAT_COMBAT_EXPERTISE))
      return TRUE;
    return FALSE;

  case FEAT_WHIRLWIND_ATTACK:
    if (!has_feat(ch, FEAT_DODGE))
      return FALSE;
    if (!has_feat(ch, FEAT_MOBILITY))
      return FALSE;
    if (!has_feat(ch, FEAT_SPRING_ATTACK))
      return FALSE;
    if (ch->real_abils.intel < 13)
      return FALSE;
    if (ch->real_abils.dex < 13)
      return FALSE;
    if (BAB(ch) < 4)
      return FALSE;
    return TRUE;

  case FEAT_STUNNING_FIST:
    if (has_feat(ch, FEAT_IMPROVED_UNARMED_STRIKE) && ch->real_abils.str >= 13 && ch->real_abils.dex >= 13 && BAB(ch) >= 8)
      return TRUE;
    if (CLASS_LEVEL(ch, CLASS_MONK) > 0)
      return TRUE;
    return FALSE;
  
  case FEAT_POWER_ATTACK:
    if (ch->real_abils.str >= 13)
      return TRUE;
    return FALSE;

  case FEAT_CLEAVE:
    if (has_feat(ch, FEAT_POWER_ATTACK))
      return TRUE;
    return FALSE;

  case FEAT_GREAT_CLEAVE:
    if (has_feat(ch, FEAT_POWER_ATTACK) && 
        has_feat(ch, FEAT_CLEAVE) &&
        (BAB(ch) >= 4) &&
        (ch->real_abils.str >= 13))
      return TRUE;
    else return FALSE;

  case FEAT_SUNDER:
    if (has_feat(ch, FEAT_POWER_ATTACK))
      return TRUE;
    return FALSE;

  case FEAT_TWO_WEAPON_FIGHTING:
    if (ch->real_abils.dex >= 15)
      return TRUE;
    return FALSE;

  case FEAT_IMPROVED_TWO_WEAPON_FIGHTING:
    if (ch->real_abils.dex >= 17 && has_feat(ch, FEAT_TWO_WEAPON_FIGHTING) && BAB(ch) >= 6)
      return TRUE;
    return FALSE;
   
  case FEAT_GREATER_TWO_WEAPON_FIGHTING:
    if (ch->real_abils.dex >= 19 && has_feat(ch, FEAT_TWO_WEAPON_FIGHTING) && has_feat(ch, FEAT_IMPROVED_TWO_WEAPON_FIGHTING) && BAB(ch) >= 11)
      return TRUE;
    return FALSE;  	
/*
  case FEAT_PERFECT_TWO_WEAPON_FIGHTING:
    if (ch->real_abils.dex >= 25 && has_feat(ch, FEAT_GREATER_TWO_WEAPON_FIGHTING))
      return TRUE;
    return FALSE;
*/
  case FEAT_IMPROVED_CRITICAL:
    if (BAB(ch) < 8)
      return FALSE;
    if (!iarg || is_proficient_with_weapon(ch, iarg))
      return TRUE;
    return FALSE;
/*
  case FEAT_POWER_CRITICAL:
    if (BAB(ch) < 4)
      return FALSE;
    if (!iarg || has_combat_feat(ch, CFEAT_WEAPON_FOCUS, iarg))
      return TRUE;
    return FALSE;

  case FEAT_WEAPON_MASTERY:
    if (BAB(ch) < 8)
      return FALSE;
    if (!iarg)
      return TRUE;
    if (!is_proficient_with_weapon(ch, iarg))
      return FALSE;
    if (!has_combat_feat(ch, CFEAT_WEAPON_FOCUS, iarg))
      return FALSE;
    if (!has_combat_feat(ch, CFEAT_WEAPON_SPECIALIZATION, iarg))
      return FALSE;
    return TRUE;

  case FEAT_WEAPON_FLURRY:
    if (BAB(ch) < 14)
      return FALSE;
    if (!iarg)
      return TRUE;
    if (!is_proficient_with_weapon(ch, iarg))
      return FALSE;
    if (!has_combat_feat(ch, CFEAT_WEAPON_FOCUS, iarg))
      return FALSE;
    if (!has_combat_feat(ch, CFEAT_WEAPON_SPECIALIZATION, iarg))
      return FALSE;
    if (!has_combat_feat(ch, CFEAT_WEAPON_MASTERY, iarg))
      return FALSE;
    return TRUE;

  case FEAT_WEAPON_SUPREMACY:
    if (CLASS_LEVEL(ch, CLASS_WARRIOR) < 17)
      return FALSE;
    if (!iarg)
      return TRUE;
    if (!is_proficient_with_weapon(ch, iarg))
      return FALSE;
    if (!has_combat_feat(ch, CFEAT_WEAPON_FOCUS, iarg))
      return FALSE;
    if (!has_combat_feat(ch, CFEAT_WEAPON_SPECIALIZATION, iarg))
      return FALSE;
    if (!has_combat_feat(ch, CFEAT_GREATER_WEAPON_FOCUS, iarg))
      return FALSE;
    if (!has_combat_feat(ch, CFEAT_GREATER_WEAPON_SPECIALIZATION, iarg))
      return FALSE;
    if (!has_combat_feat(ch, CFEAT_WEAPON_MASTERY, iarg))
      return FALSE;
    return TRUE;

  case FEAT_ROBILARS_GAMBIT:
    if (!HAS_REAL_FEAT(ch, FEAT_COMBAT_REFLEXES))
      return FALSE;
    if (BAB(ch) < 12)
      return FALSE;
    return TRUE;

  case FEAT_KNOCKDOWN:
    if (!HAS_REAL_FEAT(ch, FEAT_IMPROVED_TRIP))
      return FALSE;
    if (BAB(ch) < 4)
      return FALSE;
    return TRUE;
*/
  case FEAT_ARMOR_SPECIALIZATION_LIGHT:
    if (!has_feat(ch, FEAT_ARMOR_PROFICIENCY_LIGHT))
      return FALSE;
    if (BAB(ch) < 12)
      return FALSE;
    return TRUE;

  case FEAT_ARMOR_SPECIALIZATION_MEDIUM:
    if (!has_feat(ch, FEAT_ARMOR_PROFICIENCY_MEDIUM))
      return FALSE;
    if (BAB(ch) < 12)
      return FALSE;
    return TRUE;

  case FEAT_ARMOR_SPECIALIZATION_HEAVY:
    if (!has_feat(ch, FEAT_ARMOR_PROFICIENCY_HEAVY))
      return FALSE;
    if (BAB(ch) < 12)
      return FALSE;
    return TRUE;

  case FEAT_WEAPON_FINESSE:
    if (BAB(ch) < 1)
      return FALSE;
    return TRUE;
/* 
  case FEAT_WEAPON_FOCUS:
    if (BAB(ch) < 1)
      return FALSE;
    if (!iarg || is_proficient_with_weapon(ch, iarg))
      return TRUE;
    return FALSE;
*/
  case FEAT_WEAPON_SPECIALIZATION:
    if (BAB(ch) < 4 || CLASS_LEVEL(ch, CLASS_WARRIOR) < 4)
      return FALSE;
    if (!iarg || is_proficient_with_weapon(ch, iarg))
      return TRUE;
    return FALSE;
/*
  case FEAT_GREATER_WEAPON_FOCUS:
    if (CLASS_LEVEL(ch, CLASS_WARRIOR) < 8)
      return FALSE;
    if (!iarg)
      return TRUE;
    if (is_proficient_with_weapon(ch, iarg) && has_combat_feat(ch, CFEAT_WEAPON_FOCUS, iarg))
      return TRUE;
    return FALSE;
    
  case FEAT_EPIC_SKILL_FOCUS:
    if (!iarg)
      return TRUE;
    if (GET_ABILITY(ch, iarg) >= 20)
      return TRUE;
    return FALSE;

  case  FEAT_IMPROVED_WEAPON_FINESSE:
	if (!has_feat(ch, FEAT_WEAPON_FINESSE))
  	  return FALSE;
  	if (BAB(ch) < 4)
  		return FALSE;
        if (!iarg)
          return TRUE;
  	if (!has_combat_feat(ch, CFEAT_WEAPON_FOCUS, iarg))
  		return FALSE;
        if (weapon_list[iarg].size >= get_size(ch))
          return FALSE;

  	return TRUE;
*/
  case FEAT_GREATER_WEAPON_SPECIALIZATION:
    if (CLASS_LEVEL(ch, CLASS_WARRIOR) < 12)
      return FALSE;
    if (!iarg)
      return TRUE;
    if (is_proficient_with_weapon(ch, iarg) &&
        has_combat_feat(ch, CFEAT_GREATER_WEAPON_FOCUS, iarg) &&
        has_combat_feat(ch, CFEAT_WEAPON_SPECIALIZATION, iarg) &&
        has_combat_feat(ch, CFEAT_WEAPON_FOCUS, iarg))
      return TRUE;
    return FALSE;

  case FEAT_SPELL_FOCUS:
    if (CLASS_LEVEL(ch, CLASS_WIZARD))
      return TRUE;
    return FALSE;

  case FEAT_SPELL_PENETRATION:
    if (GET_LEVEL(ch))
      return TRUE;
    return FALSE;

  case FEAT_BREW_POTION:
    if (GET_LEVEL(ch) >= 3)
      return TRUE;
    return FALSE;

  case FEAT_CRAFT_MAGICAL_ARMS_AND_ARMOR:
    if (GET_LEVEL(ch) >= 5)
      return TRUE;
    return FALSE;

  case FEAT_CRAFT_ROD:
    if (GET_LEVEL(ch) >= 9)
      return TRUE;
    return FALSE;

  case FEAT_CRAFT_STAFF:
    if (GET_LEVEL(ch) >= 12)
      return TRUE;
    return FALSE;

  case FEAT_CRAFT_WAND:
    if (GET_LEVEL(ch) >= 5)
      return TRUE;
    return FALSE;

  case FEAT_FORGE_RING:
    if (GET_LEVEL(ch) >= 5)
      return TRUE;
    return FALSE;

  case FEAT_SCRIBE_SCROLL:
    if (GET_LEVEL(ch) >= 1)
      return TRUE;
    return FALSE;

  case FEAT_EXTEND_SPELL:
    if (IS_SPELLCASTER(ch))
      return TRUE;
    return FALSE;

  case FEAT_HEIGHTEN_SPELL:
    if (CLASS_LEVEL(ch, CLASS_WIZARD))
      return TRUE;
    return FALSE;

  case FEAT_MAXIMIZE_SPELL:
  case FEAT_EMPOWER_SPELL:
    if (IS_SPELLCASTER(ch))
      return TRUE;
    return FALSE;

  case FEAT_QUICKEN_SPELL:
    if (CLASS_LEVEL(ch, CLASS_WIZARD))
      return TRUE;
    return FALSE;

  case FEAT_SILENT_SPELL:
    if (CLASS_LEVEL(ch, CLASS_WIZARD))
      return TRUE;
    return FALSE;

  case FEAT_STILL_SPELL:
    if (CLASS_LEVEL(ch, CLASS_WIZARD))
      return TRUE;
    return FALSE;

  case FEAT_EXTRA_TURNING:
    if (CLASS_LEVEL(ch, CLASS_CLERIC))
      return TRUE;
    return FALSE;

  case FEAT_SPELL_MASTERY:
    if (CLASS_LEVEL(ch, CLASS_WIZARD))
      return TRUE;
    return FALSE;

  case FEAT_IMPROVED_SPELL_RESISTANCE:
    if (has_feat(ch, FEAT_DIAMOND_SOUL))
      return TRUE;
    return FALSE;

  case FEAT_IMPROVED_SHIELD_BASH:
    if (has_feat(ch, FEAT_ARMOR_PROFICIENCY_SHIELD))
      return TRUE;
    return FALSE;

  case FEAT_SHIELD_CHARGE:
    if (!has_feat(ch, FEAT_IMPROVED_SHIELD_BASH) ||
        (BAB(ch) < 3))
      return FALSE;
    return TRUE;

  case FEAT_SHIELD_SLAM:
    if (!has_feat(ch, FEAT_SHIELD_CHARGE) ||
        !has_feat(ch, FEAT_IMPROVED_SHIELD_BASH) ||
        (BAB(ch) < 6))
      return FALSE;
    return TRUE;

  default:
    return TRUE;

  }
  }
  return TRUE;
}

int is_proficient_with_armor(const struct char_data *ch, int armor_type)
{

  int general_type = find_armor_type(armor_type);

  if (armor_type == SPEC_ARMOR_TYPE_CLOTHING)
    return TRUE;

  if (armor_type == SPEC_ARMOR_TYPE_TOWER_SHIELD &&
       !has_feat((char_data *) ch, FEAT_ARMOR_PROFICIENCY_TOWER_SHIELD))
    return FALSE;

  switch (general_type) {
    case ARMOR_TYPE_LIGHT:
      if (has_feat((char_data *) ch, FEAT_ARMOR_PROFICIENCY_LIGHT))
        return TRUE;
    break;
    case ARMOR_TYPE_MEDIUM:
      if (has_feat((char_data *) ch, FEAT_ARMOR_PROFICIENCY_MEDIUM))
        return TRUE;
    break;
    case ARMOR_TYPE_HEAVY:
      if (has_feat((char_data *) ch, FEAT_ARMOR_PROFICIENCY_HEAVY))
        return TRUE;
    break;
    case ARMOR_TYPE_SHIELD:
      if (has_feat((char_data *) ch, FEAT_ARMOR_PROFICIENCY_SHIELD))
        return TRUE;
    break;
    default:
      return TRUE;
  }
  return FALSE;
}

int is_proficient_with_weapon(const struct char_data *ch, int weapon)
{

  /* Adapt this - Focus on an aspect of the divine, not a deity. */
/*  if (has_feat((char_data *) ch, FEAT_DEITY_WEAPON_PROFICIENCY) && weapon == deity_list[GET_DEITY(ch)].favored_weapon)
    return TRUE;
*/  
  if (has_feat((char_data *) ch, FEAT_SIMPLE_WEAPON_PROFICIENCY) &&
      IS_SET(weapon_list[weapon].weaponFlags, WEAPON_FLAG_SIMPLE))
    return TRUE;

  if (has_feat((char_data *) ch, FEAT_MARTIAL_WEAPON_PROFICIENCY) &&
      IS_SET(weapon_list[weapon].weaponFlags, WEAPON_FLAG_MARTIAL))
    return TRUE;

  if (HAS_COMBAT_FEAT(ch, CFEAT_EXOTIC_WEAPON_PROFICIENCY, DAMAGE_TYPE_SLASHING) &&
      IS_SET(weapon_list[weapon].weaponFlags, WEAPON_FLAG_EXOTIC) &&
      IS_SET(weapon_list[weapon].damageTypes, DAMAGE_TYPE_SLASHING)) {
    return TRUE;
  }

  if (HAS_COMBAT_FEAT(ch, CFEAT_EXOTIC_WEAPON_PROFICIENCY, DAMAGE_TYPE_PIERCING) &&
      IS_SET(weapon_list[weapon].weaponFlags, WEAPON_FLAG_EXOTIC) &&
      IS_SET(weapon_list[weapon].damageTypes, DAMAGE_TYPE_PIERCING)) {
    return TRUE;
  }

  if (HAS_COMBAT_FEAT(ch, CFEAT_EXOTIC_WEAPON_PROFICIENCY, DAMAGE_TYPE_BLUDGEONING) &&
      IS_SET(weapon_list[weapon].weaponFlags, WEAPON_FLAG_EXOTIC) &&
      IS_SET(weapon_list[weapon].damageTypes, DAMAGE_TYPE_BLUDGEONING)) {
    return TRUE;
  }


  if (CLASS_LEVEL(ch, CLASS_MONK) && 
      weapon_list[weapon].weaponFamily == WEAPON_FAMILY_MONK)
    return TRUE;

  if (has_feat((char_data *) ch, FEAT_WEAPON_PROFICIENCY_DRUID) || CLASS_LEVEL(ch, CLASS_DRUID) > 0) {
    switch (weapon) {
      case WEAPON_TYPE_CLUB:
      case WEAPON_TYPE_DAGGER:
      case WEAPON_TYPE_QUARTERSTAFF:
      case WEAPON_TYPE_DART:
      case WEAPON_TYPE_SICKLE:
      case WEAPON_TYPE_SCIMITAR:
      case WEAPON_TYPE_SHORTSPEAR:
      case WEAPON_TYPE_SPEAR:
      case WEAPON_TYPE_SLING:
        return TRUE;
    }
  }

  if (CLASS_LEVEL(ch, CLASS_BARD) > 0) {
    switch (weapon) {
      case WEAPON_TYPE_LONG_SWORD:
      case WEAPON_TYPE_RAPIER:
      case WEAPON_TYPE_SAP:
      case WEAPON_TYPE_SHORT_SWORD:
      case WEAPON_TYPE_SHORT_BOW:
      case WEAPON_TYPE_WHIP:
        return TRUE;
    }
  }

  if (has_feat((struct char_data *) ch, FEAT_WEAPON_PROFICIENCY_ROGUE) || CLASS_LEVEL(ch, CLASS_ROGUE) > 0) {
    switch (weapon) {
      case WEAPON_TYPE_HAND_CROSSBOW:
      case WEAPON_TYPE_RAPIER:
      case WEAPON_TYPE_SAP:
      case WEAPON_TYPE_SHORT_SWORD:
      case WEAPON_TYPE_SHORT_BOW:
        return TRUE;
    }
  }

  if (has_feat((struct char_data *)ch, FEAT_WEAPON_PROFICIENCY_WIZARD) || CLASS_LEVEL(ch, CLASS_WIZARD) > 0) {
    switch (weapon) {
      case WEAPON_TYPE_DAGGER:
      case WEAPON_TYPE_QUARTERSTAFF:
      case WEAPON_TYPE_CLUB:
      case WEAPON_TYPE_HEAVY_CROSSBOW:
      case WEAPON_TYPE_LIGHT_CROSSBOW:
        return TRUE;
    }
  }

  if (has_feat((struct char_data *)ch, FEAT_WEAPON_PROFICIENCY_ELF) || IS_ELF(ch)) {
    switch (weapon) {
      case WEAPON_TYPE_LONG_SWORD:
      case WEAPON_TYPE_RAPIER:
      case WEAPON_TYPE_LONG_BOW:
      case WEAPON_TYPE_COMPOSITE_LONGBOW:
      case WEAPON_TYPE_SHORT_BOW:
      case WEAPON_TYPE_COMPOSITE_SHORTBOW:
        return TRUE;
    }
  }

  if (IS_DWARF(ch) && has_feat((struct char_data *)ch, FEAT_MARTIAL_WEAPON_PROFICIENCY)) {
    switch (weapon) {
      case WEAPON_TYPE_DWARVEN_WAR_AXE:
      case WEAPON_TYPE_DWARVEN_URGOSH:
        return TRUE;
    }
  }

  return FALSE;

}
  
/* Helper function for t sort_feats function - not very robust and should not be reused. 
 * SCARY pointer stuff! */
int compare_feats(const void *x, const void *y)
{
  int   a = *(const int *)x,
        b = *(const int *)y;
  
  return strcmp(feat_list[a].name, feat_list[b].name);
}

void sort_feats(void)
{
  int a;

  /* initialize array, avoiding reserved. */
  for (a = 1; a < NUM_FEATS; a++)
    feat_sort_info[a] = a;

  qsort(&feat_sort_info[1], NUM_FEATS, sizeof(int), compare_feats);
}

/*
 *  --------------------------------Known Feats-------------------------------------
 *  Heavy Armor Proficiency       : You are proficient with heavy armor.
 *  Light Armor Proficiency       : You are proficient with light armor.
 *  Medium Armor Proficiency      : You are proficient with medium armor.
 *  Simple Weapon Proficiency     : You are proficient with simple weapons.
 *  Martial Weapon Proficiency    : You are proficient with martial weapons.
 *  Shield Proficiency            : You are proficient with shields and bucklers.
 *  Tower Shield Proficiency      : You are proficient with tower shields.
 *  Power Attack                  : You can make exceptionally powerful attacks.
 *  Cleave                        : You can follow through with powerful blows.
 *  Great Cleave                  : You can slay multiple enemies with each strike.
 *  Tongue of the Sun and the Moon: You can speak any language.
 *  Stacking Feat                 : This feat stacks.
 *  Stacking Feat                 : This feat stacks.
 *  --------------------------------------------------------------------------------
 *
 *  The short description of the feats must not be longer than 47 characters.
 *
 *  --------------------------------Known Feats-------------------------------------
 *  Heavy Armor Proficiency             Simple Weapon Proficiency  
 *  Light Armor Proficiency             Martial Weapon Proficiency 
 *  Medium Armor Proficiency            Shield Proficiency
 *  Shield Proficiency                  Stackable Feat
 *  Stackable Feat                      Weapon Focus (Greatsword) 
 *  -------------------------------------------------------------------------------- 
 */
void list_feats(struct char_data *ch, char *arg, int list_type) 
{
  int i, sortpos, j;
  int none_shown = TRUE;
  int mode = 0;
  char buf [MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH], buf3[150];
  int count = 0;
  int subfeat;

  int line_length = 80; /* Width of the display. */

  if (*arg && is_abbrev(arg, "descriptions")) {
    mode = 1;
  }
  
  /* Header bar */
  if (list_type == LIST_FEATS_KNOWN)
    sprintf(buf + strlen(buf), "\tC%s\tn", text_line_string("\tYKnown Feats\tC", line_length, '-', '-'));
  if (list_type == LIST_FEATS_AVAILABLE)
    sprintf(buf + strlen(buf), "\tC%s\tn", text_line_string("\tYAvailable Feats\tC", line_length, '-', '-'));
  if (list_type == LIST_FEATS_ALL)
    sprintf(buf + strlen(buf), "\tC%s\tn", text_line_string("\tYAll Feats\tC", line_length, '-', '-'));

  strcpy(buf2, buf);

  for (sortpos = 1; sortpos < NUM_FEATS; sortpos++) {

    if (strlen(buf2) > MAX_STRING_LENGTH -32)
      break;

    i = feat_sort_info[sortpos];
    /*  Print the feat, depending on the type of list. */
    if ((list_type == LIST_FEATS_KNOWN     && (has_feat(ch, i)  && feat_list[i].in_game)) ||
        (list_type == LIST_FEATS_AVAILABLE && (feat_is_available(ch, i, 0, NULL) && feat_list[i].in_game && feat_list[i].can_learn)) ||
        (list_type == LIST_FEATS_ALL       && (feat_list[i].in_game)))
        
    {
      if ((subfeat = feat_to_sfeat(i)) != -1) {
        /* This is a 'school feat' */
        for (j = 1; j < NUM_SCHOOLS; j++) {         
          if (HAS_SCHOOL_FEAT(ch, subfeat, j)) {
            if (mode == 1) { /* description mode */
              sprintf(buf3, "%s (%s)", feat_list[i].name, spell_schools[j]);
              sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
            } else {
              sprintf(buf3, "%s (%s) ", feat_list[i].name, spell_schools[j]);
              sprintf(buf, "%-40s ", buf3);
            }
            strcat(buf2, buf);
            none_shown = FALSE;
  
          }
        }
      } else 
      if ((subfeat = feat_to_cfeat(i)) != -1) {
        /* This is a 'combat feat' */
        for (j = 1; j < NUM_WEAPON_TYPES; j++) {
          if (HAS_COMBAT_FEAT(ch, subfeat, j)) {
            if (mode == 1) {
              sprintf(buf3, "%s (%s)", feat_list[i].name, weapon_list[j].name);
              sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
            } else {
              sprintf(buf3, "%s (%s) ", feat_list[i].name, weapon_list[j].name);
              sprintf(buf, "%-40s ", buf3);
            }
            strcat(buf2, buf);
            none_shown = FALSE;
            
          }
        }
      } else
      if ((subfeat = feat_to_skfeat(i)) != -1) {
        /* This is a 'skill' feat */
        for (j = 1; j < NUM_ABILITIES; j++) {
          if (ch->player_specials->saved.skill_focus[i][j] > 0) {
            if (mode == 1) {
              sprintf(buf3, "%s (%s)", feat_list[i].name, ability_names[j]);
              sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
            } else {
              sprintf(buf3, "%s (%s)", feat_list[i].name, ability_names[j]);
              sprintf(buf, "%-40s ", buf3);
            }
            strcat(buf2, buf);
            none_shown = FALSE;
          }
        }
      } else if (i == FEAT_FAST_HEALING) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d hp/round)", feat_list[i].name, has_feat(ch, FEAT_FAST_HEALING) * 3);
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d hp/round)", feat_list[i].name, has_feat(ch, FEAT_FAST_HEALING) * 3);
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_DAMAGE_REDUCTION) {
          if (mode == 1) {
            sprintf(buf3, "%s (%d/-)", feat_list[i].name, has_feat(ch, FEAT_DAMAGE_REDUCTION));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (%d/-)", feat_list[i].name, has_feat(ch, FEAT_DAMAGE_REDUCTION));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_ARMOR_SKIN) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d ac)", feat_list[i].name, has_feat(ch, FEAT_ARMOR_SKIN));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d ac)", feat_list[i].name, has_feat(ch, FEAT_ARMOR_SKIN));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_ENERGY_RESISTANCE) {
          if (mode == 1) {
            sprintf(buf3, "%s (%d/-)", feat_list[i].name, has_feat(ch, FEAT_ENERGY_RESISTANCE) * 3);
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (%d/-)", feat_list[i].name, has_feat(ch, FEAT_ENERGY_RESISTANCE) * 3);
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_HASTE) {
          if (mode == 1) {
            sprintf(buf3, "%s (3x/day)", feat_list[i].name);
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (3x/day)", feat_list[i].name);
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_SACRED_FLAMES) {
          if (mode == 1) {
            sprintf(buf3, "%s (3x/day)", feat_list[i].name);
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (3x/day)", feat_list[i].name);
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_DRAGON_MOUNT_BREATH) {
          if (mode == 1) {
            sprintf(buf3, "%s (%dx/day)", feat_list[i].name, has_feat(ch, FEAT_DRAGON_MOUNT_BREATH));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (%dx/day)", feat_list[i].name, has_feat(ch, FEAT_DRAGON_MOUNT_BREATH));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_DRAGON_MOUNT_BOOST) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_DRAGON_MOUNT_BOOST));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_DRAGON_MOUNT_BOOST));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_BREATH_WEAPON) {
          if (mode == 1) {
            sprintf(buf3, "%s (%dd8 dmg|%dx/day)", feat_list[i].name, has_feat(ch, FEAT_BREATH_WEAPON), HAS_FEAT(ch, FEAT_BREATH_WEAPON));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (%dd8 dmg|%dx/day)", feat_list[i].name, has_feat(ch, FEAT_BREATH_WEAPON), HAS_FEAT(ch, FEAT_BREATH_WEAPON));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_LEADERSHIP) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d%% group exp)", feat_list[i].name, 5 * (1 + has_feat(ch, FEAT_LEADERSHIP)));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d%% group exp)", feat_list[i].name, 5 * (1 + has_feat(ch, FEAT_LEADERSHIP)));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_RAGE) {
          if (mode == 1) {
            sprintf(buf3, "%s (%d / day)", feat_list[i].name, has_feat(ch, FEAT_RAGE));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (%d / day)", feat_list[i].name, has_feat(ch, FEAT_RAGE));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_DEFENSIVE_STANCE) {
          if (mode == 1) {
            sprintf(buf3, "%s (%d / day)", feat_list[i].name, has_feat(ch, FEAT_DEFENSIVE_STANCE));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (%d / day)", feat_list[i].name, has_feat(ch, FEAT_DEFENSIVE_STANCE));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_ENHANCED_SPELL_DAMAGE) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d dam / die)", feat_list[i].name, has_feat(ch, FEAT_ENHANCED_SPELL_DAMAGE));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d dam / die)", feat_list[i].name, has_feat(ch, FEAT_ENHANCED_SPELL_DAMAGE));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_FASTER_MEMORIZATION) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d ranks)", feat_list[i].name, has_feat(ch, FEAT_FASTER_MEMORIZATION));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d ranks)", feat_list[i].name, has_feat(ch, FEAT_FASTER_MEMORIZATION));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_EMPOWERED_MAGIC) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d to dcs)", feat_list[i].name, has_feat(ch, FEAT_EMPOWERED_MAGIC));
            sprintf(buf, "\tW%-30s\tC:\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d to dcs)", feat_list[i].name, has_feat(ch, FEAT_EMPOWERED_MAGIC));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_ENHANCE_SPELL) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d dam dice)", feat_list[i].name, has_feat(ch, FEAT_ENHANCE_SPELL) * 5);
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d dam dice)", feat_list[i].name, has_feat(ch, FEAT_ENHANCE_SPELL) * 5);
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_NATURAL_ARMOR_INCREASE) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d ac)", feat_list[i].name, has_feat(ch, FEAT_NATURAL_ARMOR_INCREASE));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d ac)", feat_list[i].name, has_feat(ch, FEAT_NATURAL_ARMOR_INCREASE));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_GREAT_STRENGTH) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_GREAT_STRENGTH));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_GREAT_STRENGTH));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_GREAT_DEXTERITY) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_GREAT_DEXTERITY));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_GREAT_DEXTERITY));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_GREAT_CONSTITUTION) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_GREAT_CONSTITUTION));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_GREAT_CONSTITUTION));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_GREAT_INTELLIGENCE) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_GREAT_INTELLIGENCE));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_GREAT_INTELLIGENCE));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_GREAT_WISDOM) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_GREAT_WISDOM));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_GREAT_WISDOM));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_GREAT_CHARISMA) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_GREAT_CHARISMA));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_GREAT_CHARISMA));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_POISON_SAVE_BONUS) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_POISON_SAVE_BONUS));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_POISON_SAVE_BONUS));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_SNEAK_ATTACK) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%dd6)", feat_list[i].name, has_feat(ch, FEAT_SNEAK_ATTACK));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%dd6)", feat_list[i].name, has_feat(ch, FEAT_SNEAK_ATTACK));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_SELF_CONCEALMENT) {
          if (mode == 1) {
            sprintf(buf3, "%s (%d%% miss)", feat_list[i].name, has_feat(ch, FEAT_SELF_CONCEALMENT) * 10);
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (%d%% miss)", feat_list[i].name, has_feat(ch, FEAT_SELF_CONCEALMENT) * 10);
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_ENHANCE_ARROW_MAGIC) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_ENHANCE_ARROW_MAGIC));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d)", feat_list[i].name, has_feat(ch, FEAT_ENHANCE_ARROW_MAGIC));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_FAST_CRAFTER) {
          if (mode == 1) {
            sprintf(buf3, "%s (%d%% less time)", feat_list[i].name, has_feat(ch, FEAT_FAST_CRAFTER) * 10);
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (-%d seconds)", feat_list[i].name, has_feat(ch, FEAT_FAST_CRAFTER) * 10);
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_PROFICIENT_CRAFTER) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d to checks)", feat_list[i].name, has_feat(ch, FEAT_PROFICIENT_CRAFTER));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%s (+%d to checks)", feat_list[i].name, has_feat(ch, FEAT_PROFICIENT_CRAFTER));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else if (i == FEAT_PROFICIENT_HARVESTER) {
          if (mode == 1) {
            sprintf(buf3, "%s (+%d to checks)", feat_list[i].name, has_feat(ch, FEAT_PROFICIENT_HARVESTER));
            sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
          } else {
            sprintf(buf3, "%-20s (+%d to checks)", feat_list[i].name, has_feat(ch, FEAT_PROFICIENT_HARVESTER));
            sprintf(buf, "%-40s ", buf3);
          }
          strcat(buf2, buf);
          none_shown = FALSE;
      } else {
        if (mode == 1) {
          sprintf(buf3, "%s", feat_list[i].name);
          sprintf(buf, "\tW%-30s\tC:\tn %s\r\n", buf3, feat_list[i].description);
        } else {
          sprintf(buf, "%-40s ", feat_list[i].name);
        }
        strcat(buf2, buf);        /* The above, ^ should always be safe to do. */
        none_shown = FALSE;
      }
      if (!mode) {
        count++;
        if (count % 2 == 0)
         strcat(buf2, "\r\n");
      }
    }
  }

  if (none_shown) {
    sprintf(buf, "You do not know any feats at this time.\r\n");
    strcat(buf2, buf);
  }

  if (count %2 == 1) /* Only one feat on last row */
    strcat(buf2, "\r\n");

  strcat(buf2, "\tC");
  strcat(buf2, line_string(line_length, '-', '-'));
  strcat(buf2, "\tDSyntax: feats <known|available|all|descriptions>\tn\r\n");

  page_string(ch->desc, buf2, 1);
}

void list_feats_available(struct char_data *ch, char *arg) 
{
  char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH], buf3[150];
  int i, sortpos;
  int none_shown = TRUE;
  int mode = 0;
  int count = 0;

  if (*arg && is_abbrev(arg, "descriptions")) {
    mode = 1;
  }
  else if (*arg && is_abbrev(arg, "requisites")) {
    mode = 2;
  }
  else if (*arg && (is_abbrev(arg, "classfeats") || is_abbrev(arg, "class-feats"))) {
    list_class_feats(ch);
    return;
  }

  if (!GET_FEAT_POINTS(ch))
    strcpy(buf, "\r\nYou cannot learn any feats right now.\r\n");
  else
    sprintf(buf, "\r\nYou can learn %d feat%s and %d class feat%s right now.\r\n",
            GET_FEAT_POINTS(ch), (GET_FEAT_POINTS(ch) == 1 ? "" : "s"), GET_CLASS_FEATS(ch, GET_CLASS(ch)), 
	    (GET_CLASS_FEATS(ch, GET_CLASS(ch)) == 1 ? "" : "s"));
    
    // Display Headings
    sprintf(buf + strlen(buf), "\r\n");
    sprintf(buf + strlen(buf), "\tC-- \tWFeats Available\tC -----------------------------------------\tn\r\n");
    sprintf(buf + strlen(buf), "\r\n");

  strcpy(buf2, buf);

  for (sortpos = 1; sortpos < NUM_FEATS; sortpos++) {
    i = feat_sort_info[sortpos];
    if (strlen(buf2) >= MAX_STRING_LENGTH - 32) {
      strcat(buf2, "**OVERFLOW**\r\n"); 
      break;   
    }
    if (feat_is_available(ch, i, 0, NULL) && feat_list[i].in_game && feat_list[i].can_learn) {
        if (mode == 1) {
          sprintf(buf3, "%s:", feat_list[i].name);
          sprintf(buf, "\tW%-30s\tn %s\r\n", buf3, feat_list[i].description);
        } else if (mode == 2) {
          sprintf(buf3, "%s:", feat_list[i].name);
          sprintf(buf, "\tW%-30s\tn %s\r\n", buf3, feat_list[i].prerequisites);
        } else {
          sprintf(buf, "%-25s ", feat_list[i].name);
        }
      strcat(buf2, buf);        /* The above, ^ should always be safe to do. */
      none_shown = FALSE;

      if (!mode) {
        count++;
        if (count % 3 == 2)
         strcat(buf2, "\r\n");
      }
     
    }
  }

  if (!mode) { 
    if (count % 3 != 2)
      strcat(buf2, "\r\n");
  }
  strcat(buf2, "\r\n");
  

  if (none_shown) {
    sprintf(buf, "There are no feats available for you to learn at this point.\r\n");
    strcat(buf2, buf);
  }

  strcat(buf2, "\tWSyntax: feats <known|available|complete> <description|requisites|classfeats> (both arguments optional)	n\r\n");
   
  page_string(ch->desc, buf2, 1);
}

void list_class_feats(struct char_data *ch)
{

  int featMarker = 1;
  int featCounter = 0;
  int i = 0;
  int sortpos = 0;
  char buf3[100];

    send_to_char(ch, "\r\n");
    send_to_char(ch, "\tC-- \tWClass Feats Available\tC -----------------------------------\tn\r\n");
    send_to_char(ch, "\r\n");
    send_to_char(ch, "\tW%-30s\tn %s\r\n", "Feat Name", "Feat Description");
    send_to_char(ch, "\tW%-30s\tn %s\r\n", " ", "Feat Prerequisites");
    send_to_char(ch, "\r\n");


  for (sortpos = 1; sortpos < NUM_FEATS; sortpos++) {
    i = feat_sort_info[sortpos];
    if (feat_is_available(ch, i, 0, NULL) && feat_list[i].in_game && feat_list[i].can_learn) {
      featMarker = 1;
      featCounter = 0;
      while (featMarker != 0) {
        featMarker = class_bonus_feats[GET_CLASS(ch)][featCounter];
        if (i == featMarker) {
          sprintf(buf3, "%s:", feat_list[i].name);
          send_to_char(ch, "\tW%-30s\tn %s\r\n", buf3, feat_list[featMarker].description);
          send_to_char(ch, "\tW%-30s\tn %s\r\n", " ", feat_list[featMarker].prerequisites);
        }
        featCounter++;
      }
    }  
  }
}

int is_class_feat(int featnum, int class) {

  int i = 0;
  int marker = class_bonus_feats[class][i];

  while (marker != FEAT_UNDEFINED) {
    if (marker == featnum)
      return TRUE;
    marker = class_bonus_feats[class][++i];
  }

  return FALSE;

}

void list_feats_complete(struct char_data *ch, char *arg) 
{

  char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH], buf3[150];
  int i, sortpos;
  int none_shown = TRUE;
  int mode = 0, count = 0;

  if (*arg && is_abbrev(arg, "descriptions")) {
    mode = 1;
  }
  else if (*arg && is_abbrev(arg, "requisites")) {
    mode = 2;
  }
  // Display Headings
  sprintf(buf + strlen(buf), "\r\n");
  sprintf(buf + strlen(buf), "\twComplete Feat List\tn\r\n");
  sprintf(buf + strlen(buf), "\tC-- \tWComplete Feat List\tC --------------------------------------\tn\r\n");
  sprintf(buf + strlen(buf), "\r\n");

  strcpy(buf2, buf);

  for (sortpos = 1; sortpos < NUM_FEATS; sortpos++) {
    i = feat_sort_info[sortpos];
    if (strlen(buf2) >= MAX_STRING_LENGTH - 32) {
      strcat(buf2, "**OVERFLOW**\r\n"); 
      break;   
    }
//	sprintf(buf, "%s : %s\r\n", feat_list[i].name, feat_list[i].in_game ? "In Game" : "Not In Game");
//	strcat(buf2, buf);
    if (feat_list[i].in_game) {
        if (mode == 1) {
          sprintf(buf3, "%s:", feat_list[i].name);
          sprintf(buf, "\tW%-30s\tn %s\r\n", buf3, feat_list[i].description);
        } else if (mode == 2) {
          sprintf(buf3, "%s:", feat_list[i].name);
          sprintf(buf, "\tW%-30s\tn %s\r\n", buf3, feat_list[i].prerequisites);
        } else {
          sprintf(buf, "%-25s ", feat_list[i].name);
        }

      strcat(buf2, buf);        /* The above, ^ should always be safe to do. */
      none_shown = FALSE;

      if (!mode) {
        count++;
        if (count % 3 == 2)
         strcat(buf2, "\r\n");
      }
     
    }
  }
  if (!mode) { 
    if (count % 3 != 2)
      strcat(buf2, "\r\n");
  }
  strcat(buf2, "\r\n");

  if (none_shown) {
    sprintf(buf, "There are currently no feats in the game.\r\n");
    strcat(buf2, buf);
  }

  strcat(buf2, "\tWSyntax: feats <known|available|complete> <description|requisites|classfeats> (both arguments optional)\tn\r\n");
   
  page_string(ch->desc, buf2, 1);
}

int find_feat_num(char *name)
{  
  int index, ok;
  char *temp, *temp2;
  char first[256], first2[256];
   
  for (index = 1; index < NUM_FEATS; index++) {
    if (is_abbrev(name, feat_list[index].name))
      return (index);
    
    ok = TRUE;
    /* It won't be changed, but other uses of this function elsewhere may. */
    temp = any_one_arg((char *)feat_list[index].name, first);
    temp2 = any_one_arg(name, first2);
    while (*first && *first2 && ok) {
      if (!is_abbrev(first2, first))
        ok = FALSE;
      temp = any_one_arg(temp, first);
      temp2 = any_one_arg(temp2, first2);
    }
  
    if (ok && !*first2)
      return (index);
  }
    
  return (-1);
}

/* display_feat_info()
 *
 * Show information about a particular feat, dynamically
 * generated to tailor the output to a particular player.
 *
 * Example feat info :
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 * Feat    : Cleave
 * Type    : Combat
 * ---------------------------------------------------
 * Prerequisites : Power Attack, Str:13
 * Required for  : Greater Cleave
 * ---------------------------------------------------
 * Benefit : If you deal a creature enough damage to 
 * make it drop (typically by dropping it to below 0 
 * hit points or killing it), you get an immediate, 
 * extra melee attack against another engaged 
 * creature.  The extra attack is with the 
 * same weapon and at the same bonus as the attack 
 * that dropped the previous creature.  You can use 
 * this ability once per round.
 *
 * Normal : (none)
 *
 * Special : A fighter may select Cleave as one of his
 * fighter bonus feats.
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-= 
 *
 * (NOTE: The headers of the sections above will be colored
 * differently, making them stand out.) */

void display_feat_info(struct char_data *ch, char *featname) {
  int feat = -1;
  char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];
  
//  static int line_length = 57;
  static int line_length = 80;

  skip_spaces(&featname);
  
  if (!strcmp(featname, "")) {
    send_to_char(ch, "You must provide the name of a feat.\r\n");
    return;
  }

  feat = find_feat_num(featname);

  if (feat == -1 || feat_list[feat].in_game == FALSE) {
    /* Not found - Maybe put in a soundex list here? */
    send_to_char(ch, "Could not find that feat.\r\n");
    return;
  }

  /* We found the feat, and the feat number is stored in 'feat'. */
  
  /* Display the feat info, formatted. */
  send_to_char(ch, "\tC\r\n");
//  draw_line(ch, line_length, '=', '-');
//  draw_line(ch, line_length, '-', '-');
  text_line(ch, "Feat Information", line_length, '-', '-');
//  draw_line(ch, line_length, '-', '-');
  send_to_char(ch, "\tcFeat    : \tn%s\r\n"
                   "\tcType    : \tn%s\r\n",
//                   "\tcCommand : \tn%s\r\n",
                   feat_list[feat].name,
                   feat_types[feat_list[feat].feat_type]
                   );
  send_to_char(ch, "\tC");
  draw_line(ch, line_length, '-', '-');
  
  /*  Here display the prerequisites */
  if (feat_list[feat].prerequisite_list == NULL) {
    sprintf(buf, "\tCPrerequisites : \tnnone\r\n");
  } else {
    bool first = TRUE;
    struct feat_prerequisite *prereq;

    for(prereq = feat_list[feat].prerequisite_list; prereq != NULL; prereq = prereq->next) {
      if (first) {
        first = FALSE;
        sprintf(buf, "\tcPrerequisites : %s%s%s", 
                         (meets_prerequisite(ch, prereq, -1) ? "\tn" : "\tr"), prereq->description, "\tn");
      } else {
        sprintf(buf2, ", %s%s%s", 
                         (meets_prerequisite(ch, prereq, -1) ? "\tn" : "\tr"), prereq->description, "\tn");
        strcat(buf, buf2);
      }
    }
  }
  send_to_char(ch, "%s", strfrmt(buf, line_length, 1, FALSE, FALSE, FALSE));

  send_to_char(ch, "\tC");
  draw_line(ch, line_length, '-', '-');

  /* This we will need to buffer and wrap so that it will fit in the space provided. */
  sprintf(buf, "\tcDescription : \tn%s\r\n",
//                   "\tcNormal : \tn%s\r\n"
//                   "\r\n"
//                   "\tcSpecial : \tn%s\r\n",
                   feat_list[feat].description
//                   "(undefined)",
//                   "(undefined)"
                   );
  send_to_char(ch, strfrmt(buf, line_length, 1, FALSE, FALSE, FALSE));
  send_to_char(ch, "\tC");
  draw_line(ch, line_length, '-', '-');
  send_to_char(ch, "\tn\r\n");             
                
}

/*  do_feats
 *  Overarching command for getting information out of the Luminari Feat system.
 *
 *  feats | feats known  - List all known (learnable) feats 
 *  feat info <featname> - Show detailed information about a partcular feat.
 *  feat all             - List all feats.
 *  feat available       - List all feats for which you qualify
 *  feat category <category name> - List all feats in a specific category
 *
 *  Sample output : 
 *
 *  feats known
 *
 *  --------------------------------Known Feats-------------------------------------
 *  Heavy Armor Proficiency       : You are proficient with heavy armor.
 *  Light Armor Proficiency       : You are proficient with light armor.
 *  Medium Armor Proficiency      : You are proficient with medium armor.
 *  Simple Weapon Proficiency     : You are proficient with simple weapons.
 *  Martial Weapon Proficiency    : You are proficient with martial weapons.
 *  Shield Proficiency            : You are proficient with shields and bucklers.
 *  Tower Shield Proficiency      : You are proficient with tower shields.
 *  Power Attack                  : You can make exceptionally powerful attacks.
 *  Cleave                        : You can follow through with powerful blows.
 *  Great Cleave                  : You can slay multiple enemies with each strike.
 *  Tongue of the Sun and the Moon: You can speak any language.
 *  --------------------------------------------------------------------------------
 *
 *  --------------------------------Known Feats-------------------------------------
 *  Heavy Armor Proficiency             Simple Weapon Proficiency  
 *  Light Armor Proficiency             Martial Weapon Proficiency 
 *  Medium Armor Proficiency            Shield Proficiency
 *  Shield Proficiency                  Stackable Feat
 *  Stackable Feat     
 *  --------------------------------------------------------------------------------
 *
 *  Use the same format for the other listings, other than info.
 *
 *  */
ACMD(do_feats)
{
  char arg[80];
  char arg2[80];
  char *featname;
  
  /*  Have to process arguments like this
   *  because of the syntax - feat info <featname> */
  featname = one_argument(argument, arg);
  one_argument(featname, arg2);
  
  if (is_abbrev(arg, "known") || !*arg) {
    list_feats(ch, arg2, LIST_FEATS_KNOWN);
  } else if (is_abbrev(arg, "info")) {
    display_feat_info(ch, featname);
  } else if (is_abbrev(arg, "available")) {
    list_feats(ch, arg2, LIST_FEATS_AVAILABLE);
  } else if (is_abbrev(arg, "all")) {
    list_feats(ch, arg2, LIST_FEATS_ALL);
  }
}

int feat_to_cfeat(int feat) {
  switch (feat) {
  case FEAT_IMPROVED_CRITICAL:
    return CFEAT_IMPROVED_CRITICAL;
  case FEAT_POWER_CRITICAL:
    return CFEAT_POWER_CRITICAL;
//  case FEAT_WEAPON_FINESSE:
//    return CFEAT_WEAPON_FINESSE;
  case FEAT_WEAPON_FOCUS:
    return CFEAT_WEAPON_FOCUS;
  case FEAT_WEAPON_SPECIALIZATION:
    return CFEAT_WEAPON_SPECIALIZATION;
  case FEAT_GREATER_WEAPON_FOCUS:
    return CFEAT_GREATER_WEAPON_FOCUS;
  case FEAT_GREATER_WEAPON_SPECIALIZATION:
    return CFEAT_GREATER_WEAPON_SPECIALIZATION;
  case FEAT_IMPROVED_WEAPON_FINESSE:
    return CFEAT_IMPROVED_WEAPON_FINESSE;
  case FEAT_EXOTIC_WEAPON_PROFICIENCY:
    return CFEAT_EXOTIC_WEAPON_PROFICIENCY;
  case FEAT_MONKEY_GRIP:
    return CFEAT_MONKEY_GRIP;
  case FEAT_WEAPON_MASTERY:
    return CFEAT_WEAPON_MASTERY;
  case FEAT_WEAPON_FLURRY:
    return CFEAT_WEAPON_FLURRY;
  case FEAT_WEAPON_SUPREMACY:
    return CFEAT_WEAPON_SUPREMACY;
  default:
    return -1;
  }
}
int feat_to_sfeat(int feat) {
  switch (feat) {
  case FEAT_SPELL_FOCUS:
    return SFEAT_SPELL_FOCUS;
  case FEAT_GREATER_SPELL_FOCUS:
    return SFEAT_GREATER_SPELL_FOCUS;
  default:
    return -1;
  }
}

int feat_to_skfeat(int feat) {
  switch (feat) {
  case FEAT_SKILL_FOCUS:
    return SKFEAT_SKILL_FOCUS;
  case FEAT_EPIC_SKILL_FOCUS:
    return SKFEAT_EPIC_SKILL_FOCUS;
  default:
    return -1;
  }
}

void setweapon( int type, char *name, int numDice, int diceSize, int critRange, int critMult, 
int weaponFlags, int cost, int damageTypes, int weight, int range, int weaponFamily, int size, 
int material, int handle_type, int head_type) {

  weapon_type[type] = strdup(name);
  weapon_list[type].name = name;
  weapon_list[type].numDice = numDice;
  weapon_list[type].diceSize = diceSize;
  weapon_list[type].critRange = critRange;
  if (critMult == 2)
    weapon_list[type].critMult = CRIT_X2;
  else if (critMult == 3)
    weapon_list[type].critMult = CRIT_X3;
  else if (critMult == 4)
    weapon_list[type].critMult = CRIT_X4;
  weapon_list[type].weaponFlags = weaponFlags;
  weapon_list[type].cost = cost / 100;
  weapon_list[type].damageTypes = damageTypes;
  weapon_list[type].weight = weight;
  weapon_list[type].range = range;
  weapon_list[type].weaponFamily = weaponFamily;
  weapon_list[type].size = size;
  weapon_list[type].material = material;
  weapon_list[type].handle_type = handle_type;
  weapon_list[type].head_type = head_type;

}

void initialize_weapons(int type) 
{

  weapon_list[type].name = "unused weapon";
  weapon_list[type].numDice = 1;
  weapon_list[type].diceSize = 1;
  weapon_list[type].critRange = 0;
  weapon_list[type].critMult = 1;
  weapon_list[type].weaponFlags = 0;
  weapon_list[type].cost = 0;
  weapon_list[type].damageTypes = 0;
  weapon_list[type].weight = 0;
  weapon_list[type].range = 0;
  weapon_list[type].weaponFamily = 0;
  weapon_list[type].size = 0;
  weapon_list[type].material = 0;
  weapon_list[type].handle_type = 0;
  weapon_list[type].head_type = 0;

}

void load_weapons(void)
{
  int i = 0;

    for (i = 0; i < NUM_WEAPON_TYPES; i++)
        initialize_weapons(i);

/*	setweapon(weapon number, num dam dice, size dam dice, crit range, crit mult, weapon flags, cost, damage type, weight, reach/range, weapon family, weapon size)
*/
	setweapon(WEAPON_TYPE_UNARMED, "unarmed", 1, 3, 0, 2, WEAPON_FLAG_SIMPLE, 200, 
DAMAGE_TYPE_BLUDGEONING, 1, 0, WEAPON_FAMILY_MONK, SIZE_SMALL, MATERIAL_ORGANIC, 
HANDLE_TYPE_GLOVE, HEAD_TYPE_FIST);
	setweapon(WEAPON_TYPE_DAGGER, "dagger", 1, 4, 1, 2, WEAPON_FLAG_THROWN | 
WEAPON_FLAG_SIMPLE, 200, DAMAGE_TYPE_PIERCING, 10, 10, WEAPON_FAMILY_SMALL_BLADE, SIZE_TINY, 
MATERIAL_STEEL, HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_LIGHT_MACE, "light mace", 1, 6, 0, 2, WEAPON_FLAG_SIMPLE, 500, 
DAMAGE_TYPE_BLUDGEONING, 40, 0, WEAPON_FAMILY_CLUB, SIZE_SMALL, MATERIAL_STEEL, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_SICKLE, "sickle", 1, 6, 0, 2, WEAPON_FLAG_SIMPLE, 600, 
DAMAGE_TYPE_SLASHING, 20, 0, WEAPON_FAMILY_SMALL_BLADE, SIZE_SMALL, MATERIAL_STEEL, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_CLUB, "club", 1, 6, 0, 2, WEAPON_FLAG_SIMPLE, 10, 
DAMAGE_TYPE_BLUDGEONING, 30, 0, WEAPON_FAMILY_CLUB, SIZE_SMALL, MATERIAL_WOOD, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_HEAVY_MACE, "heavy mace", 1, 8, 0, 2, WEAPON_FLAG_SIMPLE, 1200, 
DAMAGE_TYPE_BLUDGEONING, 80, 0, WEAPON_FAMILY_CLUB, SIZE_MEDIUM, MATERIAL_STEEL, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_MORNINGSTAR, "morningstar", 1, 8, 0, 2, WEAPON_FLAG_SIMPLE, 800, 
DAMAGE_TYPE_BLUDGEONING | DAMAGE_TYPE_PIERCING, 60, 0, WEAPON_FAMILY_FLAIL, SIZE_MEDIUM, 
MATERIAL_STEEL, HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_SHORTSPEAR, "shortspear", 1, 6, 0, 2, WEAPON_FLAG_SIMPLE | 
WEAPON_FLAG_THROWN, 100, DAMAGE_TYPE_PIERCING, 30, 20, WEAPON_FAMILY_SPEAR, SIZE_MEDIUM, 
MATERIAL_WOOD, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
	setweapon(WEAPON_TYPE_LONGSPEAR, "longspear", 1, 8, 0, 3, WEAPON_FLAG_SIMPLE | 
WEAPON_FLAG_REACH, 500, DAMAGE_TYPE_PIERCING, 90, 0, WEAPON_FAMILY_SPEAR, SIZE_LARGE, 
MATERIAL_WOOD, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
	setweapon(WEAPON_TYPE_QUARTERSTAFF, "quarterstaff", 1, 6, 0, 2, WEAPON_FLAG_SIMPLE,
10, DAMAGE_TYPE_BLUDGEONING, 40, 0, WEAPON_FAMILY_MONK, SIZE_LARGE, 
MATERIAL_WOOD, HANDLE_TYPE_SHAFT, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_SPEAR, "spear", 1, 8, 0, 3, WEAPON_FLAG_SIMPLE | 
WEAPON_FLAG_THROWN | WEAPON_FLAG_REACH, 200, DAMAGE_TYPE_PIERCING, 60, 20, WEAPON_FAMILY_SPEAR, SIZE_LARGE, 
MATERIAL_WOOD, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
	setweapon(WEAPON_TYPE_HEAVY_CROSSBOW, "heavy crossbow", 1, 10, 1, 2, WEAPON_FLAG_SIMPLE 
| WEAPON_FLAG_SLOW_RELOAD | WEAPON_FLAG_RANGED, 5000, DAMAGE_TYPE_PIERCING, 80, 120, 
WEAPON_FAMILY_CROSSBOW, SIZE_LARGE, MATERIAL_WOOD, HANDLE_TYPE_HANDLE, HEAD_TYPE_BOW);
	setweapon(WEAPON_TYPE_LIGHT_CROSSBOW, "light crossbow", 1, 8, 1, 2, WEAPON_FLAG_SIMPLE 
| WEAPON_FLAG_SLOW_RELOAD | WEAPON_FLAG_RANGED, 3500, DAMAGE_TYPE_PIERCING, 40, 80, 
WEAPON_FAMILY_CROSSBOW, SIZE_MEDIUM, MATERIAL_WOOD, HANDLE_TYPE_HANDLE, HEAD_TYPE_BOW);
	setweapon(WEAPON_TYPE_DART, "dart", 1, 4, 0, 2, WEAPON_FLAG_SIMPLE | WEAPON_FLAG_THROWN 
| WEAPON_FLAG_RANGED, 50, DAMAGE_TYPE_PIERCING, 5, 20, WEAPON_FAMILY_THROWN, SIZE_TINY, 
MATERIAL_WOOD, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
	setweapon(WEAPON_TYPE_JAVELIN, "javelin", 1, 6, 0, 2, WEAPON_FLAG_SIMPLE | 
WEAPON_FLAG_THROWN | WEAPON_FLAG_RANGED, 100, DAMAGE_TYPE_PIERCING, 20, 30, 
WEAPON_FAMILY_SPEAR, SIZE_MEDIUM, MATERIAL_WOOD, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
	setweapon(WEAPON_TYPE_SLING, "sling", 1, 4, 0, 2, WEAPON_FLAG_SIMPLE | 
WEAPON_FLAG_RANGED, 10, DAMAGE_TYPE_BLUDGEONING, 1, 50, WEAPON_FAMILY_THROWN, SIZE_SMALL, 
MATERIAL_LEATHER, HANDLE_TYPE_STRAP, HEAD_TYPE_POUCH);
	setweapon(WEAPON_TYPE_THROWING_AXE, "throwing axe", 1, 6, 0, 2, WEAPON_FLAG_MARTIAL | 
WEAPON_FLAG_THROWN, 800, DAMAGE_TYPE_SLASHING, 20, 10, WEAPON_FAMILY_AXE, SIZE_SMALL, 
MATERIAL_STEEL, HANDLE_TYPE_HANDLE, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_LIGHT_HAMMER, "light hammer", 1, 4, 0, 2, WEAPON_FLAG_MARTIAL | 
WEAPON_FLAG_THROWN, 100, DAMAGE_TYPE_BLUDGEONING, 20, 20, WEAPON_FAMILY_HAMMER, SIZE_SMALL, 
MATERIAL_STEEL, HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_HAND_AXE, "hand axe", 1, 6, 0, 3, WEAPON_FLAG_MARTIAL, 600, 
DAMAGE_TYPE_SLASHING, 30, 0, WEAPON_FAMILY_AXE, SIZE_SMALL, MATERIAL_STEEL, HANDLE_TYPE_HANDLE, 
HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_KUKRI, "kukri", 1, 4, 2, 2, WEAPON_FLAG_MARTIAL, 800, 
DAMAGE_TYPE_SLASHING, 20, 0, WEAPON_FAMILY_SMALL_BLADE, SIZE_SMALL, MATERIAL_STEEL, 
HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_LIGHT_PICK, "light pick", 1, 4, 0, 4, WEAPON_FLAG_MARTIAL, 400, 
DAMAGE_TYPE_PIERCING, 30, 0, WEAPON_FAMILY_PICK, SIZE_SMALL, MATERIAL_STEEL, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_SAP, "sap", 1, 6, 0, 2, WEAPON_FLAG_MARTIAL | 
WEAPON_FLAG_NONLETHAL, 100, DAMAGE_TYPE_BLUDGEONING | DAMAGE_TYPE_NONLETHAL, 20, 0, 
WEAPON_FAMILY_CLUB, SIZE_SMALL, MATERIAL_LEATHER, HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_SHORT_SWORD, "short sword", 1, 6, 1, 2, WEAPON_FLAG_MARTIAL, 
1000, DAMAGE_TYPE_PIERCING, 20, 0, WEAPON_FAMILY_SMALL_BLADE, SIZE_SMALL, MATERIAL_STEEL, 
HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_BATTLE_AXE, "battle axe", 1, 8, 0, 3, WEAPON_FLAG_MARTIAL, 1000, 
DAMAGE_TYPE_SLASHING, 60, 0, WEAPON_FAMILY_AXE, SIZE_MEDIUM, MATERIAL_STEEL, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_FLAIL, "flail", 1, 8, 0, 2, WEAPON_FLAG_MARTIAL, 800, 
DAMAGE_TYPE_BLUDGEONING, 50, 0, WEAPON_FAMILY_FLAIL, SIZE_MEDIUM, MATERIAL_STEEL, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_LONG_SWORD, "long sword", 1, 8, 1, 2, WEAPON_FLAG_MARTIAL, 1500, 
DAMAGE_TYPE_SLASHING, 40, 0, WEAPON_FAMILY_MEDIUM_BLADE, SIZE_MEDIUM, MATERIAL_STEEL, 
HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_HEAVY_PICK, "heavy pick", 1, 6, 0, 4, WEAPON_FLAG_MARTIAL, 800, 
DAMAGE_TYPE_PIERCING, 60, 0, WEAPON_FAMILY_PICK, SIZE_MEDIUM, MATERIAL_STEEL, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_RAPIER, "rapier", 1, 6, 2, 2, WEAPON_FLAG_MARTIAL | 
WEAPON_FLAG_BALANCED, 2000, DAMAGE_TYPE_PIERCING, 20, 0, WEAPON_FAMILY_SMALL_BLADE, 
SIZE_SMALL, MATERIAL_STEEL, HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_SCIMITAR, "scimitar", 1, 6, 2, 2, WEAPON_FLAG_MARTIAL, 1500, 
DAMAGE_TYPE_SLASHING, 40, 0, WEAPON_FAMILY_MEDIUM_BLADE, SIZE_MEDIUM, MATERIAL_STEEL, 
HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
/*  	setweapon(WEAPON_TYPE_KHOPESH, "khopesh", 1, 8, 2, 2, WEAPON_FLAG_EXOTIC, 2500, 
DAMAGE_TYPE_SLASHING, 40, 0, WEAPON_FAMILY_MEDIUM_BLADE, SIZE_MEDIUM, MATERIAL_STEEL, 
HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_CURVE_BLADE, "elven curve blade", 1, 10, 2, 2, WEAPON_FLAG_EXOTIC, 6000, 
DAMAGE_TYPE_SLASHING, 70, 0, WEAPON_FAMILY_LARGE_BLADE, SIZE_LARGE, MATERIAL_STEEL, 
HANDLE_TYPE_HILT, HEAD_TYPE_BLADE); */
	setweapon(WEAPON_TYPE_TRIDENT, "trident", 1, 8, 0, 2, WEAPON_FLAG_MARTIAL | 
WEAPON_FLAG_THROWN, 1500, DAMAGE_TYPE_PIERCING, 40, 0, WEAPON_FAMILY_SPEAR, SIZE_MEDIUM, 
MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
	setweapon(WEAPON_TYPE_WARHAMMER, "warhammer", 1, 8, 0, 3, WEAPON_FLAG_MARTIAL, 1200, 
DAMAGE_TYPE_BLUDGEONING, 50, 0, WEAPON_FAMILY_HAMMER, SIZE_MEDIUM, MATERIAL_STEEL, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_FALCHION, "falchion", 2, 4, 2, 2, WEAPON_FLAG_MARTIAL, 7500, 
DAMAGE_TYPE_SLASHING, 80, 0, WEAPON_FAMILY_LARGE_BLADE, SIZE_LARGE, MATERIAL_STEEL, 
HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_GLAIVE, "glaive", 1, 10, 0, 3, WEAPON_FLAG_MARTIAL | 
WEAPON_FLAG_REACH, 800, DAMAGE_TYPE_SLASHING, 100, 0, WEAPON_FAMILY_POLEARM, SIZE_LARGE, 
MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_GREAT_AXE, "great axe", 1, 12, 0, 3, WEAPON_FLAG_MARTIAL, 2000, 
DAMAGE_TYPE_SLASHING, 120, 0, WEAPON_FAMILY_AXE, SIZE_LARGE, MATERIAL_STEEL, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_GREAT_CLUB, "great club", 1, 10, 0, 2, WEAPON_FLAG_MARTIAL, 500, 
DAMAGE_TYPE_BLUDGEONING, 80, 0, WEAPON_FAMILY_CLUB, SIZE_LARGE, MATERIAL_WOOD, 
HANDLE_TYPE_SHAFT, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_HEAVY_FLAIL, "heavy flail", 1, 10, 1, 2, WEAPON_FLAG_MARTIAL, 
1500, DAMAGE_TYPE_BLUDGEONING, 100, 0, WEAPON_FAMILY_FLAIL, SIZE_LARGE, MATERIAL_STEEL, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_GREAT_SWORD, "great sword", 2, 6, 1, 2, WEAPON_FLAG_MARTIAL, 
5000, DAMAGE_TYPE_SLASHING, 80, 0, WEAPON_FAMILY_LARGE_BLADE, SIZE_LARGE, MATERIAL_STEEL, 
HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
/*	setweapon(WEAPON_TYPE_FULLBLADE, "fullblade", 2, 8, 1, 2, WEAPON_FLAG_EXOTIC, 
6000, DAMAGE_TYPE_SLASHING, 100, 0, WEAPON_FAMILY_LARGE_BLADE, SIZE_LARGE, MATERIAL_STEEL, 
HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);*/
	setweapon(WEAPON_TYPE_GUISARME, "guisarme", 2, 4, 0, 3, WEAPON_FLAG_MARTIAL | 
WEAPON_FLAG_REACH, 900, DAMAGE_TYPE_SLASHING, 120, 0, WEAPON_FAMILY_POLEARM, SIZE_LARGE, 
MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_HALBERD, "halberd", 1, 10, 0, 3, WEAPON_FLAG_MARTIAL | 
WEAPON_FLAG_REACH, 1000, DAMAGE_TYPE_SLASHING | DAMAGE_TYPE_PIERCING, 120, 0, 
WEAPON_FAMILY_POLEARM, SIZE_LARGE, MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_LANCE, "lance", 1, 8, 0, 3, WEAPON_FLAG_MARTIAL | 
WEAPON_FLAG_REACH | WEAPON_FLAG_CHARGE, 1000, DAMAGE_TYPE_PIERCING, 100, 0, 
WEAPON_FAMILY_POLEARM, SIZE_LARGE, MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
	setweapon(WEAPON_TYPE_RANSEUR, "ranseur", 2, 4, 0, 3, WEAPON_FLAG_MARTIAL | 
WEAPON_FLAG_REACH, 1000, DAMAGE_TYPE_PIERCING, 100, 0, WEAPON_FAMILY_POLEARM, SIZE_LARGE, 
MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_POINT);
	setweapon(WEAPON_TYPE_SCYTHE, "scythe", 2, 4, 0, 4, WEAPON_FLAG_MARTIAL, 1800, 
DAMAGE_TYPE_SLASHING | DAMAGE_TYPE_PIERCING, 100, 0, WEAPON_FAMILY_POLEARM, SIZE_LARGE, 
MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_LONG_BOW, "long bow", 1, 8, 0, 3, WEAPON_FLAG_MARTIAL | 
WEAPON_FLAG_RANGED, 7500, DAMAGE_TYPE_PIERCING, 30, 100, WEAPON_FAMILY_BOW, SIZE_MEDIUM, 
MATERIAL_WOOD, HANDLE_TYPE_STRING, HEAD_TYPE_BOW);
	setweapon(WEAPON_TYPE_COMPOSITE_LONGBOW, "composite long bow", 1, 8, 0, 3, 
WEAPON_FLAG_MARTIAL | WEAPON_FLAG_RANGED, 10000, DAMAGE_TYPE_PIERCING, 30, 110, 
WEAPON_FAMILY_BOW, SIZE_MEDIUM, MATERIAL_WOOD, HANDLE_TYPE_STRING, HEAD_TYPE_BOW);
/*	setweapon(WEAPON_TYPE_GREATBOW, "great bow", 1, 12, 0, 3, 
WEAPON_FLAG_EXOTIC | WEAPON_FLAG_RANGED, 10000, DAMAGE_TYPE_PIERCING, 30, 200, 
WEAPON_FAMILY_BOW, SIZE_MEDIUM, MATERIAL_WOOD, HANDLE_TYPE_STRING, HEAD_TYPE_BOW);*/
	setweapon(WEAPON_TYPE_SHORT_BOW, "short bow", 1, 6, 0, 3, WEAPON_FLAG_MARTIAL | 
WEAPON_FLAG_RANGED, 3000, DAMAGE_TYPE_PIERCING, 20, 60, WEAPON_FAMILY_BOW, SIZE_SMALL, 
MATERIAL_WOOD, HANDLE_TYPE_STRING, HEAD_TYPE_BOW);
	setweapon(WEAPON_TYPE_COMPOSITE_SHORTBOW, "composite short bow", 1, 6, 0, 3, 
WEAPON_FLAG_MARTIAL | WEAPON_FLAG_RANGED, 7500, DAMAGE_TYPE_PIERCING, 20, 70, 
WEAPON_FAMILY_BOW, SIZE_SMALL, MATERIAL_WOOD, HANDLE_TYPE_STRING, HEAD_TYPE_BOW);
	setweapon(WEAPON_TYPE_KAMA, "kama", 1, 6, 0, 2, WEAPON_FLAG_EXOTIC, 200, 
DAMAGE_TYPE_SLASHING, 20, 0,  WEAPON_FAMILY_MONK, SIZE_SMALL, MATERIAL_STEEL, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_NUNCHAKU, "nunchaku", 1, 6, 1, 2, WEAPON_FLAG_EXOTIC, 200, 
DAMAGE_TYPE_BLUDGEONING, 20, 0, WEAPON_FAMILY_MONK, SIZE_SMALL, MATERIAL_WOOD, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_SAI, "sai", 1, 4, 1, 2, WEAPON_FLAG_EXOTIC | WEAPON_FLAG_THROWN, 
100, DAMAGE_TYPE_BLUDGEONING, 10, 10, WEAPON_FAMILY_MONK, SIZE_SMALL, MATERIAL_STEEL, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_POINT);
	setweapon(WEAPON_TYPE_SIANGHAM, "siangham", 1, 6, 1, 2, WEAPON_FLAG_EXOTIC, 300, 
DAMAGE_TYPE_PIERCING, 10, 0, WEAPON_FAMILY_MONK, SIZE_SMALL, MATERIAL_STEEL, 
HANDLE_TYPE_HANDLE, HEAD_TYPE_POINT);
	setweapon(WEAPON_TYPE_BASTARD_SWORD, "bastard sword", 1, 10, 1, 2, WEAPON_FLAG_EXOTIC, 
3500, DAMAGE_TYPE_SLASHING, 60, 0, WEAPON_FAMILY_MEDIUM_BLADE, SIZE_MEDIUM, MATERIAL_STEEL, 
HANDLE_TYPE_HILT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_DWARVEN_WAR_AXE, "dwarven war axe", 1, 10, 0, 3, 
WEAPON_FLAG_EXOTIC, 3000, DAMAGE_TYPE_SLASHING, 80, 0, WEAPON_FAMILY_AXE, SIZE_MEDIUM, 
MATERIAL_STEEL, HANDLE_TYPE_HANDLE, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_WHIP, "whip", 1, 3, 0, 2, WEAPON_FLAG_EXOTIC | WEAPON_FLAG_REACH 
| WEAPON_FLAG_DISARM | WEAPON_FLAG_TRIP, 100, DAMAGE_TYPE_SLASHING, 20, 0, WEAPON_FAMILY_WHIP, 
SIZE_MEDIUM, MATERIAL_LEATHER, HANDLE_TYPE_HANDLE, HEAD_TYPE_CORD);
	setweapon(WEAPON_TYPE_SPIKED_CHAIN, "spiked chain", 2, 4, 0, 2, WEAPON_FLAG_EXOTIC | 
WEAPON_FLAG_REACH | WEAPON_FLAG_DISARM | WEAPON_FLAG_TRIP, 2500, DAMAGE_TYPE_PIERCING, 100, 0, 
WEAPON_FAMILY_WHIP, SIZE_LARGE, MATERIAL_STEEL, HANDLE_TYPE_GRIP, HEAD_TYPE_CHAIN);
	setweapon(WEAPON_TYPE_DOUBLE_AXE, "double-headed axe", 1, 8, 0, 3, WEAPON_FLAG_EXOTIC | 
WEAPON_FLAG_DOUBLE, 6500, DAMAGE_TYPE_SLASHING, 150, 0, WEAPON_FAMILY_DOUBLE, SIZE_LARGE, 
MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_DIRE_FLAIL, "dire flail", 1, 8, 0, 2, WEAPON_FLAG_EXOTIC | 
WEAPON_FLAG_DOUBLE, 9000, DAMAGE_TYPE_BLUDGEONING, 100, 0, WEAPON_FAMILY_DOUBLE, SIZE_LARGE, 
MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_HOOKED_HAMMER, "hooked hammer", 1, 6, 0, 4, WEAPON_FLAG_EXOTIC | 
WEAPON_FLAG_DOUBLE, 2000, DAMAGE_TYPE_PIERCING | DAMAGE_TYPE_BLUDGEONING, 60, 0, 
WEAPON_FAMILY_DOUBLE, SIZE_LARGE, MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_HEAD);
	setweapon(WEAPON_TYPE_2_BLADED_SWORD, "two-bladed sword", 1, 8, 1, 2, 
WEAPON_FLAG_EXOTIC | WEAPON_FLAG_DOUBLE, 10000, DAMAGE_TYPE_SLASHING, 100, 0, 
WEAPON_FAMILY_DOUBLE, SIZE_LARGE, MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_DWARVEN_URGOSH, "dwarven urgosh", 1, 7, 0, 3, WEAPON_FLAG_EXOTIC 
| WEAPON_FLAG_DOUBLE, 5000, DAMAGE_TYPE_PIERCING | DAMAGE_TYPE_SLASHING, 120, 0, 
WEAPON_FAMILY_DOUBLE, SIZE_LARGE, MATERIAL_STEEL, HANDLE_TYPE_SHAFT, HEAD_TYPE_BLADE);
	setweapon(WEAPON_TYPE_HAND_CROSSBOW, "hand crossbow", 1, 4, 1, 2, WEAPON_FLAG_EXOTIC | 
WEAPON_FLAG_RANGED, 10000, DAMAGE_TYPE_PIERCING, 20, 30, WEAPON_FAMILY_CROSSBOW, SIZE_SMALL, 
MATERIAL_WOOD, HANDLE_TYPE_HANDLE, HEAD_TYPE_BOW);
	setweapon(WEAPON_TYPE_HEAVY_REP_XBOW, "heavy repeating crossbow", 1, 10, 1, 2, 
WEAPON_FLAG_EXOTIC | WEAPON_FLAG_RANGED | WEAPON_FLAG_REPEATING, 40000, DAMAGE_TYPE_PIERCING, 120, 120, 
WEAPON_FAMILY_CROSSBOW, SIZE_LARGE, MATERIAL_WOOD, HANDLE_TYPE_HANDLE, HEAD_TYPE_BOW);
	setweapon(WEAPON_TYPE_LIGHT_REP_XBOW, "light repeating crossbow", 1, 8, 1, 2, 
WEAPON_FLAG_EXOTIC | WEAPON_FLAG_RANGED, 25000, DAMAGE_TYPE_PIERCING, 60, 80, 
WEAPON_FAMILY_CROSSBOW, SIZE_MEDIUM, MATERIAL_WOOD, HANDLE_TYPE_HANDLE, HEAD_TYPE_BOW);
	setweapon(WEAPON_TYPE_BOLA, "bola", 1, 4, 0, 2, WEAPON_FLAG_EXOTIC | WEAPON_FLAG_THROWN 
| WEAPON_FLAG_TRIP, 500, DAMAGE_TYPE_BLUDGEONING, 20, 10, WEAPON_FAMILY_THROWN, SIZE_MEDIUM, 
MATERIAL_LEATHER, HANDLE_TYPE_GRIP, HEAD_TYPE_CORD);
	setweapon(WEAPON_TYPE_NET, "net", 1, 1, 0, 1, WEAPON_FLAG_EXOTIC | WEAPON_FLAG_THROWN | 
WEAPON_FLAG_ENTANGLE, 2000, DAMAGE_TYPE_BLUDGEONING, 60, 10, WEAPON_FAMILY_THROWN, SIZE_LARGE, 
MATERIAL_LEATHER, HANDLE_TYPE_GRIP, HEAD_TYPE_MESH);
	setweapon(WEAPON_TYPE_SHURIKEN, "shuriken", 1, 2, 0, 2, WEAPON_FLAG_EXOTIC | 
WEAPON_FLAG_THROWN, 20, DAMAGE_TYPE_PIERCING, 5, 10, WEAPON_FAMILY_MONK, SIZE_SMALL, 
MATERIAL_STEEL, HANDLE_TYPE_GRIP, HEAD_TYPE_BLADE); 



}

void setarmor(int type, char *name, int armorType, int cost, int armorBonus, int dexBonus, int armorCheck, int spellFail, int thirtyFoot, int twentyFoot, int weight, int material)
{

  armor_list[type].name = name;
  armor_list[type].armorType = armorType;
  armor_list[type].cost = cost / 100;
  armor_list[type].armorBonus = armorBonus;
  armor_list[type].dexBonus = dexBonus;
  armor_list[type].armorCheck = armorCheck;
  armor_list[type].spellFail = spellFail;
  armor_list[type].thirtyFoot = thirtyFoot;
  armor_list[type].twentyFoot = twentyFoot;
  armor_list[type].weight = weight;
  armor_list[type].material = material;

}

void initialize_armor(int type)
{

  armor_list[type].name = "unused armor";
  armor_list[type].armorType = 0;
  armor_list[type].cost = 0;
  armor_list[type].armorBonus = 0;
  armor_list[type].dexBonus = 0;
  armor_list[type].armorCheck = 0;
  armor_list[type].spellFail = 0;
  armor_list[type].thirtyFoot = 0;
  armor_list[type].twentyFoot = 0;
  armor_list[type].weight = 0;
  armor_list[type].material = 0;
}

void load_armor(void) {

    int i = 0;

    for (i = 0; i <= NUM_SPEC_ARMOR_TYPES; i++)
	    initialize_armor(i);
	
	setarmor(SPEC_ARMOR_TYPE_CLOTHING, "clothing", ARMOR_TYPE_NONE, 100, 0, 999, 0, 0, 30, 20, 100, MATERIAL_COTTON);
	setarmor(SPEC_ARMOR_TYPE_PADDED, "padded armor", ARMOR_TYPE_LIGHT, 500, 10, 8, 0, 5, 30, 20, 100, MATERIAL_COTTON);
	setarmor(SPEC_ARMOR_TYPE_LEATHER, "leather armor", ARMOR_TYPE_LIGHT, 1000, 20, 6, 0, 10, 30, 20, 150, MATERIAL_LEATHER);
	setarmor(SPEC_ARMOR_TYPE_STUDDED_LEATHER, "studded leather armor", ARMOR_TYPE_LIGHT, 2500, 30, 5, -1, 15, 30, 20, 200, MATERIAL_LEATHER);
	setarmor(SPEC_ARMOR_TYPE_LIGHT_CHAIN, "light chainmail armor", ARMOR_TYPE_LIGHT, 10000, 40, 4, -2, 20, 30, 20, 250, MATERIAL_STEEL);
	setarmor(SPEC_ARMOR_TYPE_HIDE, "hide armor", ARMOR_TYPE_MEDIUM, 1500, 30, 4, -3, 20, 20, 15, 250, MATERIAL_LEATHER);
	setarmor(SPEC_ARMOR_TYPE_SCALE, "scale armor", ARMOR_TYPE_MEDIUM, 5000, 40, 3, -4, 25, 20, 15, 300, MATERIAL_STEEL);
	setarmor(SPEC_ARMOR_TYPE_CHAINMAIL, "chainmail armor", ARMOR_TYPE_MEDIUM, 15000, 50, 2, -5, 30, 20, 15, 400, MATERIAL_STEEL);
	setarmor(SPEC_ARMOR_TYPE_PIECEMEAL, "piecemeal armor", ARMOR_TYPE_MEDIUM, 20000, 50, 3, -4, 25, 20, 15, 300, MATERIAL_STEEL);
	setarmor(SPEC_ARMOR_TYPE_SPLINT, "splint mail armor", ARMOR_TYPE_HEAVY, 20000, 60, 0, -7, 40, 20, 15, 450, MATERIAL_STEEL);
	setarmor(SPEC_ARMOR_TYPE_BANDED, "banded mail armor", ARMOR_TYPE_HEAVY, 25000, 60, 1, -6, 35, 20, 15, 350, MATERIAL_STEEL);
	setarmor(SPEC_ARMOR_TYPE_HALF_PLATE, "half plate armor", ARMOR_TYPE_HEAVY, 60000, 70, 1, -6, 40, 20, 15, 500, MATERIAL_STEEL);
	setarmor(SPEC_ARMOR_TYPE_FULL_PLATE, "full plate armor", ARMOR_TYPE_HEAVY, 150000, 80, 1, -6, 35, 20, 15, 500, MATERIAL_STEEL);
	setarmor(SPEC_ARMOR_TYPE_BUCKLER, "buckler shield", ARMOR_TYPE_SHIELD, 1500, 10, 99, -1, 5, 999, 999, 50, MATERIAL_WOOD);
	setarmor(SPEC_ARMOR_TYPE_SMALL_SHIELD, "small shield", ARMOR_TYPE_SHIELD, 900, 10, 99, -1, 5, 999, 999, 60, MATERIAL_WOOD);
	setarmor(SPEC_ARMOR_TYPE_LARGE_SHIELD, "heavy shield", ARMOR_TYPE_SHIELD, 2000, 20, 99, -2, 15, 999,999, 150, MATERIAL_WOOD);
	setarmor(SPEC_ARMOR_TYPE_TOWER_SHIELD, "tower shield", ARMOR_TYPE_SHIELD, 3000, 40, 2, -10, 50, 999, 999, 450, MATERIAL_WOOD);
}

int has_feat(struct char_data *ch, int featnum) {

  if (ch->desc && LEVELUP(ch)) {
    return (HAS_FEAT(ch, featnum) + LEVELUP(ch)->feats[featnum]);
  } 

/*
  struct obj_data *obj;
  int i = 0, j = 0;

  for (j = 0; j < NUM_WEARS; j++) {
    if ((obj = GET_EQ(ch, j)) == NULL)
      continue;
    for (i = 0; i < 6; i++) {
      if (obj->affected[i].location == APPLY_FEAT && obj->affected[i].specific == featnum)
        return (has_feat(ch, featnum) + obj->affected[i].modifier);
    }
  }
*/
  return HAS_FEAT(ch, featnum);
}

int has_combat_feat(struct char_data *ch, int i, int j) {

	if (ch->desc && LEVELUP(ch)) {
		if ((IS_SET_AR(LEVELUP(ch)->combat_feats[(i)], (j))))
			return TRUE;
	}

	if ((IS_SET_AR((ch)->char_specials.saved.combat_feats[(i)], (j))))
		return TRUE;

	return FALSE;
}

#ifdef COMPILE_D20_FEATS
int has_weapon_feat(struct char_data *ch, int i, int j) {
  return has_weapon_feat_full(ch, i, j, TRUE);
}

int has_weapon_feat_full(struct char_data *ch, int i, int j, int display) {

  struct obj_data *obj = GET_EQ(ch, WEAR_WIELD);
  int k = 0;

  if (obj) {
    if (display && HAS_COMBAT_FEAT(ch, feat_to_cfeat(i), DAMAGE_TYPE_SLASHING) &&
         (IS_SET(weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes, DAMAGE_TYPE_SLASHING) ||
          IS_SET(weapon_list[k].damageTypes, DAMAGE_TYPE_SLASHING) ||
          weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes == DAMAGE_TYPE_SLASHING))
      return TRUE;
    if (display && HAS_COMBAT_FEAT(ch, feat_to_cfeat(i), DAMAGE_TYPE_BLUDGEONING) &&
         (IS_SET(weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes, DAMAGE_TYPE_BLUDGEONING) ||
          IS_SET(weapon_list[k].damageTypes, DAMAGE_TYPE_BLUDGEONING) ||
          weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes == DAMAGE_TYPE_BLUDGEONING))
      return TRUE;
    if (display && HAS_COMBAT_FEAT(ch, feat_to_cfeat(i), DAMAGE_TYPE_PIERCING) &&
         (IS_SET(weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes, DAMAGE_TYPE_PIERCING) ||
          IS_SET(weapon_list[k].damageTypes, DAMAGE_TYPE_PIERCING) ||
          weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes == DAMAGE_TYPE_PIERCING))
      return TRUE;

    for (k = 0; k < NUM_WEAPON_TYPES; k++) {
      if (HAS_COMBAT_FEAT(ch, feat_to_cfeat(i), k) && display &&
         (IS_SET(weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes, weapon_list[k].damageTypes) ||
          IS_SET(weapon_list[k].damageTypes, weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes) ||
          weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes == weapon_list[k].damageTypes))
      {
        return TRUE;
      }
    }
    for (k = 0; k < 6; k++) {
      if (obj->affected[k].location == APPLY_FEAT && obj->affected[k].specific == i &&
          GET_OBJ_TYPE(obj) == ITEM_WEAPON && GET_OBJ_VAL(obj, 0) == j)
      {
        return TRUE;
      }
      if (obj->affected[k].location == APPLY_FEAT && obj->affected[k].specific == i &&
          GET_OBJ_TYPE(obj) == ITEM_WEAPON && display &&
         (IS_SET(weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes, weapon_list[j].damageTypes) ||
          IS_SET(weapon_list[j].damageTypes, weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes) ||
          weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes == weapon_list[j].damageTypes))
      {
        return TRUE;
      }
    }
  }

  obj = GET_EQ(ch, WEAR_HOLD);

  if (obj) {
    if (display && HAS_COMBAT_FEAT(ch, feat_to_cfeat(i), DAMAGE_TYPE_SLASHING) &&
         (IS_SET(weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes, DAMAGE_TYPE_SLASHING) ||
          IS_SET(weapon_list[k].damageTypes, DAMAGE_TYPE_SLASHING) ||
          weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes == DAMAGE_TYPE_SLASHING))
      return TRUE;
    if (display && HAS_COMBAT_FEAT(ch, feat_to_cfeat(i), DAMAGE_TYPE_BLUDGEONING) &&
         (IS_SET(weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes, DAMAGE_TYPE_BLUDGEONING) ||
          IS_SET(weapon_list[k].damageTypes, DAMAGE_TYPE_BLUDGEONING) ||
          weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes == DAMAGE_TYPE_BLUDGEONING))
      return TRUE;
    if (display && HAS_COMBAT_FEAT(ch, feat_to_cfeat(i), DAMAGE_TYPE_PIERCING) &&
         (IS_SET(weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes, DAMAGE_TYPE_PIERCING) ||
          IS_SET(weapon_list[k].damageTypes, DAMAGE_TYPE_PIERCING) ||
          weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes == DAMAGE_TYPE_PIERCING))
      return TRUE;
    for (k = 0; k < NUM_WEAPON_TYPES; k++) {
      if (HAS_COMBAT_FEAT(ch, feat_to_cfeat(i), k) && display &&
         (IS_SET(weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes, weapon_list[k].damageTypes) ||
          IS_SET(weapon_list[k].damageTypes, weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes) ||
          weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes == weapon_list[k].damageTypes))
      {
        return TRUE;
      }
    }
    for (k = 0; k < 6; k++) {
      if (obj->affected[k].location == APPLY_FEAT && obj->affected[k].specific == i &&
          GET_OBJ_TYPE(obj) == ITEM_WEAPON && GET_OBJ_VAL(obj, 0) == j)
      {
        return TRUE;
      }
      if (obj->affected[k].location == APPLY_FEAT && obj->affected[k].specific == i &&
          GET_OBJ_TYPE(obj) == ITEM_WEAPON && display &&
         (IS_SET(weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes, weapon_list[j].damageTypes) ||
          IS_SET(weapon_list[j].damageTypes, weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes) ||
          weapon_list[GET_OBJ_VAL(obj, 0)].damageTypes == weapon_list[j].damageTypes))
      {
        return TRUE;
      }
    }
  }

	return FALSE;
}

void display_levelup_weapons(struct char_data *ch) {

	int i=0;

	extern char *weapon_damage_types[];

	send_to_char(ch, "Please select a weapon:\r\n\r\n");

	for (i = MIN_WEAPON_DAMAGE_TYPES; i <= MAX_WEAPON_DAMAGE_TYPES; i++) {
		send_to_char(ch, "%2d) %-25s   ", i, weapon_damage_types[i-MIN_WEAPON_DAMAGE_TYPES]);
		if (i % 2 == 0)
			send_to_char(ch, "\r\n");
	}

	if (i % 2 != 0)
		send_to_char(ch, "\r\n");
	send_to_char(ch, "\r\n");

	send_to_char(ch, "Please select a weapon by typing a number beside it: (-1 to cancel) ");
}

void set_feat(struct char_data *ch, int i, int j) {

	if (ch->desc && LEVELUP(ch) && STATE(ch->desc) >= CON_LEVELUP_START && STATE(ch->desc) <= CON_LEVELUP_END) {
		LEVELUP(ch)->feats[i] = j;
		return;
	}

	SET_FEAT(ch, i, j);
}

#define FEAT_TYPE_NORMAL                1
#define FEAT_TYPE_NORMAL_CLASS          2
#define FEAT_TYPE_EPIC                  3
#define FEAT_TYPE_EPIC_CLASS            4


int handle_levelup_feat_points(struct char_data *ch, int feat_num, int return_val) {

	if (has_feat(ch, feat_num) && !feat_list[feat_num].can_stack) {
          send_to_char(ch, "You already have this feat.\r\nPress enter to continue.\r\n");
          return FALSE;
        }
	

	int feat_points = LEVELUP(ch)->feat_points;
	int epic_feat_points = LEVELUP(ch)->epic_feat_points;
	int class_feat_points = LEVELUP(ch)->num_class_feats;
	int epic_class_feat_points = LEVELUP(ch)->num_epic_class_feats;
	int feat_type = 0;

	if (feat_list[feat_num].epic == TRUE) {
	    if (is_class_feat(feat_num, LEVELUP(ch)->class))
	      feat_type = FEAT_TYPE_EPIC_CLASS;
	    else
	      feat_type = FEAT_TYPE_EPIC;
	}
	else {
	    if (is_class_feat(feat_num, LEVELUP(ch)->class))
	      feat_type = FEAT_TYPE_NORMAL_CLASS;
	    else
	      feat_type = FEAT_TYPE_NORMAL;
	}

	if (return_val == 0) {
		// if it's an epic feat, make sure they have an epic feat point

		  if (feat_type == FEAT_TYPE_EPIC && epic_feat_points < 1) {
		    send_to_char(ch, "This is an epic feat and you do not have any epic feat points remaining.\r\n");
		    send_to_char(ch, "Please press enter to continue.\r\n");
		    return 0;
		  }

		  // if it's an epic class feat, make sure they have an epic feat point or an epic class feat point

		  if (feat_type == FEAT_TYPE_EPIC_CLASS && epic_feat_points < 1 && epic_class_feat_points < 1) {
		    send_to_char(ch, "This is an epic class feat and you do not have any epic feat points or epic class feat points remaining.\r\n");
		    send_to_char(ch, "Please press enter to continue.\r\n");
		    return 0;
		  }

		  // if it's a normal feat, make sure they have a normal feat point

		  if (feat_type == FEAT_TYPE_NORMAL && feat_points < 1) {
		    send_to_char(ch, "This is a normal feat and you do not have any normal feat points remaining.\r\n");
		    send_to_char(ch, "Please press enter to continue.\r\n");
		    return 0;
		  }

		  // if it's a normal class feat, make sure they have a normal feat point or a normal class feat point

		  if (feat_type == FEAT_TYPE_NORMAL_CLASS && feat_points < 1 && class_feat_points < 1 && epic_class_feat_points < 1) {
		    send_to_char(ch, "This is a normal class feat and you do not have any normal feat points or normal class feat points remaining.\r\n");
		    send_to_char(ch, "Please press enter to continue.\r\n");
		    return 0;
		  }
		  return 1;
	}
	else {
		// reduce the appropriate feat point type based on what the feat type was set as above.  This simulatyes spendinng the feat

		  if (feat_type == FEAT_TYPE_EPIC) {
		    epic_feat_points--;
		  }
		  else if (feat_type == FEAT_TYPE_EPIC_CLASS) {
		    if (epic_class_feat_points > 0)
		      epic_class_feat_points--;
		    else
		      epic_feat_points--;
		  }
		  else if (feat_type == FEAT_TYPE_NORMAL) {
		    feat_points--;
		  }
		  else if (feat_type == FEAT_TYPE_NORMAL_CLASS) {
		    if (class_feat_points > 0)
		      class_feat_points--;
		    else if (feat_points > 0)
		      feat_points--;
		    if (epic_class_feat_points > 0)
		      epic_class_feat_points--;
		    else
		      epic_feat_points--;
		  }

		  LEVELUP(ch)->feat_points = feat_points;
		  LEVELUP(ch)->epic_feat_points = epic_feat_points;
		  LEVELUP(ch)->num_class_feats = class_feat_points;
		  LEVELUP(ch)->num_epic_class_feats = epic_class_feat_points;

		  LEVELUP(ch)->feats[feat_num]++;

		  return 1;
	}

    return 1;
}

#endif
