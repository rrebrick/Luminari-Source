/**************************************************************************
 *  File: act.other.c                                       Part of tbaMUD *
 *  Usage: Miscellaneous player-level commands.                             *
 *                                                                         *
 *  All rights reserved.  See license for complete information.            *
 *                                                                         *
 *  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 **************************************************************************/

/* needed by sysdep.h to allow for definition of <sys/stat.h> */
#define __ACT_OTHER_C__

#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "screen.h"
#include "house.h"
#include "constants.h"
#include "dg_scripts.h"
#include "act.h"
#include "spec_procs.h"
#include "class.h"
#include "fight.h"
#include "mail.h"  /* for has_mail() */
#include "shop.h"
#include "quest.h"
#include "modify.h"
#include "race.h"
#include "clan.h"
#include "mud_event.h"
#include "craft.h"
#include "treasure.h"
#include "mudlim.h"
#include "spec_abilities.h"
#include "actions.h"
#include "feats.h"

/* Local defined utility functions */
/* do_group utility functions */
static void print_group(struct char_data *ch);
static void display_group_list(struct char_data * ch);

/* innate animate dead ability */
#define MOB_ZOMBIE            11   /* animate dead levels 1-7 */
#define MOB_GHOUL             35   // " " level 11+
#define MOB_GIANT_SKELETON    36   // " " level 21+
#define MOB_MUMMY             37   // " " level 30
ACMD(do_animatedead) {
  int uses_remaining = 0;
  struct char_data *mob = NULL;
  mob_vnum mob_num = 0;

  if (!HAS_FEAT(ch, FEAT_ANIMATE_DEAD)) {
    send_to_char(ch, "You do not know how to animate dead!\r\n");
    return;
  }

  if (IS_HOLY(IN_ROOM(ch))) {
    send_to_char(ch, "This place is too holy for such blasphemy!");
    return;
  }

  if (HAS_PET_UNDEAD(ch)) {
    send_to_char(ch, "You can't control more undead!\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_CHARM)) {
    send_to_char(ch, "You are too giddy to have any followers!\r\n");
    return;
  }

  if ((uses_remaining = daily_uses_remaining(ch, FEAT_ANIMATE_DEAD)) == 0) {
    send_to_char(ch, "You must recover the energy required to animate the dead.\r\n");
    return;
  }
  if (uses_remaining < 0) {
    send_to_char(ch, "You are not experienced enough.\r\n");
    return;
  }

  /* success! */

  if (CASTER_LEVEL(ch) >= 30)
    mob_num = MOB_MUMMY;
  else if (CASTER_LEVEL(ch) >= 20)
    mob_num = MOB_GIANT_SKELETON;
  else if (CASTER_LEVEL(ch) >= 10)
    mob_num = MOB_GHOUL;
  else
    mob_num = MOB_ZOMBIE;

  if (!(mob = read_mobile(mob_num, VIRTUAL))) {
    send_to_char(ch, "You don't quite remember how to make that creature.\r\n");
    return;
  }
  char_to_room(mob, IN_ROOM(ch));
  IS_CARRYING_W(mob) = 0;
  IS_CARRYING_N(mob) = 0;
  SET_BIT_AR(AFF_FLAGS(mob), AFF_CHARM);

  act("$n animates a corpse!", FALSE, ch, 0, mob, TO_ROOM);
  act("You animate a corpse!", FALSE, ch, 0, mob, TO_CHAR);
  load_mtrigger(mob);
  add_follower(mob, ch);
  if (GROUP(ch) && GROUP_LEADER(GROUP(ch)) == ch)
    join_group(mob, GROUP(ch));

  if (!IS_NPC(ch))
    start_daily_use_cooldown(ch, FEAT_ANIMATE_DEAD);

  USE_STANDARD_ACTION(ch);
}

ACMD(do_abundantstep) {
  int steps = 0, i = 0, j, repeat = 0, max = 0;
  room_rnum room_tracker = NOWHERE, nextroom = NOWHERE;
  char buf[MAX_INPUT_LENGTH] = { '\0' }, tc = '\0';
  const char *p = NULL;

  if (!HAS_FEAT(ch, FEAT_ABUNDANT_STEP)) {
    send_to_char(ch, "You do not know that martial art skill!\r\n");
    return;
  }

  if (FIGHTING(ch)) {
    send_to_char(ch, "You can't focus enough in combat to use this martial art skill!\r\n");
    return;
  }

  if (GET_MOVE(ch) < 30) {
    send_to_char(ch, "You are too tired to use this martial art skill!\r\n");
    return;
  }

  steps = 0;
  room_tracker = IN_ROOM(ch); /* start the room tracker in current location */
  p = argument;
  max = 5 + CLASS_LEVEL(ch, CLASS_MONK) / 2; /* maximum steps */

  while (p && *p && !isdigit(*p) && !isalpha(*p))
    p++; /* looking for first number or letter */

  if (!p || !*p) { /* empty argument */
    send_to_char(ch, "You must give directions from your current location.  Examples:\r\n"
            "  w w n n e\r\n"
            "  2w n n e\r\n");
    return;
  }

  /* step through our string */
  while (*p) {

    while (*p && !isdigit(*p) && !isalpha(*p))
      p++; /* skipping spaces, and anything not a letter or number */

    if (isdigit(*p)) { /* value a number?  if so it will be our repeat */
      repeat = atoi(p);

      while (isdigit(*p)) /* get rid of extra numbers */
        p++;

    } else /* value isn't a number, so we are moving just a single space */
      repeat = 1;

    /* indication we haven't found a direction to move yet */
    i = -1;

    if (isalpha(*p)) { /* ok found a letter, and repeat is set */

      for (i = 0; isalpha(*p); i++, p++)
        buf[i] = LOWER(*p); /* turn a string of letters into lower case  buf */

      j = i; /* how many letters we found */
      tc = buf[i]; /* the non-alpha that terminated us */
      buf[i] = 0; /* placing a '0' in that last spot in this mini buf */

      for (i = 1; complete_cmd_info[i].command_pointer == do_move && strcmp(complete_cmd_info[i].sort_as, buf); i++)
        ; /* looking for a move command that matches our buf */

      if (complete_cmd_info[i].command_pointer == do_move) {
        i = complete_cmd_info[i].subcmd;
      } else
        i = -1;
      /* so now i is either our direction to move (define) or -1 */

      buf[j] = tc; /* replace the terminating character in this mini buff */
      //send_to_char(ch, "i: %d\r\n", i);
    }

    if (i > -1) { /* we have a direction to move! */
      while (repeat > 0) {
        repeat--;

        if (++steps > max) /* reached our limit of steps! */
          break;

        if (!W_EXIT(room_tracker, i)) { /* is i a valid direction? */
          send_to_char(ch, "Invalid step. Skipping.\r\n");
          break;
        }

        nextroom = W_EXIT(room_tracker, i)->to_room;
        if (nextroom == NOWHERE)
          break;

        room_tracker = nextroom;
      }
    }
    if (steps > max)
      break;
  } /* finished stepping through the string */

  if (IN_ROOM(ch) != room_tracker) {
    send_to_char(ch, "Your will bends reality as you travel through the ethereal plane.\r\n");
    act("$n is suddenly absent.", TRUE, ch, 0, 0, TO_ROOM);

    char_from_room(ch);
    char_to_room(ch, room_tracker);

    act("$n is suddenly present.", TRUE, ch, 0, 0, TO_ROOM);

    look_at_room(ch, 0);
    GET_MOVE(ch) -= 30;
    USE_MOVE_ACTION(ch);
  } else {
    send_to_char(ch, "You failed!\r\n");
  }

  return;
}

/* apply poison to a weapon */
ACMD(do_applypoison) {
  char arg1[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];
  struct obj_data *poison, *weapon;
  int amount = 1;

  two_arguments(argument, arg1, arg2);

  if (!HAS_FEAT(ch, FEAT_APPLY_POISON)) {
    send_to_char(ch, "You do not know how!\r\n");
    return;
  }

  if (!*arg1) {
    send_to_char(ch, "Apply what poison?\r\n");
    return;
  }
  if (!*arg2) {
    send_to_char(ch, "Apply on which weapon?\r\n");
    return;
  }

  poison = get_obj_in_list_vis(ch, arg1, NULL, ch->carrying);
  if (!poison) {
    send_to_char(ch, "You do not carry that poison!\r\n");
    return;
  }

  weapon = get_obj_in_list_vis(ch, arg2, NULL, ch->carrying);
  if (!weapon) {
    send_to_char(ch, "You do not carry that weapon!\r\n");
    return;
  }

  if (GET_OBJ_TYPE(poison) != ITEM_POISON) {
    send_to_char(ch, "But that is not a poison!\r\n");
    return;
  }
  if (GET_OBJ_TYPE(weapon) != ITEM_WEAPON) {
    send_to_char(ch, "But that is not a weapon!\r\n");
    return;
  }
  if (GET_OBJ_VAL(poison, 2) <= 0) {
    send_to_char(ch, "That vial is empty!\r\n");
    return;
  }
  if (weapon->weapon_poison.poison) {
    send_to_char(ch, "That weapon is already poisoned!\r\n");
    return;
  }

  /* high chance of success, just random for now */
  if (rand_number(0, 5)) {
    weapon->weapon_poison.poison_hits = GET_OBJ_VAL(poison, 3);
    weapon->weapon_poison.poison = GET_OBJ_VAL(poison, 0);
    weapon->weapon_poison.poison_level = GET_OBJ_VAL(poison, 1);
    act("$n applies some \tGpoison\tn onto $p.", FALSE, ch, weapon, 0, TO_ROOM);
    act("You apply some \tGpoison\tn onto $p.", FALSE, ch, weapon, 0, TO_CHAR);
    USE_STANDARD_ACTION(ch);
    USE_MOVE_ACTION(ch);
  } else {
    act("$n fails to apply the \tGpoison\tn onto $p.", FALSE, ch, weapon, 0, TO_ROOM);
    act("You fail to \tGpoison\tn your $p.", FALSE, ch, weapon, 0, TO_CHAR);
  }

  GET_OBJ_VAL(poison, 2) -= amount;
}


#define BARD_AFFECTS 7
void perform_perform(struct char_data *ch) {
  struct affected_type af[BARD_AFFECTS];
  int level = 0, i = 0, duration = 0;
  struct char_data *tch = NULL;
  long cooldown;

  if (char_has_mud_event(ch, ePERFORM)) {
    send_to_char(ch, "You must wait longer before you can use this ability "
            "again.\r\n");
    return;
  }

  if (IS_NPC(ch))
    level = GET_LEVEL(ch);
  else
    level = CLASS_LEVEL(ch, CLASS_BARD) + GET_CHA_BONUS(ch);

  duration = 14 + GET_CHA_BONUS(ch);

  /* init affect array */
  for (i = 0; i < BARD_AFFECTS; i++) {
    new_affect(&(af[i]));
    af[i].spell = SKILL_PERFORM;
    af[i].duration = duration;
  }

  af[0].location = APPLY_HITROLL;
  af[0].modifier = MAX(1, level / 5);

  af[1].location = APPLY_DAMROLL;
  af[1].modifier = MAX(1, level / 5);

  af[2].location = APPLY_SAVING_WILL;
  af[2].modifier = MAX(1, level / 5);

  af[3].location = APPLY_SAVING_FORT;
  af[3].modifier = MAX(1, level / 5);

  af[4].location = APPLY_SAVING_REFL;
  af[4].modifier = MAX(1, level / 5);

  af[5].location = APPLY_AC_NEW;
  af[5].modifier = 2 + (level / 10);

  af[6].location = APPLY_HIT;
  af[6].modifier = 10 + level;

  USE_STANDARD_ACTION(ch);

  act("$n sings a rousing tune!", FALSE, ch, NULL, NULL, TO_ROOM);
  act("You sing a rousing tune!", FALSE, ch, NULL, NULL, TO_CHAR);

  cooldown = (2 * SECS_PER_MUD_DAY) - (level * 100);
  attach_mud_event(new_mud_event(ePERFORM, ch, NULL), cooldown);

  if (!GROUP(ch)) {
    if (affected_by_spell(ch, SKILL_PERFORM))
      return;
    SONG_AFF_VAL(ch) = MAX(1, level / 5);
    GET_HIT(ch) += 20 + level;
    for (i = 0; i < BARD_AFFECTS; i++)
      affect_join(ch, af + i, FALSE, FALSE, FALSE, FALSE);
    return;
  }

  while ((tch = (struct char_data *) simple_list(GROUP(ch)->members)) !=
          NULL) {
    if (IN_ROOM(tch) != IN_ROOM(ch))
      continue;
    if (affected_by_spell(tch, SKILL_PERFORM))
      continue;
    SONG_AFF_VAL(tch) = MAX(1, level / 5);
    GET_HIT(tch) += 20 + level;
    for (i = 0; i < BARD_AFFECTS; i++)
      affect_join(tch, af + i, FALSE, FALSE, FALSE, FALSE);
    act("A song from $n enhances you!", FALSE, ch, NULL, tch, TO_VICT);
  }

}

ACMD(do_perform) {

  if (!IS_NPC(ch) && !HAS_FEAT(ch, FEAT_BARDIC_MUSIC)) {
    send_to_char(ch, "You don't know how to perform.\r\n");
    return;
  }

  perform_perform(ch);
}
#undef BARD_AFFECTS


#define MOB_PALADIN_MOUNT 70

void perform_call(struct char_data *ch, int call_type, int level) {
  int i = 0;
  struct follow_type *k = NULL, *next = NULL;
  struct char_data *mob = NULL;
  mob_vnum mob_num = NOBODY;
  /* tests for whether you can actually call a companion */

  /* companion here already ? */
  for (k = ch->followers; k; k = next) {
    next = k->next;
    if (IS_NPC(k->follower) && AFF_FLAGGED(k->follower, AFF_CHARM) &&
            MOB_FLAGGED(k->follower, call_type)) {
      if (IN_ROOM(ch) == IN_ROOM(k->follower)) {
        send_to_char(ch, "Your companion has already been summoned!\r\n");
        return;
      } else {
        char_from_room(k->follower);

        if (ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(ch)), ZONE_WILDERNESS)) {
          X_LOC(k->follower) = world[IN_ROOM(ch)].coords[0];
          Y_LOC(k->follower) = world[IN_ROOM(ch)].coords[1];
        }


        char_to_room(k->follower, IN_ROOM(ch));
        act("$n calls $N!", FALSE, ch, 0, k->follower, TO_ROOM);
        act("You call forth $N!", FALSE, ch, 0, k->follower, TO_CHAR);
        return;
      }
    }
  }

  /* doing two disqualifying tests in this switch block */
  switch (call_type) {
    case MOB_C_ANIMAL:
      /* do they even have a valid selection yet? */
      if (!IS_NPC(ch) && GET_ANIMAL_COMPANION(ch) <= 0) {
        send_to_char(ch, "You have to select your companion via the 'study' "
                "command.\r\n");
        return;
      }

      /* is the ability on cooldown? */
      if (char_has_mud_event(ch, eC_ANIMAL)) {
        send_to_char(ch, "You must wait longer before you can use this ability again.\r\n");
        return;
      }

      /* todo:  seriously, fix this */
      if (!(mob_num = GET_ANIMAL_COMPANION(ch)))
        mob_num = 63; // meant for npc's

      break;
    case MOB_C_FAMILIAR:
      /* do they even have a valid selection yet? */
      if (!IS_NPC(ch) && GET_FAMILIAR(ch) <= 0) {
        send_to_char(ch, "You have to select your companion via the 'study' "
                "command.\r\n");
        return;
      }

      /* is the ability on cooldown? */
      if (char_has_mud_event(ch, eC_FAMILIAR)) {
        send_to_char(ch, "You must wait longer before you can use this ability again.\r\n");
        return;
      }

      mob_num = GET_FAMILIAR(ch);

      break;
    case MOB_C_MOUNT:
      /* for now just one selection for paladins */
      GET_MOUNT(ch) = MOB_PALADIN_MOUNT;

      /* do they even have a valid selection yet? */
      if (GET_MOUNT(ch) <= 0) {
        send_to_char(ch, "You have to select your companion via the 'study' "
                "command.\r\n");
        return;
      }

      /* is the ability on cooldown? */
      if (char_has_mud_event(ch, eC_MOUNT)) {
        send_to_char(ch, "You must wait longer before you can use this ability again.\r\n");
        return;
      }

      mob_num = GET_MOUNT(ch);

      break;
  }

  /* couple of dummy checks */
  if (mob_num <= 0 || mob_num > 99) //zone 0 for mobiles
    return;
  if (level >= LVL_IMMORT)
    level = LVL_IMMORT - 1;

  /* passed all the tests, bring on the companion! */
  /* HAVE to make sure the mobiles for the lists of
     companions / familiars / etc have the proper
     MOB_C_x flag set via medit */
  if (!(mob = read_mobile(mob_num, VIRTUAL))) {
    send_to_char(ch, "You don't quite remember how to call that creature.\r\n");
    return;
  }

  if (ZONE_FLAGGED(GET_ROOM_ZONE(IN_ROOM(ch)), ZONE_WILDERNESS)) {
    X_LOC(mob) = world[IN_ROOM(ch)].coords[0];
    Y_LOC(mob) = world[IN_ROOM(ch)].coords[1];
  }


  char_to_room(mob, IN_ROOM(ch));
  IS_CARRYING_W(mob) = 0;
  IS_CARRYING_N(mob) = 0;

  /* setting mob strength according to 'level' */
  GET_LEVEL(mob) = level;
  GET_REAL_MAX_HIT(mob) += 20;
  for (i = 0; i < level; i++)
    GET_REAL_MAX_HIT(mob) += dice(3, 12) + 1;
  GET_HIT(mob) = GET_REAL_MAX_HIT(mob);
  GET_REAL_HITROLL(mob) += level / 3;
  GET_REAL_DAMROLL(mob) += level / 3;
  GET_REAL_AC(mob) += (level * 4); /* 12 ac at level 30 */

  /* make sure paladin mount is appropriate size to ride */
  if (call_type == MOB_C_MOUNT) {
    GET_SIZE(mob) = GET_SIZE(ch) + 1;
    GET_MOVE(mob) = GET_REAL_MAX_MOVE(mob) = 500;
  }

  affect_total(mob);

  SET_BIT_AR(AFF_FLAGS(mob), AFF_CHARM);
  act("$n calls $N!", FALSE, ch, 0, mob, TO_ROOM);
  act("You call forth $N!", FALSE, ch, 0, mob, TO_CHAR);
  load_mtrigger(mob);
  add_follower(mob, ch);
  if (GROUP(ch) && GROUP_LEADER(GROUP(ch)) == ch)
    join_group(mob, GROUP(ch));

  /* finally attach cooldown, approximately 14 minutes right now */
  if (call_type == MOB_C_ANIMAL) {
    attach_mud_event(new_mud_event(eC_ANIMAL, ch, NULL), 4 * SECS_PER_MUD_DAY);
  }
  if (call_type == MOB_C_FAMILIAR) {
    attach_mud_event(new_mud_event(eC_FAMILIAR, ch, NULL), 4 * SECS_PER_MUD_DAY);
  }
  if (call_type == MOB_C_MOUNT) {
    attach_mud_event(new_mud_event(eC_MOUNT, ch, NULL), 4 * SECS_PER_MUD_DAY);
  }

}

ACMD(do_call) {
  int call_type = -1, level = 0;

  skip_spaces(&argument);

  /* call types
     MOB_C_ANIMAL -> animal companion
     MOB_C_FAMILIAR -> familiar
     MOB_C_MOUNT -> paladin mount
   */
  if (!argument) {
    send_to_char(ch, "Usage:  call <companion/familiar/mount>\r\n");
    return;
  } else if (is_abbrev(argument, "companion")) {
    level = CLASS_LEVEL(ch, CLASS_DRUID);
    if (CLASS_LEVEL(ch, CLASS_RANGER) >= 4)
      level += CLASS_LEVEL(ch, CLASS_RANGER) - 3;

    if (!HAS_FEAT(ch, FEAT_ANIMAL_COMPANION)) {
      send_to_char(ch, "You do not have an animal companion.\r\n");
      return;
    }

    if (level <= 0) {
      send_to_char(ch, "You are too inexperienced to use this ability!\r\n");
      return;
    }
    call_type = MOB_C_ANIMAL;
  } else if (is_abbrev(argument, "familiar")) {
    level = CLASS_LEVEL(ch, CLASS_SORCERER) + CLASS_LEVEL(ch, CLASS_WIZARD);

    if (!HAS_FEAT(ch, FEAT_SUMMON_FAMILIAR)) {
      send_to_char(ch, "You do not have a familiar.\r\n");
      return;
    }

    if (level <= 0) {
      send_to_char(ch, "You are too inexperienced to use this ability!\r\n");
      return;
    }

    call_type = MOB_C_FAMILIAR;
  } else if (is_abbrev(argument, "mount")) {
    level = CLASS_LEVEL(ch, CLASS_PALADIN) - 2;

    if (!HAS_FEAT(ch, FEAT_CALL_MOUNT)) {
      send_to_char(ch, "You do not have a mount that you can call.\r\n");
      return;
    }

    if (level <= 0) {
      send_to_char(ch, "You are too inexperienced to use this ability!\r\n");
      return;
    }

    call_type = MOB_C_MOUNT;
  } else {
    send_to_char(ch, "Usage:  call <companion/familiar/mount>\r\n");
    return;
  }

  perform_call(ch, call_type, level);
}
#undef MOB_PALADIN_MOUNT

ACMD(do_purify) {
  char arg[MAX_INPUT_LENGTH] = {'\0'};
  struct char_data *vict = NULL;
  int uses_remaining = 0;

  if (IS_NPC(ch) || !HAS_FEAT(ch, FEAT_REMOVE_DISEASE)) {
    send_to_char(ch, "You have no idea how.\r\n");
    return;
  }

  one_argument(argument, arg);

  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "Whom do you want to purify?\r\n");
    return;
  }

  if ((uses_remaining = daily_uses_remaining(ch, FEAT_REMOVE_DISEASE)) == 0) {
    send_to_char(ch, "You must recover the divine energy required to remove disease.\r\n");
    return;
  }

  if (!IS_AFFECTED(vict, AFF_DISEASE) &&
          !affected_by_spell(vict, SPELL_EYEBITE)) {
    send_to_char(ch, "Your target isn't diseased!\r\n");
    return;
  }

  send_to_char(ch, "Your hands flash \tWbright white\tn as you reach out...\r\n");
  act("You are \tWhealed\tn by $N!", FALSE, vict, 0, ch, TO_CHAR);
  act("$n \tWheals\tn $N!", FALSE, ch, 0, vict, TO_NOTVICT);
  if (affected_by_spell(vict, SPELL_EYEBITE))
    affect_from_char(vict, SPELL_EYEBITE);
  if (IS_AFFECTED(vict, AFF_DISEASE))
    REMOVE_BIT_AR(AFF_FLAGS(vict), AFF_DISEASE);


  if (!IS_NPC(ch))
    start_daily_use_cooldown(ch, FEAT_REMOVE_DISEASE);

  update_pos(vict);

}

/* this is a temporary command, a simple cheesy way
   to get rid of your followers in a bind */
ACMD(do_dismiss) {
  struct follow_type *k = NULL;
  char buf[MAX_STRING_LENGTH] = {'\0'};
  char arg[MAX_INPUT_LENGTH] = {'\0'};
  struct char_data *vict = NULL;
  int found = 0;
  struct mud_event_data *pMudEvent = NULL;

  one_argument(argument, arg);

  if (!*arg) {
    send_to_char(ch, "You dismiss your non-present followers.\r\n");
    snprintf(buf, sizeof (buf), "$n dismisses $s non present followers.");
    act(buf, FALSE, ch, 0, 0, TO_ROOM);

    for (k = ch->followers; k; k = k->next)
      if (IN_ROOM(ch) != IN_ROOM(k->follower))
        if (AFF_FLAGGED(k->follower, AFF_CHARM))
          extract_char(k->follower);

    return;
  }

  if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "Whom do you want to dismiss?\r\n");
    return;
  }

  /* is this follower the target? */
  if ((vict->master == ch)) {
    /* is this follower charmed? */
    if (AFF_FLAGGED(vict, AFF_CHARM)) {
      /* is this a special companion?
       * if so, modify event cooldown (if it exits) */
      if (MOB_FLAGGED(vict, MOB_C_ANIMAL)) {
        if ((pMudEvent = char_has_mud_event(ch, eC_ANIMAL)) &&
                event_time(pMudEvent->pEvent) > (59 * PASSES_PER_SEC)) {
          change_event_duration(ch, eC_ANIMAL, (59 * PASSES_PER_SEC));
        }
      }
      if (MOB_FLAGGED(vict, MOB_C_FAMILIAR)) {
        if ((pMudEvent = char_has_mud_event(ch, eC_FAMILIAR)) &&
                event_time(pMudEvent->pEvent) > (59 * PASSES_PER_SEC)) {
          change_event_duration(ch, eC_FAMILIAR, (59 * PASSES_PER_SEC));
        }
      }
      if (MOB_FLAGGED(vict, MOB_C_MOUNT)) {
        if ((pMudEvent = char_has_mud_event(ch, eC_MOUNT)) &&
                event_time(pMudEvent->pEvent) > (59 * PASSES_PER_SEC)) {
          change_event_duration(ch, eC_MOUNT, (59 * PASSES_PER_SEC));
        }
      }

      extract_char(vict);
      found = 1;
    }
  }

  if (!found) {
    send_to_char(ch, "Your target is not valid!\r\n");
    return;
  } else {
    act("With a wave of your hand, you dismiss $N.",
            FALSE, ch, 0, vict, TO_CHAR);
    act("$n waves at you, indicating your dismissal.",
            FALSE, ch, 0, vict, TO_VICT);
    act("With a wave, $n dismisses $N.",
            TRUE, ch, 0, vict, TO_NOTVICT);
  }

}

/* recharge allows the refilling of charges for wands and staves
   for a price */
ACMD(do_recharge) {
  char buf[MAX_INPUT_LENGTH] = {'\0'};
  struct obj_data *obj = NULL;
  int maxcharge = 0, mincharge = 0, chargeval = 0;

  if (!IS_NPC(ch))
    ;
  else {
    send_to_char(ch, "You don't know how to do that!\r\n");
    return;
  }

  argument = one_argument(argument, buf);

  if (!(obj = get_obj_in_list_vis(ch, buf, NULL, ch->carrying))) {
    send_to_char(ch, "You don't have that!\r\n");
    return;
  }

  if (GET_OBJ_TYPE(obj) != ITEM_STAFF &&
          GET_OBJ_TYPE(obj) != ITEM_WAND) {
    send_to_char(ch, "Are you daft!  You can't recharge that!\r\n");
    return;
  }

  if (((GET_OBJ_TYPE(obj) == ITEM_STAFF) && !HAS_FEAT(ch, FEAT_CRAFT_STAFF)) ||
          ((GET_OBJ_TYPE(obj) == ITEM_WAND) && !HAS_FEAT(ch, FEAT_CRAFT_WAND))) {
    send_to_char(ch, "You don't know how to recharge that.\r\n");
    return;
  }



  if (GET_GOLD(ch) < 5000) {
    send_to_char(ch, "You don't have enough gold on hand!\r\n");
    return;
  }

  maxcharge = GET_OBJ_VAL(obj, 1);
  mincharge = GET_OBJ_VAL(obj, 2);

  if (mincharge < maxcharge) {
    chargeval = maxcharge - mincharge;
    GET_OBJ_VAL(obj, 2) += chargeval;
    GET_GOLD(ch) -= 5000;
    send_to_char(ch, "The %s glows blue for a moment.\r\n", (GET_OBJ_TYPE(obj) == ITEM_STAFF ? "staff" : "wand"));
    sprintf(buf, "The item now has %d charges remaining.\r\n", maxcharge);
    send_to_char(ch, buf);
    act("$p glows with a subtle blue light as $n recharges it.",
            FALSE, ch, obj, 0, TO_ROOM);
  } else {
    send_to_char(ch, "The item does not need recharging.\r\n");
  }
  return;
}

ACMD(do_mount) {
  char arg[MAX_INPUT_LENGTH];
  struct char_data *vict;

  one_argument(argument, arg);

  if (!*arg) {
    send_to_char(ch, "Mount who?\r\n");
    return;
  } else if (!(vict = get_char_room_vis(ch, arg, NULL))) {
    send_to_char(ch, "There is no-one by that name here.\r\n");
    return;
  } else if (!IS_NPC(vict) && GET_LEVEL(ch) < LVL_IMMORT) {
    send_to_char(ch, "Ehh... no.\r\n");
    return;
  } else if (RIDING(ch) || RIDDEN_BY(ch)) {
    send_to_char(ch, "You are already mounted.\r\n");
    return;
  } else if (RIDING(vict) || RIDDEN_BY(vict)) {
    send_to_char(ch, "It is already mounted.\r\n");
    return;
  } else if (GET_LEVEL(ch) < LVL_IMMORT && IS_NPC(vict) && !MOB_FLAGGED(vict, MOB_MOUNTABLE)) {
    send_to_char(ch, "You can't mount that!\r\n");
    return;
  } else if (!GET_ABILITY(ch, ABILITY_RIDE)) {
    send_to_char(ch, "First you need to learn *how* to mount.\r\n");
    return;
  } else if (GET_SIZE(vict) < (GET_SIZE(ch) + 1)) {
    send_to_char(ch, "The mount is too small for you!\r\n");
    return;
  } else if (GET_SIZE(vict) > (GET_SIZE(ch) + 2)) {
    send_to_char(ch, "The mount is too large for you!\r\n");
    return;
  } else if ((compute_ability(ch, ABILITY_RIDE) + 1) <= rand_number(1, GET_LEVEL(vict))) {
    act("You try to mount $N, but slip and fall off.", FALSE, ch, 0, vict, TO_CHAR);
    act("$n tries to mount you, but slips and falls off.", FALSE, ch, 0, vict, TO_VICT);
    act("$n tries to mount $N, but slips and falls off.", TRUE, ch, 0, vict, TO_NOTVICT);
    damage(ch, ch, dice(1, 2), -1, -1, -1);
    return;
  }

  act("You mount $N.", FALSE, ch, 0, vict, TO_CHAR);
  act("$n mounts you.", FALSE, ch, 0, vict, TO_VICT);
  act("$n mounts $N.", TRUE, ch, 0, vict, TO_NOTVICT);
  mount_char(ch, vict);

  USE_MOVE_ACTION(ch);

  if (IS_NPC(vict) && !AFF_FLAGGED(vict, AFF_TAMED) &&
          compute_ability(ch, ABILITY_RIDE) <= rand_number(1, GET_LEVEL(vict))) {
    act("$N suddenly bucks upwards, throwing you violently to the ground!", FALSE, ch, 0, vict, TO_CHAR);
    act("$n is thrown to the ground as $N violently bucks!", TRUE, ch, 0, vict, TO_NOTVICT);
    act("You buck violently and throw $n to the ground.", FALSE, ch, 0, vict, TO_VICT);
    dismount_char(ch);
    damage(vict, ch, dice(1, 3), -1, -1, -1);
  }
}

ACMD(do_dismount) {
  if (!RIDING(ch)) {
    send_to_char(ch, "You aren't even riding anything.\r\n");
    return;
  } else if (SECT(ch->in_room) == SECT_WATER_NOSWIM && !has_boat(ch)) {
    send_to_char(ch, "Yah, right, and then drown...\r\n");
    return;
  }

  act("You dismount $N.", FALSE, ch, 0, RIDING(ch), TO_CHAR);
  act("$n dismounts from you.", FALSE, ch, 0, RIDING(ch), TO_VICT);
  act("$n dismounts $N.", TRUE, ch, 0, RIDING(ch), TO_NOTVICT);
  dismount_char(ch);
}

ACMD(do_buck) {
  if (!RIDDEN_BY(ch)) {
    send_to_char(ch, "You're not even being ridden!\r\n");
    return;
  } else if (AFF_FLAGGED(ch, AFF_TAMED)) {
    send_to_char(ch, "But you're tamed!\r\n");
    return;
  }

  act("You quickly buck, throwing $N to the ground.", FALSE, ch, 0, RIDDEN_BY(ch), TO_CHAR);
  act("$n quickly bucks, throwing you to the ground.", FALSE, ch, 0, RIDDEN_BY(ch), TO_VICT);
  act("$n quickly bucks, throwing $N to the ground.", FALSE, ch, 0, RIDDEN_BY(ch), TO_NOTVICT);
  GET_POS(RIDDEN_BY(ch)) = POS_SITTING;
  if (rand_number(0, 4)) {
    send_to_char(RIDDEN_BY(ch), "You hit the ground hard!\r\n");
    damage(RIDDEN_BY(ch), RIDDEN_BY(ch), dice(2, 4), -1, -1, -1);
  }
  dismount_char(ch);


  /*
   * you might want to call set_fighting() or some nonsense here if you
   * want the mount to attack the unseated rider or vice-versa.
   */
}

ACMD(do_tame) {
  char arg[MAX_INPUT_LENGTH];
  struct affected_type af;
  struct char_data *vict;

  one_argument(argument, arg);

  if (!*arg) {
    send_to_char(ch, "Tame who?\r\n");
    return;
  } else if (!(vict = get_char_room_vis(ch, arg, NULL))) {
    send_to_char(ch, "They're not here.\r\n");
    return;
  } else if (GET_LEVEL(ch) < LVL_IMMORT && IS_NPC(vict) && !MOB_FLAGGED(vict, MOB_MOUNTABLE)) {
    send_to_char(ch, "You can't do that to them.\r\n");
    return;
  } else if (!GET_ABILITY(ch, ABILITY_RIDE)) {
    send_to_char(ch, "You don't even know how to tame something.\r\n");
    return;
  } else if (!IS_NPC(vict) && GET_LEVEL(ch) < LVL_IMMORT) {
    send_to_char(ch, "You can't do that.\r\n");
    return;
  } else if (GET_SKILL(ch, ABILITY_RIDE) <= rand_number(1, GET_LEVEL(vict))) {
    send_to_char(ch, "You fail to tame it.\r\n");
    return;
  }

  new_affect(&af);
  af.duration = 50 + compute_ability(ch, ABILITY_HANDLE_ANIMAL) * 4;
  SET_BIT_AR(af.bitvector, AFF_TAMED);
  affect_to_char(vict, &af);

  act("You tame $N.", FALSE, ch, 0, vict, TO_CHAR);
  act("$n tames you.", FALSE, ch, 0, vict, TO_VICT);
  act("$n tames $N.", FALSE, ch, 0, vict, TO_NOTVICT);
}


/* does the ch have a valid alignment for proposed class? */
/* returns 1 for valid alignment */

/* returns 0 for problem with alignment */
int valid_align_by_class(int alignment, int class) {

  switch (class) {

      /* any lawful alignment */
    case CLASS_MONK:
      switch (alignment) {
        case LAWFUL_GOOD:
        case LAWFUL_NEUTRAL:
        case LAWFUL_EVIL:
          return 1;
        default:
          return 0;
      }

      /* any 'neutral' alignment */
    case CLASS_DRUID:
      switch (alignment) {
        case NEUTRAL_GOOD:
        case LAWFUL_NEUTRAL:
        case TRUE_NEUTRAL:
        case CHAOTIC_NEUTRAL:
        case NEUTRAL_EVIL:
          return 1;
        default:
          return 0;
      }

      /* any 'non-lawful' alignment */
    case CLASS_BERSERKER:
    case CLASS_BARD:
      switch (alignment) {
          /* we are checking for invalids */
        case LAWFUL_GOOD:
        case LAWFUL_NEUTRAL:
        case LAWFUL_EVIL:
          return 0;
        default:
          return 1;
      }

      /* only lawful good */
    case CLASS_PALADIN:
      if (alignment == LAWFUL_GOOD)
        return 1;
      else
        return 0;

      /* default, no alignment restrictions */
    case CLASS_WIZARD:
    case CLASS_CLERIC:
    case CLASS_RANGER:
    case CLASS_ROGUE:
    case CLASS_WARRIOR:
    case CLASS_SORCERER:
      return 1;
  }

  /* shouldn't get here if we got all classes listed above */
  return 1;
}


// if you meet the class pre-reqs, return 1, otherwise 0
// class = class attempting to level in

int meet_class_reqs(struct char_data *ch, int class) {
  int i;

  /* alignment restrictions */
  if (!valid_align_by_class(convert_alignment(GET_ALIGNMENT(ch)), class))
    return 0;

  // this is to make sure an epic race doesn't multiclass
  for (i = 0; i < NUM_CLASSES; i++)
    if (CLASS_LEVEL(ch, i)) /* found char current class */
      break;
  switch (GET_RACE(ch)) {
    case RACE_CRYSTAL_DWARF:
      if (class == i) /* char class selection and current class match? */
        return 1;
      else
        return 0;
    case RACE_TRELUX:
      if (class == i) /* char class selection and current class match? */
        return 1;
      else
        return 0;
    default:
      break;
  }

  /* stat, and other restrictions */
  switch (class) {
    case CLASS_WIZARD:
      if (GET_REAL_INT(ch) >= 9)
        return 1;
      break;
    case CLASS_CLERIC:
      if (GET_REAL_WIS(ch) >= 9)
        return 1;
      break;
    case CLASS_PALADIN:
      if (GET_REAL_CHA(ch) >= 9)
        return 1;
      break;
    case CLASS_DRUID:
      if (GET_REAL_WIS(ch) >= 9)
        return 1;
      break;
    case CLASS_ROGUE:
      if (GET_REAL_DEX(ch) >= 9)
        return 1;
      break;
    case CLASS_WARRIOR:
      if (GET_REAL_STR(ch) >= 9)
        return 1;
      break;
    case CLASS_BERSERKER:
      if (GET_REAL_CON(ch) >= 9)
        return 1;
      break;
    case CLASS_SORCERER:
      if (GET_REAL_CHA(ch) >= 9)
        return 1;
      break;
    case CLASS_BARD:
      if (GET_REAL_CHA(ch) >= 9)
        return 1;
      break;
    case CLASS_RANGER:
      if (GET_REAL_DEX(ch) >= 9 && GET_REAL_WIS(ch) >= 9)
        return 1;
      break;
    case CLASS_MONK:
      if (GET_REAL_DEX(ch) >= 9 && GET_REAL_WIS(ch) >= 9)
        return 1;
      break;
  }
  return 0;
}

/* simple function to list classes with a "valid" check */
void list_valid_classes(struct char_data *ch) {
  int i;

  for (i = 0; i < NUM_CLASSES; i++) {
    if (meet_class_reqs(ch, i)) {
      send_to_char(ch, "%s\r\n", pc_class_types[i]);
    }
  }
  send_to_char(ch, "\r\n");
}

/* reset character to level 1, but preserve xp */
ACMD(do_respec) {
  char arg[MAX_INPUT_LENGTH] = {'\0'};
  int class = -1;

  if (IS_NPC(ch) || !ch->desc)
    return;

  one_argument(argument, arg);

  if (!*arg) {
    send_to_char(ch, "You need to select a starting class to respec to,"
            " here are your options:\r\n");
    list_valid_classes(ch);
    return;
  } else {
    class = get_class_by_name(arg);
    if (class == -1) {
      send_to_char(ch, "Invalid class.\r\n");
      list_valid_classes(ch);
      return;
    }
    if (class >= NUM_CLASSES || !meet_class_reqs(ch, class)) {
      send_to_char(ch, "That is not a valid class!  These are valid choices:\r\n");
      list_valid_classes(ch);
      return;
    }
    if (GET_LEVEL(ch) < 2) {
      send_to_char(ch, "You need to be at least 2nd level to respec...\r\n");
      return;
    }
    if (GET_LEVEL(ch) >= LVL_IMMORT) {
      send_to_char(ch, "Sorry staff can't respec...\r\n");
      return;
    }

    int tempXP = GET_EXP(ch);
    GET_CLASS(ch) = class;
    /* Make sure that players can't make wildshaped forms permanent.*/
    SUBRACE(ch) = 0;
    IS_MORPHED(ch) = 0;
    if (affected_by_spell(ch, SKILL_WILDSHAPE)) {
      affect_from_char(ch, SKILL_WILDSHAPE);
      send_to_char(ch, "You return to your normal form..\r\n");
    }

    do_start(ch);
    GET_EXP(ch) = tempXP;
    send_to_char(ch, "\tMYou have respec'd!\tn\r\n");
    send_to_char(ch, "\tDType 'gain' to regain your level(s)...\tn\r\n");
  }
}


/* level advancement, with multi-class support */
#define MULTICAP	3

ACMD(do_gain) {
  char arg[MAX_INPUT_LENGTH] = {'\0'};
  int is_altered = FALSE, num_levels = 0;
  int class = -1, i, classCount = 0;

  if (IS_NPC(ch) || !ch->desc)
    return;

  one_argument(argument, arg);

  if (!(GET_LEVEL(ch) < LVL_IMMORT - CONFIG_NO_MORT_TO_IMMORT &&
          GET_EXP(ch) >= level_exp(ch, GET_LEVEL(ch) + 1))) {
    send_to_char(ch, "You are not experienced enough to gain a level.\r\n");
    return;
  }

  if (!*arg) {
    send_to_char(ch, "You may gain a level in one of the following classes:\r\n\r\n");
    list_valid_classes(ch);
    send_to_char(ch, "Type 'gain <classname>' to gain a level in the chosen class.\r\n");
    return;
  } else {
    class = get_class_by_name(arg);
    if (class == -1) {
      send_to_char(ch, "Invalid class.\r\n");
      list_valid_classes(ch);
      return;
    }

    if (class < 0 || class >= NUM_CLASSES || !meet_class_reqs(ch, class)) {
      send_to_char(ch, "That is not a valid class!  These are valid choices:\r\n");
      list_valid_classes(ch);
      return;
    }

    //multi class cap
    for (i = 0; i < MAX_CLASSES; i++) {
      if (CLASS_LEVEL(ch, i) && i != class)
        classCount++;
    }
    if (classCount >= MULTICAP) {
      send_to_char(ch, "Current cap on multi-classing is %d.\r\n", MULTICAP);
      send_to_char(ch, "Please select one of the classes you already have!\r\n");
      return;
    }
    if ((GET_PRACTICES(ch) != 0) ||
            (GET_TRAINS(ch) != 0) ||
            (GET_BOOSTS(ch) != 0)) {//    ||
      /*         ((CLASS_LEVEL(ch, CLASS_SORCERER) && !IS_SORC_LEARNED(ch)) ||
               (CLASS_LEVEL(ch, CLASS_WIZARD)   && !IS_WIZ_LEARNED(ch))  ||
               (CLASS_LEVEL(ch, CLASS_BARD)     && !IS_BARD_LEARNED(ch)) ||
               (CLASS_LEVEL(ch, CLASS_DRUID)    && !IS_DRUID_LEARNED(ch))||
               (CLASS_LEVEL(ch, CLASS_RANGER)   && !IS_RANG_LEARNED(ch)))) {
       */
      /* The last level has not been completely gained yet - The player must
       * use all trains, pracs, boosts and choose spells and other benefits
       * vis 'study' before they can gain a level. */
      //      if (GET_PRACTICES(ch) != 0)
      //        send_to_char(ch, "You must use all practices before gaining another level.  You have %d practice%s remaining.\r\n", GET_PRACTICES(ch), (GET_PRACTICES(ch) > 1 ? "s" : ""));
      if (GET_TRAINS(ch) > 0)
        send_to_char(ch, "You must use all trains before gaining another level.  You have %d train%s remaining.\r\n", GET_TRAINS(ch), (GET_TRAINS(ch) > 1 ? "s" : ""));
      if (GET_BOOSTS(ch) != 0)
        send_to_char(ch, "You must use all boosts before gaining another level.  You have %d boost%s remaining.\r\n", GET_BOOSTS(ch), (GET_BOOSTS(ch) > 1 ? "s" : ""));
      /*       if(CLASS_LEVEL(ch, CLASS_SORCERER) && !IS_SORC_LEARNED(ch))
              send_to_char(ch, "You must 'study sorcerer' before gaining another level.\r\n");
            if(CLASS_LEVEL(ch, CLASS_WIZARD) && !IS_WIZ_LEARNED(ch))
              send_to_char(ch, "You must 'study wizard' before gaining another level.\r\n");
            if(CLASS_LEVEL(ch, CLASS_BARD) && !IS_BARD_LEARNED(ch))
              send_to_char(ch, "You must 'study bard' before gaining another level.\r\n");
            if(CLASS_LEVEL(ch, CLASS_DRUID) && !IS_DRUID_LEARNED(ch))
              send_to_char(ch, "You must 'study druid' before gaining another level.\r\n");
            if(CLASS_LEVEL(ch, CLASS_RANGER) && !IS_RANG_LEARNED(ch))
              send_to_char(ch, "You must 'study ranger' before gaining another level.\r\n");
       */
      return;

    } else if (GET_LEVEL(ch) < LVL_IMMORT - CONFIG_NO_MORT_TO_IMMORT &&
            GET_EXP(ch) >= level_exp(ch, GET_LEVEL(ch) + 1)) {
      GET_LEVEL(ch) += 1;
      CLASS_LEVEL(ch, class)++;
      GET_CLASS(ch) = class;
      num_levels++;
      advance_level(ch, class);
      is_altered = TRUE;
    } else {
      send_to_char(ch, "You are unable to gain a level.\r\n");
      return;
    }

    if (is_altered) {
      mudlog(BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE,
              "%s advanced %d level%s to level %d.", GET_NAME(ch),
              num_levels, num_levels == 1 ? "" : "s", GET_LEVEL(ch));
      if (num_levels == 1)
        send_to_char(ch, "You rise a level!\r\n");
      else
        send_to_char(ch, "You rise %d levels!\r\n", num_levels);
      set_title(ch, NULL);
      if (GET_LEVEL(ch) >= LVL_IMMORT && !PLR_FLAGGED(ch, PLR_NOWIZLIST))
        run_autowiz();

      send_to_char(ch, "\tMDon't forget to \tmTRAIN\tM, \tmSTUDY\tM and "
              "\tmBOOST\tM your abilities, feats and stats!\tn\r\n");
    }
  }
}
#undef MULTICAP

/*************************/
/* shapechange functions */
/*************************/

/* header file:  act.h */
void list_forms(struct char_data *ch) {
  send_to_char(ch, "%s\r\n", npc_race_menu);
}

/*    FIRST version of shapechange/wildshape
 *  shapechange function
 * mode = 1 = druid
 * mode = 2 = polymorph spell
 * header file:  act.h */
void perform_shapechange(struct char_data *ch, char *arg, int mode) {
  int form = -1;

  if (!*arg) {
    if (!IS_MORPHED(ch)) {
      send_to_char(ch, "You are already in your natural form!\r\n");
    } else {
      send_to_char(ch, "You shift back into your natural form...\r\n");
      act("$n shifts back to his natural form.", TRUE, ch, 0, 0, TO_ROOM);
      IS_MORPHED(ch) = 0;
    }
    if (CLASS_LEVEL(ch, CLASS_DRUID) >= 6)
      list_forms(ch);
  } else {
    form = atoi(arg);
    if (form < 1 || form > NUM_NPC_RACES - 1) {
      send_to_char(ch, "That is not a valid race!\r\n");
      list_forms(ch);
      return;
    }
    IS_MORPHED(ch) = form;
    if (mode == 1)
      GET_SHAPECHANGES(ch)--;

    /* the morph_to_x are in race.c */
    send_to_char(ch, "You transform into a %s!\r\n", RACE_ABBR(ch));
    act(morph_to_char[IS_MORPHED(ch)], TRUE, ch, 0, 0, TO_CHAR);
    send_to_char(ch, "\tDType 'innates' to see your abilities.\tn\r\n");
    act("$n shapechanges!", TRUE, ch, 0, 0, TO_ROOM);
    act(morph_to_room[IS_MORPHED(ch)], TRUE, ch, 0, 0, TO_ROOM);
  }

}

/* engine for shapechanging / wildshape
   turned this into a sub-function in case we want
   to use the engine for spells (like 'animal shapes')
 */
#define SHAPE_AFFECTS   3

void perform_wildshape(struct char_data *ch, int form_num, int spellnum) {
  struct affected_type af[SHAPE_AFFECTS];
  int i = 0;

  /* some dummy checks */
  if (!ch)
    return;
  if (spellnum <= 0 || spellnum >= NUM_SKILLS)
    return;
  if (form_num <= 0 || form_num >= NUM_SHAPE_TYPES)
    return;
  if (affected_by_spell(ch, spellnum))
    affect_from_char(ch, spellnum);

  /* should be ok to apply */
  for (i = 0; i < SHAPE_AFFECTS; i++) {
    new_affect(&(af[i]));
    af[i].spell = spellnum;
    if (spellnum == SKILL_WILDSHAPE)
      af[i].duration = 50 + CLASS_LEVEL(ch, CLASS_DRUID) * GET_WIS_BONUS(ch);
    else
      af[i].duration = 100;
  }

  /* determine stat bonuses, etc */
  SUBRACE(ch) = form_num;
  switch (SUBRACE(ch)) {
    case PC_SUBRACE_BADGER:
      af[0].location = APPLY_DEX;
      af[0].modifier = 2;
      break;
    case PC_SUBRACE_PANTHER:
      af[0].location = APPLY_DEX;
      af[0].modifier = 8;
      break;
    case PC_SUBRACE_BEAR:
      af[0].location = APPLY_STR;
      af[0].modifier = 8;
      af[1].location = APPLY_CON;
      af[1].modifier = 6;
      af[2].location = APPLY_HIT;
      af[2].modifier = 60;
      break;
    case PC_SUBRACE_G_CROCODILE:
      SET_BIT_AR(af[0].bitvector, AFF_SCUBA);
      SET_BIT_AR(af[1].bitvector, AFF_WATERWALK);
      af[2].location = APPLY_STR;
      af[2].modifier = 6;
      break;
  }

  for (i = 0; i < SHAPE_AFFECTS; i++)
    affect_join(ch, af + i, FALSE, FALSE, FALSE, FALSE);

  IS_MORPHED(ch) = NPCRACE_ANIMAL;

  act(shape_to_char[SUBRACE(ch)], TRUE, ch, 0, 0, TO_CHAR);
  act(shape_to_room[SUBRACE(ch)], TRUE, ch, 0, 0, TO_ROOM);

  if (!IS_NPC(ch) && (spellnum == SKILL_WILDSHAPE))
    start_daily_use_cooldown(ch, FEAT_WILD_SHAPE);

  USE_STANDARD_ACTION(ch);
}

/* a trivial shapechange code for druids */
ACMD(do_shapechange) {
  int form_num = -1, i = 0, uses_remaining = 0;

  if (!ch->desc || IS_NPC(ch))
    return;

  skip_spaces(&argument);

  if (!HAS_FEAT(ch, FEAT_WILD_SHAPE)) {
    send_to_char(ch, "You do not have a wild shape.\r\n");
    return;
  }

  if (((uses_remaining = daily_uses_remaining(ch, FEAT_WILD_SHAPE)) == 0) && *argument) {
    send_to_char(ch, "You must recover the energy required to take a wild shape.\r\n");
    return;
  }

  if (!*argument) {
    if (CLASS_LEVEL(ch, CLASS_DRUID) < 10)
      form_num = 1;
    if (CLASS_LEVEL(ch, CLASS_DRUID) < 14)
      form_num = 2;
    if (CLASS_LEVEL(ch, CLASS_DRUID) < 14)
      form_num = 3;
    if (CLASS_LEVEL(ch, CLASS_DRUID) >= 14)
      form_num = 4;
    send_to_char(ch, "Available Forms:\r\n\r\n");
    for (i = 1; i <= form_num; i++) {
      send_to_char(ch, shape_types[i]);
      send_to_char(ch, "\r\n");
    }
    send_to_char(ch, "\r\nYou can return to your normal form by typing:  "
            "shapechange normal\r\n");
    return;
  }

  /* should be OK at this point */
  if (is_abbrev(argument, shape_types[1])) {
    /* badger */
    form_num = PC_SUBRACE_BADGER;

  } else if (is_abbrev(argument, shape_types[2])) {
    /* panther */
    form_num = PC_SUBRACE_PANTHER;

  } else if (is_abbrev(argument, shape_types[3])) {
    /* bear */
    form_num = PC_SUBRACE_BEAR;

  } else if (is_abbrev(argument, shape_types[4])) {
    /* giant crocodile */
    form_num = PC_SUBRACE_G_CROCODILE;

  } else if (is_abbrev(argument, "normal")) {
    /* return to normal form */
    SUBRACE(ch) = 0;
    IS_MORPHED(ch) = 0;
    if (affected_by_spell(ch, SKILL_WILDSHAPE))
      affect_from_char(ch, SKILL_WILDSHAPE);
    send_to_char(ch, "You return to your normal form..\r\n");
    return;

  } else {
    /* invalid */
    send_to_char(ch, "This is not a valid form to shapechange into!\r\n");
    return;

  }

  perform_wildshape(ch, form_num, SKILL_WILDSHAPE);
}
#undef SHAPE_AFFECTS

/*****************************/
/* end shapechange functions */

/*****************************/


ACMD(do_quit) {
  if (IS_NPC(ch) || !ch->desc)
    return;

  if (subcmd != SCMD_QUIT && GET_LEVEL(ch) < LVL_IMMORT)
    send_to_char(ch, "You have to type quit--no less, to quit!\r\n");
  else if (FIGHTING(ch))
    send_to_char(ch, "No way!  You're fighting for your life!\r\n");
  else if (GET_POS(ch) < POS_STUNNED) {
    send_to_char(ch, "You die before your time...\r\n");
    die(ch, NULL);
  } else {
    act("$n has left the game.", TRUE, ch, 0, 0, TO_ROOM);
    mudlog(NRM, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), TRUE, "%s has quit the game.", GET_NAME(ch));

    if (GET_QUEST_TIME(ch) != -1)
      quest_timeout(ch);

    send_to_char(ch, "Goodbye, friend.. Come back soon!\r\n");

    /* We used to check here for duping attempts, but we may as well do it right
     * in extract_char(), since there is no check if a player rents out and it
     * can leave them in an equally screwy situation. */

    if (CONFIG_FREE_RENT)
      Crash_rentsave(ch, 0);

    GET_LOADROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));

    /* Stop snooping so you can't see passwords during deletion or change. */
    if (ch->desc->snoop_by) {
      write_to_output(ch->desc->snoop_by, "Your victim is no longer among us.\r\n");
      ch->desc->snoop_by->snooping = NULL;
      ch->desc->snoop_by = NULL;
    }

    extract_char(ch); /* Char is saved before extracting. */
  }
}

ACMD(do_save) {
  if (IS_NPC(ch) || !ch->desc)
    return;

  send_to_char(ch, "Saving %s.\r\n", GET_NAME(ch));
  save_char(ch, 0);
  Crash_crashsave(ch);
  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_HOUSE_CRASH))
    House_crashsave(GET_ROOM_VNUM(IN_ROOM(ch)));
  GET_LOADROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));
}

/* Generic function for commands which are normally overridden by special
 * procedures - i.e., shop commands, mail commands, etc. */
ACMD(do_not_here) {
  send_to_char(ch, "Sorry, but you cannot do that here!\r\n");
}

/* ability lore, functions like identify */
ACMD(do_lore) {
  char arg[MAX_INPUT_LENGTH] = {'\0'};
  struct char_data *tch = NULL;
  struct obj_data *tobj = NULL;
  int target = 0;

  if (IS_NPC(ch))
    return;

  if (!IS_NPC(ch) && !GET_ABILITY(ch, ABILITY_LORE)) {
    send_to_char(ch, "You have no ability to do that!\r\n");
    return;
  }

  one_argument(argument, arg);

  target = generic_find(arg, FIND_CHAR_ROOM | FIND_OBJ_INV | FIND_OBJ_ROOM |
          FIND_OBJ_EQUIP, ch, &tch, &tobj);

  if (*arg) {
    if (!target) {
      act("There is nothing to here to use your Lore ability on...", FALSE,
              ch, NULL, NULL, TO_CHAR);
      return;
    }
  } else {
    tch = ch;
  }

  send_to_char(ch, "You attempt to utilize your vast knowledge of lore...\r\n");
  USE_STANDARD_ACTION(ch);

  if (tobj && GET_OBJ_COST(tobj) > lore_app[(compute_ability(ch, ABILITY_LORE)
          + CLASS_LEVEL(ch, CLASS_BARD))]) {
    send_to_char(ch, "Your knowledge is not extensive enough to know about this object!\r\n");
    return;
  }
  if (tch && GET_LEVEL(tch) > compute_ability(ch, ABILITY_LORE)) {
    send_to_char(ch, "Your knowledge is not extensive enough to know about this creature!\r\n");
    return;
  }

  //level ch tch tobj
  // this is weak, we need to make this a real skill instead of calling spell_identify
  // call_magic(ch, tch, tobj, SPELL_IDENTIFY, GET_LEVEL(ch), CAST_SPELL);

  int i, found;
  size_t len;

  if (tobj) {
    char bitbuf[MAX_STRING_LENGTH];

    sprinttype(GET_OBJ_TYPE(tobj), item_types, bitbuf, sizeof (bitbuf));
    send_to_char(ch, "You feel informed:\r\nObject '%s', Item type: %s\r\n", tobj->short_description, bitbuf);

    sprintbitarray(GET_OBJ_WEAR(tobj), wear_bits, TW_ARRAY_MAX, bitbuf);
    send_to_char(ch, "Can be worn on: %s\r\n", bitbuf);

    if (GET_OBJ_AFFECT(tobj)) {
      sprintbitarray(GET_OBJ_AFFECT(tobj), affected_bits, AF_ARRAY_MAX, bitbuf);
      send_to_char(ch, "Item will give you following abilities:  %s\r\n", bitbuf);
    }

    sprintbitarray(GET_OBJ_EXTRA(tobj), extra_bits, EF_ARRAY_MAX, bitbuf);
    send_to_char(ch, "Item is: %s\r\n", bitbuf);

    send_to_char(ch, "Size: %s, Material: %s.\r\n",
            size_names[GET_OBJ_SIZE(tobj)],
            material_name[GET_OBJ_MATERIAL(tobj)]);

    send_to_char(ch, "Weight: %d, Value: %d, Rent: %d, Min. level: %d\r\n",
            GET_OBJ_WEIGHT(tobj), GET_OBJ_COST(tobj), GET_OBJ_RENT(tobj), GET_OBJ_LEVEL(tobj));

    switch (GET_OBJ_TYPE(tobj)) {
      case ITEM_SCROLL:
      case ITEM_POTION:
        len = i = 0;
        int hasVal = 0;

        if (GET_OBJ_VAL(tobj, 1) >= 1) {
          i = snprintf(bitbuf + len, sizeof (bitbuf) - len, " %s", skill_name(GET_OBJ_VAL(tobj, 1)));
          if (i >= 0)
            len += i;
          hasVal++;
        }

        if (GET_OBJ_VAL(tobj, 2) >= 1 && len < sizeof (bitbuf)) {
          i = snprintf(bitbuf + len, sizeof (bitbuf) - len, " %s", skill_name(GET_OBJ_VAL(tobj, 2)));
          if (i >= 0)
            len += i;
          hasVal++;
        }

        if (GET_OBJ_VAL(tobj, 3) >= 1 && len < sizeof (bitbuf)) {
          i = snprintf(bitbuf + len, sizeof (bitbuf) - len, " %s", skill_name(GET_OBJ_VAL(tobj, 3)));
          if (i >= 0)
            len += i;
          hasVal++;
        }

        if (hasVal)
          send_to_char(ch, "This %s casts: %s\r\n", item_types[(int) GET_OBJ_TYPE(tobj)],
                bitbuf);
        else
          send_to_char(ch, "This item has no spells imbued in it.\t\n");
        break;
      case ITEM_WAND:
      case ITEM_STAFF:
        send_to_char(ch, "This %s casts: %s\r\nIt has %d maximum charge%s and %d remaining.\r\n",
                item_types[(int) GET_OBJ_TYPE(tobj)], skill_name(GET_OBJ_VAL(tobj, 3)),
                GET_OBJ_VAL(tobj, 1), GET_OBJ_VAL(tobj, 1) == 1 ? "" : "s", GET_OBJ_VAL(tobj, 2));
        break;
      case ITEM_WEAPON:
        send_to_char(ch, "Damage Dice is '%dD%d' for an average per-round damage of %.1f.\r\n",
                GET_OBJ_VAL(tobj, 1), GET_OBJ_VAL(tobj, 2), ((GET_OBJ_VAL(tobj, 2) + 1) / 2.0) * GET_OBJ_VAL(tobj, 1));
        send_to_char(ch, "Weapon Type: %s (%d) Enhancement Bonus: %d\r\n",
          weapon_list[GET_WEAPON_TYPE(tobj)].name, GET_WEAPON_TYPE(tobj), GET_ENHANCEMENT_BONUS(tobj));
        send_to_char(ch, "Proficiency: %s\r\n", item_profs[GET_OBJ_PROF(tobj)]);
        break;
      case ITEM_SPELLBOOK:
        send_to_char(ch, "Number of pages:  %d", GET_OBJ_VAL(tobj, 1));
        break;
      case ITEM_ARMOR:
        send_to_char(ch, "AC-apply is %d\r\n", GET_OBJ_VAL(tobj, 0));
        send_to_char(ch, "Proficiency: %s\r\n", item_profs[GET_OBJ_PROF(tobj)]);
        break;
    }
    found = FALSE;
    for (i = 0; i < MAX_OBJ_AFFECT; i++) {
      if ((tobj->affected[i].location != APPLY_NONE) &&
              (tobj->affected[i].modifier != 0)) {
        if (!found) {
          send_to_char(ch, "Can affect you as :\r\n");
          found = TRUE;
        }
        sprinttype(tobj->affected[i].location, apply_types, bitbuf, sizeof (bitbuf));
        send_to_char(ch, "   Affects: %s By %d (%s)\r\n", bitbuf, tobj->affected[i].modifier, bonus_types[tobj->affected[i].bonus_type]);
      }
    }
  } else if (tch) { /* victim */
    send_to_char(ch, "Name: %s\r\n", GET_NAME(tch));
    if (!IS_NPC(tch))
      send_to_char(ch, "%s is %d years, %d months, %d days and %d hours old.\r\n",
            GET_NAME(tch), age(tch)->year, age(tch)->month,
            age(tch)->day, age(tch)->hours);
    send_to_char(ch, "Alignment: %s.\r\n", get_align_by_num(GET_ALIGNMENT(tch)));
    send_to_char(ch, "Height %d cm, Weight %d pounds\r\n", GET_HEIGHT(tch), GET_WEIGHT(tch));
    send_to_char(ch, "Level: %d, Hits: %d, Mana: %d\r\n", GET_LEVEL(tch), GET_HIT(tch), GET_MANA(tch));
    send_to_char(ch, "AC: %d, Hitroll: %d, Damroll: %d\r\n", compute_armor_class(NULL, tch, FALSE), GET_HITROLL(tch), GET_DAMROLL(tch));
    send_to_char(ch, "Str: %d/%d, Int: %d, Wis: %d, Dex: %d, Con: %d, Cha: %d\r\n",
            GET_STR(tch), GET_ADD(tch), GET_INT(tch),
            GET_WIS(tch), GET_DEX(tch), GET_CON(tch), GET_CHA(tch));
  }
}

/* a generic command to get rid of a fly flag */
ACMD(do_land) {
  bool msg = FALSE;

  if (affected_by_spell(ch, SPELL_FLY)) {
    affect_from_char(ch, SPELL_FLY);
    msg = TRUE;
  }

  if AFF_FLAGGED(ch, AFF_FLYING) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_FLYING);
    msg = TRUE;
  }

  if (msg) {
    send_to_char(ch, "You land on the ground.\r\n");
    act("$n lands on the ground.", TRUE, ch, 0, 0, TO_ROOM);
  } else {
    send_to_char(ch, "You are not flying.\r\n");
  }
}

/* race trelux innate ability */
ACMD(do_fly) {
  if (GET_RACE(ch) != RACE_TRELUX) {
    send_to_char(ch, "You don't have this ability.\r\n");
    return;
  }

  if AFF_FLAGGED(ch, AFF_FLYING) {
    send_to_char(ch, "You are already flying!\r\n");
    return;
  } else {
    SET_BIT_AR(AFF_FLAGS(ch), AFF_FLYING);
    act("$n begins to fly above the ground!", TRUE, ch, 0, 0, TO_ROOM);
    send_to_char(ch, "You take off and begin to fly!\r\n");
  }
  // old version just called the spell, but not as nice methinks
  //call_magic(ch, ch, NULL, SPELL_FLY, GET_LEVEL(ch), CAST_SPELL);
}

/* Helper function for 'search' command.
 * Returns the DC of the search attempt to find the specified door. */
int get_hidden_door_dc(struct char_data *ch, int door) {

  /* (Taken from the d&d 3.5e SRD)
   * Task	                                                Search DC
   * -----------------------------------------------------------------------
   * Ransack a chest full of junk to find a certain item	   10
   * Notice a typical secret door or a simple trap	   20
   * Find a difficult nonmagical trap (rogue only)1	21 or higher
   * Find a magic trap (rogue only)(1)             	25 + lvl of spell
   *                                                     used to create trap
   * Find a footprint	                                 Varies(2)
   * Notice a well-hidden secret door                        30
   * -----------------------------------------------------------------------
   * (1) Dwarves (even if they are not rogues) can use Search to find traps built
   *     into or out of stone.
   * (2) A successful Search check can find a footprint or similar sign of a
   *     creature's passage, but it won't let you find or follow a trail. See the
   *     Track feat for the appropriate DC. */

  /* zusuk bumped up these values slightly from the commented, srd-values because
   of the naturally much higher stats in our MUD world */
  if (EXIT_FLAGGED(EXIT(ch, door), EX_HIDDEN_EASY))
    return 15;
    //return 10;
  if (EXIT_FLAGGED(EXIT(ch, door), EX_HIDDEN_MEDIUM))
    return 30;
    //return 20;
  if (EXIT_FLAGGED(EXIT(ch, door), EX_HIDDEN_HARD))
    return 45;
    //return 30;

  /* If we get here, the door is not hidden. */
  return 0;
}

/* 'search' command, uses the rogue's search skill, if available, although
 * the command is available to all.  */
ACMD(do_search) {
  int door, found = FALSE;
  //  int val;
  //  struct char_data *i; // for player/mob
  //  struct char_data *list = world[ch->in_room].people; // for player/mob
  //  struct obj_data *objlist = world[ch->in_room].contents;
  //  struct obj_data *obj = NULL;
  //  struct obj_data *cont = NULL;
  //  struct obj_data *next_obj = NULL;
  int search_dc = 0;

  if (FIGHTING(ch)) {
    send_to_char(ch, "You can't do that in combat!\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_GRAPPLED)) {
    send_to_char(ch, "You are unable to move to make your attempt!\r\n");
    return;
  }

  if (!LIGHT_OK(ch)) {
    send_to_char(ch, "You can't see a thing!\r\n");
    return;
  }


  skip_spaces(&argument);

  if (!*argument) {
    /*
        for (obj = objlist; obj; obj = obj->next_content) {
          if (OBJ_FLAGGED(obj, ITEM_HIDDEN)) {
            SET_BIT(GET_OBJ_SAVED(obj), SAVE_OBJ_EXTRA);
            REMOVE_BIT(obj->obj_flags.extra_flags, ITEM_HIDDEN);
            act("You find $P.", FALSE, ch, 0, obj, TO_CHAR);
            act("$n finds $P.", FALSE, ch, 0, obj, TO_NOTVICT);
            found = TRUE;
            break;
          }
        }*/
    /* find a player/mob */
    /*    if(!found) {
          for (i = list; i; i = i->next_in_room) {
            if ((ch != i) && AFF_FLAGGED(i, AFF_HIDE) && (val < ochance)) {
              affect_from_char(i, SPELL_VACANCY);
              affect_from_char(i, SPELL_MIRAGE_ARCANA);
              affect_from_char(i, SPELL_STONE_BLEND);
              REMOVE_BIT(AFF_FLAGS(i), AFF_HIDE);
              act("You find $N lurking here!", FALSE, ch, 0, i, TO_CHAR);
              act("$n finds $N lurking here!", FALSE, ch, 0, i, TO_NOTVICT);
              act("You have been spotted by $n!", FALSE, ch, 0, i, TO_VICT);
              found = TRUE;
              break;
            }
          }
        }
     */
    if (!found) {
      /* find a hidden door */
      for (door = 0; door < NUM_OF_DIRS && found == FALSE; door++) {
        if (EXIT(ch, door) && EXIT_FLAGGED(EXIT(ch, door), EX_HIDDEN)) {
          /* Get the DC */
          search_dc = get_hidden_door_dc(ch, door);
          /* Roll the dice... */
          if (skill_check(ch, ABILITY_PERCEPTION, search_dc)) {
            act("You find a secret entrance!", FALSE, ch, 0, 0, TO_CHAR);
            act("$n finds a secret entrance!", FALSE, ch, 0, 0, TO_ROOM);
            REMOVE_BIT(EXIT(ch, door)->exit_info, EX_HIDDEN);
            found = TRUE;
          }
        }
      }
    }
  } /*else {
    generic_find(argument, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &i, &cont);
    if(cont) {
      for (obj = cont->contains; obj; obj = next_obj) {
        next_obj = obj->next_content;
        if (IS_OBJ_STAT(obj, ITEM_HIDDEN) && (val < ochance)) {
          SET_BIT(GET_OBJ_SAVED(obj), SAVE_OBJ_EXTRA);
          REMOVE_BIT(obj->obj_flags.extra_flags, ITEM_HIDDEN);
          act("You find $P.", FALSE, ch, 0, obj, TO_CHAR);
          act("$n finds $P.", FALSE, ch, 0, obj, TO_NOTVICT);
          found = TRUE;
          break;
        }
      }
    } else {
      send_to_char("Search what?!?!?!?\r\n", ch);
      found = TRUE;
    }
  } */

  if (!found) {
    send_to_char(ch, "You don't find anything you didn't see before.\r\n");
  }

  USE_FULL_ROUND_ACTION(ch);
}

/* entry point for sneak, the command just flips the flag */
ACMD(do_sneak) {

  if (FIGHTING(ch)) {
    send_to_char(ch, "You can't do that in combat!\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_GRAPPLED)) {
    send_to_char(ch, "You are unable to move to make your attempt!\r\n");
    return;
  }

  if (IS_NPC(ch) || !GET_ABILITY(ch, ABILITY_STEALTH)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_SNEAK)) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_SNEAK);
    send_to_char(ch, "You stop sneaking...\r\n");
    return;
  }

  send_to_char(ch, "Okay, you'll try to move silently for a while.\r\n");
  SET_BIT_AR(AFF_FLAGS(ch), AFF_SNEAK);
  USE_SWIFT_ACTION(ch); /*not really necessary honestly*/
}

/* entry point for hide, the command just flips the flag */
ACMD(do_hide) {

  if (FIGHTING(ch)) {
    if (HAS_FEAT(ch, FEAT_HIDE_IN_PLAIN_SIGHT)) {
      USE_STANDARD_ACTION(ch);
      if ((skill_roll(FIGHTING(ch), ABILITY_PERCEPTION)) < (skill_roll(ch, ABILITY_STEALTH) - 8)) {
        stop_fighting(FIGHTING(ch));
        stop_fighting(ch);
      } else {
        send_to_char(ch, "You failed to hide in plain sight!\r\n");
        return;
      }
    } else {
      send_to_char(ch, "You can't do that in combat!\r\n");
      return;
    }
  }

  if (AFF_FLAGGED(ch, AFF_GRAPPLED)) {
    send_to_char(ch, "You are unable to move to make your attempt!\r\n");
    return;
  }

  if (IS_NPC(ch) || !GET_ABILITY(ch, ABILITY_STEALTH)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  if (AFF_FLAGGED(ch, AFF_HIDE)) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);
    send_to_char(ch, "You step out of the shadows...\r\n");
    return;
  }

  send_to_char(ch, "You attempt to hide yourself.\r\n");
  SET_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);
  USE_MOVE_ACTION(ch); /* protect from sniping abuse */
}

/* listen-mode, similar to search - try to find hidden/sneaking targets */
ACMD(do_listen) {
  if (AFF_FLAGGED(ch, AFF_GRAPPLED)) {
    send_to_char(ch, "You are unable to move to make your attempt!\r\n");
    return;
  }

  /* note, you do not require training in perception to attempt */
  /*
  if (IS_NPC(ch) || !GET_ABILITY(ch, ABILITY_PERCEPTION)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  */

  if (AFF_FLAGGED(ch, AFF_LISTEN)) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_LISTEN);
    send_to_char(ch, "You stop trying to listen...\r\n");
    return;
  }

  send_to_char(ch, "You enter listen mode... (movement cost is doubled)\r\n");
  SET_BIT_AR(AFF_FLAGS(ch), AFF_LISTEN);
}

/* spot-mode, similar to search - try to find hidden/sneaking targets */
ACMD(do_spot) {
  if (AFF_FLAGGED(ch, AFF_GRAPPLED)) {
    send_to_char(ch, "You are unable to move to make your attempt!\r\n");
    return;
  }

  /* note, you do not require training in perception to attempt */
  /*
  if (IS_NPC(ch) || !GET_ABILITY(ch, ABILITY_PERCEPTION)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }
  */

  if (AFF_FLAGGED(ch, AFF_SPOT)) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_SPOT);
    send_to_char(ch, "You stop trying to spot...\r\n");
    return;
  }

  send_to_char(ch, "You enter spot mode... (movement cost is doubled)\r\n");
  SET_BIT_AR(AFF_FLAGS(ch), AFF_SPOT);
}

/* fairly stock steal command */
ACMD(do_steal) {
  struct char_data *vict;
  struct obj_data *obj;
  char vict_name[MAX_INPUT_LENGTH], obj_name[MAX_INPUT_LENGTH];
  int percent, gold, eq_pos, pcsteal = 0, ohoh = 0;

  if (IS_NPC(ch) || !GET_ABILITY(ch, ABILITY_SLEIGHT_OF_HAND)) {
    send_to_char(ch, "You have no idea how to do that.\r\n");
    return;
  }

  if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return;
  }

  two_arguments(argument, obj_name, vict_name);

  if (!(vict = get_char_vis(ch, vict_name, NULL, FIND_CHAR_ROOM))) {
    send_to_char(ch, "Steal what from who?\r\n");
    return;
  } else if (vict == ch) {
    send_to_char(ch, "Come on now, that's rather stupid!\r\n");
    return;
  }

  /* 101% is a complete failure */
  percent = rand_number(1, 35);

  if (GET_POS(vict) < POS_SLEEPING)
    percent = -1; /* ALWAYS SUCCESS, unless heavy object. */

  if (!CONFIG_PT_ALLOWED && !IS_NPC(vict))
    pcsteal = 1;

  if (!AWAKE(vict)) /* Easier to steal from sleeping people. */
    percent -= 17;

  /* No stealing if not allowed. If it is no stealing from Imm's or Shopkeepers. */
  if (GET_LEVEL(vict) >= LVL_IMMORT || pcsteal || GET_MOB_SPEC(vict) == shop_keeper)
    percent = 99; /* Failure */

  if (IS_NPC(vict) && MOB_FLAGGED(vict, MOB_NOSTEAL)) {
    send_to_char(ch, "Something about this victim makes it clear this will "
            "not work...\r\n");
    percent = 99;
  }

  if (str_cmp(obj_name, "coins") && str_cmp(obj_name, "gold")) {

    if (!(obj = get_obj_in_list_vis(ch, obj_name, NULL, vict->carrying))) {

      for (eq_pos = 0; eq_pos < NUM_WEARS; eq_pos++)
        if (GET_EQ(vict, eq_pos) &&
                (isname(obj_name, GET_EQ(vict, eq_pos)->name)) &&
                CAN_SEE_OBJ(ch, GET_EQ(vict, eq_pos))) {
          obj = GET_EQ(vict, eq_pos);
          break;
        }
      if (!obj) {
        act("$E hasn't got that item.", FALSE, ch, 0, vict, TO_CHAR);
        return;
      } else { /* It is equipment */
        if ((GET_POS(vict) > POS_STUNNED)) {
          send_to_char(ch, "Steal the equipment now?  Impossible!\r\n");
          return;
        } else {
          if (!give_otrigger(obj, vict, ch) ||
                  !receive_mtrigger(ch, vict, obj)) {
            send_to_char(ch, "Impossible!\r\n");
            return;
          }
          act("You unequip $p and steal it.", FALSE, ch, obj, 0, TO_CHAR);
          act("$n steals $p from $N.", FALSE, ch, obj, vict, TO_NOTVICT);
          obj_to_char(unequip_char(vict, eq_pos), ch);
        }
      }
    } else { /* obj found in inventory */

      percent += GET_OBJ_WEIGHT(obj); /* Make heavy harder */

      if (percent > compute_ability(ch, ABILITY_SLEIGHT_OF_HAND)) {
        ohoh = TRUE;
        send_to_char(ch, "Oops..\r\n");
        act("$n tried to steal something from you!", FALSE, ch, 0, vict, TO_VICT);
        act("$n tries to steal something from $N.", TRUE, ch, 0, vict, TO_NOTVICT);
      } else { /* Steal the item */
        if (IS_CARRYING_N(ch) + 1 < CAN_CARRY_N(ch)) {
          if (!give_otrigger(obj, vict, ch) ||
                  !receive_mtrigger(ch, vict, obj)) {
            send_to_char(ch, "Impossible!\r\n");
            return;
          }
          if (IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj) < CAN_CARRY_W(ch)) {
            obj_from_char(obj);
            obj_to_char(obj, ch);
            send_to_char(ch, "Got it!\r\n");
          }
        } else
          send_to_char(ch, "You cannot carry that much.\r\n");
      }
    }
  } else { /* Steal some coins */
    if (AWAKE(vict) && (percent > compute_ability(ch, ABILITY_SLEIGHT_OF_HAND))) {
      ohoh = TRUE;
      send_to_char(ch, "Oops..\r\n");
      act("You discover that $n has $s hands in your wallet.", FALSE, ch, 0, vict, TO_VICT);
      act("$n tries to steal gold from $N.", TRUE, ch, 0, vict, TO_NOTVICT);
    } else {
      /* Steal some gold coins */
      gold = (GET_GOLD(vict) * rand_number(1, 10)) / 100;
      gold = MIN(1782, gold);
      if (gold > 0) {
        increase_gold(ch, gold);
        decrease_gold(vict, gold);
        if (gold > 1)
          send_to_char(ch, "Bingo!  You got %d gold coins.\r\n", gold);
        else
          send_to_char(ch, "You manage to swipe a solitary gold coin.\r\n");
      } else {
        send_to_char(ch, "You couldn't get any gold...\r\n");
      }
    }
  }

  if (ohoh && IS_NPC(vict) && AWAKE(vict))
    hit(vict, ch, TYPE_UNDEFINED, DAM_RESERVED_DBC, 0, FALSE);

  /* Add wait state, stealing isn't free! */
  USE_STANDARD_ACTION(ch);
}


/* entry point for listing spells, the rest of the code is in spec_procs.c */

/* this only lists spells castable for a given class */
ACMD(do_spells) {
  char arg[MAX_INPUT_LENGTH];
  int class = -1;

  if (IS_NPC(ch))
    return;

  one_argument(argument, arg);

  if (!*arg) {
    send_to_char(ch, "The spells command can use the class name as an argument.\r\n");
    list_spells(ch, 0, class);
  } else {
    class = get_class_by_name(arg);
    if (class < 0 || class >= NUM_CLASSES) {
      send_to_char(ch, "That is not a valid class!\r\n");
      return;
    }
    if (CLASS_LEVEL(ch, class)) {
      list_spells(ch, 0, class);
    } else {
      send_to_char(ch, "You don't have any levels in that class.\r\n");
    }
  }

  send_to_char(ch, "\tDType 'feats' to see your feats\tn\r\n");
  send_to_char(ch, "\tDType 'train' to see your abilities\tn\r\n");
  send_to_char(ch, "\tDType 'boost' to adjust your stats\tn\r\n");
  send_to_char(ch, "\tDType 'spelllist <classname>' to see all your class spells\tn\r\n");
}


/* entry point for listing spells, the rest of the code is in spec_procs.c */

/* this lists all spells attainable for given class */
ACMD(do_spelllist) {
  char arg[MAX_INPUT_LENGTH];
  int class = -1;

  if (IS_NPC(ch))
    return;

  one_argument(argument, arg);

  if (!*arg) {
    send_to_char(ch, "Spelllist also can be utilized - Usage:  spelllist <class name>\r\n");
    list_spells(ch, 1, class);
  } else {
    class = get_class_by_name(arg);
    if (class < 0 || class >= NUM_CLASSES) {
      send_to_char(ch, "That is not a valid class!\r\n");
      return;
    }
    if (CLASS_LEVEL(ch, class)) {
      list_spells(ch, 1, class);
    } else {
      send_to_char(ch, "You don't have any levels in that class.\r\n");
    }
  }

  send_to_char(ch, "\tDType 'feats' to see your feats\tn\r\n");
  send_to_char(ch, "\tDType 'train' to see your abilities\tn\r\n");
  send_to_char(ch, "\tDType 'boost' to adjust your stats\tn\r\n");
  send_to_char(ch, "\tDType 'spells <classname>' to see your currently known spells\tn\r\n");
}

/* entry point for boost (stat training), the rest of code is in
   the guild code in spec_procs */
ACMD(do_boosts) {
  char arg[MAX_INPUT_LENGTH];

  if (IS_NPC(ch))
    return;

  one_argument(argument, arg);

  if (*arg)
    send_to_char(ch, "You can only boost stats in your guild.\r\n");
  else
    send_to_char(ch, "\tCStat boost sessions remaining: %d\tn\r\n"
          "\tcStats:\tn\r\n"
          "Strength\r\n"
          "Constitution\r\n"
          "Dexterity\r\n"
          "Intelligence\r\n"
          "Wisdom\r\n"
          "Charisma\r\n"
          "\tC*Reminder that you can only boost your stats in your guild.\tn\r\n"
          "\r\n",
          GET_BOOSTS(ch));

  send_to_char(ch, "\tDType 'feats' to see your feats\tn\r\n");
  send_to_char(ch, "\tDType 'train' to see your abilities\tn\r\n");
  send_to_char(ch, "\tDType 'boost' to adjust your stats\tn\r\n");
  send_to_char(ch, "\tDType 'spells <classname>' to see your currently known spells\tn\r\n");
}

/* skill practice entry point, the rest of the
 * code is in spec_procs.c guild code */
ACMD(do_practice) {
  char arg[MAX_INPUT_LENGTH];

  if (IS_NPC(ch))
    return;

  one_argument(argument, arg);

  if (*arg)
    send_to_char(ch, "Type '\tYcraft\tn' without an argument to view your crafting skills.\r\n");
  else
    list_crafting_skills(ch);

  send_to_char(ch, "\tDType 'feats' to see your feats\tn\r\n");
  send_to_char(ch, "\tDType 'train' to see your abilities\tn\r\n");
  send_to_char(ch, "\tDType 'boost' to adjust your stats\tn\r\n");
  if (IS_CASTER(ch)) {
    send_to_char(ch, "\tDType 'spells' to see your spells\tn\r\n");
  }

}

/* ability training entry point, the rest of the
 * code is in spec_procs.c guild code */
ACMD(do_train) {
  char arg[MAX_INPUT_LENGTH];

  if (IS_NPC(ch))
    return;

  one_argument(argument, arg);

  if (*arg && is_abbrev(arg, "knowledge")) {
    /* Display knowledge abilities. */
    list_abilities(ch, ABILITY_TYPE_KNOWLEDGE);
  } else if (*arg && is_abbrev(arg, "craft")) {
    /* Display craft abilities. */
    list_abilities(ch, ABILITY_TYPE_CRAFT);
  } else if (*arg)
    send_to_char(ch, "You can only train abilities in your guild.\r\n");
  else
    list_abilities(ch, ABILITY_TYPE_GENERAL);

  send_to_char(ch, "\tDType 'train knowledge' to see your knowledge abilities\tn\r\n");
  /* as of 10/30/2014, we have decided to make sure crafting is an indepedent system */
  //send_to_char(ch, "\tDType 'train craft' to see your crafting abilities\tn\r\n");
  send_to_char(ch, "\tDType 'craft' to see your crafting abilities\tn\r\n");
  send_to_char(ch, "\tDType 'boost' to adjust your stats\tn\r\n");
  if (IS_CASTER(ch)) {
    send_to_char(ch, "\tDType 'spells' to see your spells\tn\r\n");
  }
}

/* general command to drop any invisibility affects */
ACMD(do_visible) {
  if (GET_LEVEL(ch) >= LVL_IMMORT) {
    perform_immort_vis(ch);
    return;
  }

  if AFF_FLAGGED(ch, AFF_INVISIBLE) {
    appear(ch, TRUE); //forced for greater invis
    send_to_char(ch, "You break the spell of invisibility.\r\n");
  } else
    send_to_char(ch, "You are already visible.\r\n");
}

ACMD(do_title) {
  skip_spaces(&argument);
  delete_doubledollar(argument);
  parse_at(argument);

  if (IS_NPC(ch))
    send_to_char(ch, "Your title is fine... go away.\r\n");
  else if (PLR_FLAGGED(ch, PLR_NOTITLE))
    send_to_char(ch, "You can't title yourself -- you shouldn't have abused it!\r\n");
  else if (strstr(argument, "(") || strstr(argument, ")"))
    send_to_char(ch, "Titles can't contain the ( or ) characters.\r\n");
  else if (strlen(argument) > MAX_TITLE_LENGTH)
    send_to_char(ch, "Sorry, titles can't be longer than %d characters.\r\n", MAX_TITLE_LENGTH);
  else {
    set_title(ch, argument);
    send_to_char(ch, "Okay, you're now %s%s%s.\r\n", GET_NAME(ch), *GET_TITLE(ch) ? " " : "", GET_TITLE(ch));
  }
}

static void print_group(struct char_data *ch) {
  struct char_data *k;

  send_to_char(ch, "Your group consists of:\r\n");

  while ((k = (struct char_data *) simple_list(ch->group->members)) != NULL)
    send_to_char(ch, "%-*s: %s[%4d/%-4d]H [%4d/%-4d]M [%4d/%-4d]V%s\r\n",
          count_color_chars(GET_NAME(k)) + 22, GET_NAME(k),
          GROUP_LEADER(GROUP(ch)) == k ? CBGRN(ch, C_NRM) : CCGRN(ch, C_NRM),
          GET_HIT(k), GET_MAX_HIT(k),
          GET_MANA(k), GET_MAX_MANA(k),
          GET_MOVE(k), GET_MAX_MOVE(k),
          CCNRM(ch, C_NRM));
}

static void display_group_list(struct char_data * ch) {
  struct group_data * group;
  int count = 0;

  if (group_list->iSize) {
    send_to_char(ch,
            "#   Group Leader     # of Mem  Open?  In Zone\r\n"
            "-------------------------------------------------------------------\r\n");

    while ((group = (struct group_data *) simple_list(group_list)) != NULL) {
      /* we don't display npc groups */
      if (IS_SET(GROUP_FLAGS(group), GROUP_NPC))
        continue;
      if (GROUP_LEADER(group) && !IS_SET(GROUP_FLAGS(group), GROUP_ANON))
        send_to_char(ch, "%-2d) %s%-12s     %-2d        %-3s    %s%s\r\n",
              ++count, IS_SET(GROUP_FLAGS(group), GROUP_OPEN) ? CCGRN(ch, C_NRM) :
              CCRED(ch, C_NRM), GET_NAME(GROUP_LEADER(group)),
              group->members->iSize, IS_SET(GROUP_FLAGS(group), GROUP_OPEN) ?
              "\tWYes\tn" : "\tRNo \tn",
              zone_table[world[IN_ROOM(GROUP_LEADER(group))].zone].name,
              CCNRM(ch, C_NRM));
      else
        send_to_char(ch, "%-2d) Hidden\r\n", ++count);

    }
  }

  if (count)
    send_to_char(ch, "\r\n");
    /*
                       "%sSeeking Members%s\r\n"
                       "%sClosed%s\r\n",
                       CCGRN(ch, C_NRM), CCNRM(ch, C_NRM),
                       CCRED(ch, C_NRM), CCNRM(ch, C_NRM));*/
  else
    send_to_char(ch, "\r\n"
          "Currently no groups formed.\r\n");
}


//vatiken's group system 1.2, installed 08/08/12

ACMD(do_group) {
  char buf[MAX_STRING_LENGTH];
  struct char_data *vict;
  argument = one_argument(argument, buf);

  if (!*buf) {
    if (GROUP(ch))
      print_group(ch);
    else
      send_to_char(ch, "You must specify a group option, or type HELP GROUP for more info.\r\n");
    return;
  }

  if (is_abbrev(buf, "new")) {
    if (GROUP(ch))
      send_to_char(ch, "You are already in a group.\r\n");
    else
      create_group(ch);
  } else if (is_abbrev(buf, "list"))
    display_group_list(ch);
  else if (is_abbrev(buf, "join")) {
    skip_spaces(&argument);
    if (!(vict = get_char_vis(ch, argument, NULL, FIND_CHAR_WORLD))) {
      send_to_char(ch, "Join who?\r\n");
      return;
    } else if (vict == ch) {
      send_to_char(ch, "That would be one lonely grouping.\r\n");
      return;
    } else if (GROUP(ch)) {
      send_to_char(ch, "But you are already part of a group.\r\n");
      return;
    } else if (!GROUP(vict)) {
      send_to_char(ch, "They are not a part of a group!\r\n");
      return;
    } else if (IS_NPC(vict)) {
      send_to_char(ch, "You can't join that group!\r\n");
      return;
    } else if (!IS_SET(GROUP_FLAGS(GROUP(vict)), GROUP_OPEN)) {
      send_to_char(ch, "That group isn't accepting members.\r\n");
      return;
    }
    join_group(ch, GROUP(vict));
  } else if (is_abbrev(buf, "kick")) {
    skip_spaces(&argument);
    if (!(vict = get_char_vis(ch, argument, NULL, FIND_CHAR_ROOM))) {
      send_to_char(ch, "Kick out who?\r\n");
      return;
    } else if (vict == ch) {
      send_to_char(ch, "There are easier ways to leave the group.\r\n");
      return;
    } else if (!GROUP(ch)) {
      send_to_char(ch, "But you are not part of a group.\r\n");
      return;
    } else if (GROUP_LEADER(GROUP(ch)) != ch) {
      send_to_char(ch, "Only the group's leader can kick members out.\r\n");
      return;
    } else if (GROUP(vict) != GROUP(ch)) {
      send_to_char(ch, "They are not a member of your group!\r\n");
      return;
    }
    send_to_char(ch, "You have kicked %s out of the group.\r\n", GET_NAME(vict));
    send_to_char(vict, "You have been kicked out of the group.\r\n");
    leave_group(vict);
  } else if (is_abbrev(buf, "leave")) {
    if (!GROUP(ch)) {
      send_to_char(ch, "But you aren't apart of a group!\r\n");
      return;
    }
    leave_group(ch);
  } else if (is_abbrev(buf, "option")) {
    skip_spaces(&argument);
    if (!GROUP(ch)) {
      send_to_char(ch, "But you aren't part of a group!\r\n");
      return;
    } else if (GROUP_LEADER(GROUP(ch)) != ch) {
      send_to_char(ch, "Only the group leader can adjust the group flags.\r\n");
      return;
    }

    if (is_abbrev(argument, "open")) {
      TOGGLE_BIT(GROUP_FLAGS(GROUP(ch)), GROUP_OPEN);
      send_to_char(ch, "The group is now %s to new members.\r\n",
              IS_SET(GROUP_FLAGS(GROUP(ch)), GROUP_OPEN) ? "open" : "closed");
    } else if (is_abbrev(argument, "anonymous")) {
      TOGGLE_BIT(GROUP_FLAGS(GROUP(ch)), GROUP_ANON);
      send_to_char(ch, "The group location is now %s to other players.\r\n",
              IS_SET(GROUP_FLAGS(GROUP(ch)), GROUP_ANON) ? "invisible" : "visible");
    } else
      send_to_char(ch, "The flag options are: Open, Anonymous\r\n");
  } else {
    send_to_char(ch, "You must specify a group option, or type HELP GROUP for more info.\r\n");
  }

}

/* the actual group report command */
ACMD(do_greport) {
  struct group_data *group;

  if ((group = GROUP(ch)) == NULL) {
    send_to_char(ch, "But you are not a member of any group!\r\n");
    return;
  }

  send_to_group(NULL, group, "%s reports: %d/%dH, %d/%dM, %d/%dV\r\n",
          GET_NAME(ch), GET_HIT(ch), GET_MAX_HIT(ch),
          GET_MANA(ch), GET_MAX_MANA(ch),
          GET_MOVE(ch), GET_MAX_MOVE(ch));
}

/* this use to be group report, switched it to general */
ACMD(do_report) {

  /* generalized output due to send_to_room */
  send_to_room(IN_ROOM(ch), "%s status: %d/%dH, %d/%dM, %d/%dV\r\n",
          GET_NAME(ch), GET_HIT(ch), GET_MAX_HIT(ch),
          GET_MANA(ch), GET_MAX_MANA(ch),
          GET_MOVE(ch), GET_MAX_MOVE(ch));
}

ACMD(do_split) {
  char buf[MAX_INPUT_LENGTH];
  int amount, num = 0, share, rest;
  size_t len;
  struct char_data *k;

  if (IS_NPC(ch))
    return;

  one_argument(argument, buf);

  if (is_number(buf)) {
    amount = atoi(buf);
    if (amount <= 0) {
      send_to_char(ch, "Sorry, you can't do that.\r\n");
      return;
    }
    if (amount > GET_GOLD(ch)) {
      send_to_char(ch, "You don't seem to have that much gold to split.\r\n");
      return;
    }

    if (GROUP(ch))
      while ((k = (struct char_data *) simple_list(GROUP(ch)->members)) != NULL)
        if (IN_ROOM(ch) == IN_ROOM(k) && !IS_NPC(k))
          num++;

    if (num && GROUP(ch)) {
      share = amount / num;
      rest = amount % num;
    } else {
      send_to_char(ch, "With whom do you wish to share your gold?\r\n");
      return;
    }

    decrease_gold(ch, share * (num - 1));

    /* Abusing signed/unsigned to make sizeof work. */
    len = snprintf(buf, sizeof (buf), "%s splits %d coins; you receive %d.\r\n",
            GET_NAME(ch), amount, share);
    if (rest && len < sizeof (buf)) {
      snprintf(buf + len, sizeof (buf) - len,
              "%d coin%s %s not splitable, so %s keeps the money.\r\n", rest,
              (rest == 1) ? "" : "s", (rest == 1) ? "was" : "were", GET_NAME(ch));
    }

    while ((k = (struct char_data *) simple_list(GROUP(ch)->members)) != NULL)
      if (k != ch && IN_ROOM(ch) == IN_ROOM(k) && !IS_NPC(k)) {
        increase_gold(k, share);
        send_to_char(k, "%s", buf);
      }
    send_to_char(ch, "You split %d coins among %d members -- %d coins each.\r\n",
            amount, num, share);

    if (rest) {
      send_to_char(ch, "%d coin%s %s not splitable, so you keep the money.\r\n",
              rest, (rest == 1) ? "" : "s", (rest == 1) ? "was" : "were");
      increase_gold(ch, rest);
    }
  } else {
    send_to_char(ch, "How many coins do you wish to split with your group?\r\n");
    return;
  }
}

ACMD(do_use) {
  char buf[MAX_INPUT_LENGTH] = {'\0'}, arg[MAX_INPUT_LENGTH] = {'\0'};
  struct obj_data *mag_item = NULL;
  int dc = 10;
  int check_result;
  int spell;
  int umd_ability_score;

  half_chop(argument, arg, buf);


  if (!*arg) {
    send_to_char(ch, "What do you want to %s?\r\n", CMD_NAME);
    return;
  }
  mag_item = GET_EQ(ch, WEAR_HOLD_1);

  if (!mag_item || !isname(arg, mag_item->name)) {
    switch (subcmd) {
      case SCMD_RECITE:
      case SCMD_QUAFF:
        if (!(mag_item = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
          send_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
          return;
        }
        break;
      case SCMD_USE:
        send_to_char(ch, "You don't seem to be holding %s %s.\r\n",
                AN(arg), arg);
        return;
      default:
        log("SYSERR: Unknown subcmd %d passed to do_use.", subcmd);
        /* SYSERR_DESC: This is the same as the unhandled case in do_gen_ps(),
         * but in the function which handles 'quaff', 'recite', and 'use'. */
        return;
    }
  }

  /* Check for object existence. */
  switch (subcmd) {
    case SCMD_QUAFF:
      if (GET_OBJ_TYPE(mag_item) != ITEM_POTION) {
        send_to_char(ch, "You can only quaff potions.\r\n");
        return;
      }
      break;
    case SCMD_RECITE:
      if (GET_OBJ_TYPE(mag_item) != ITEM_SCROLL) {
        send_to_char(ch, "You can only recite scrolls.\r\n");
        return;
      }
      break;
    case SCMD_USE:
      if ((GET_OBJ_TYPE(mag_item) != ITEM_WAND) &&
          (GET_OBJ_TYPE(mag_item) != ITEM_STAFF)  &&
          (GET_OBJ_TYPE(mag_item) != ITEM_WEAPON && HAS_SPECIAL_ABILITIES(mag_item))) {
        send_to_char(ch, "You can't seem to figure out how to use it.\r\n");
        return;
      }
      break;
  }

  /* Check if we can actually use the item in question... */
  switch (subcmd) {

    case SCMD_RECITE:

      /* 1. Decipher Writing
       *    Spellcraft check: DC 20 + spell level */

      dc = 20 + GET_OBJ_VAL(mag_item, 0);
      if (((check_result = skill_check(ch, ABILITY_SPELLCRAFT, dc)) < 0) &&
          ((check_result = skill_check(ch, ABILITY_USE_MAGIC_DEVICE, dc + 5)) < 0))
      {
        send_to_char(ch, "You are unable to decipher the magical writings!\r\n");
        return;
      }
      /* 2. Activate the Spell */
      /* 2.a. Check the spell type
       *      ARCANE - Wizard, Sorcerer, Bard
       *      DIVINE - Cleric, Druid, Paladin, Ranger */
      spell = GET_OBJ_VAL(mag_item, 1);
      if ((check_result = skill_check(ch, ABILITY_USE_MAGIC_DEVICE, dc)) < 0)
      {
        if(spell_info[spell].min_level[CLASS_WIZARD]   < LVL_STAFF ||
           spell_info[spell].min_level[CLASS_SORCERER] < LVL_STAFF ||
           spell_info[spell].min_level[CLASS_BARD]     < LVL_STAFF)
        {
          if (!(CLASS_LEVEL(ch, CLASS_WIZARD)   > 0 ||
              CLASS_LEVEL(ch, CLASS_SORCERER) > 0 ||
              CLASS_LEVEL(ch, CLASS_BARD)     > 0))
          {
            send_to_char(ch, "You must be able to use arcane magic to recite this scroll.\r\n");
            return;
          }
        } else {
          if(!(CLASS_LEVEL(ch, CLASS_CLERIC) > 0 ||
               CLASS_LEVEL(ch, CLASS_DRUID) > 0 ||
               CLASS_LEVEL(ch, CLASS_PALADIN) > 0 ||
               CLASS_LEVEL(ch, CLASS_RANGER) > 0))
          {
            send_to_char(ch, "You must be able to cast divine magic to recite this scroll.\r\n");
            return;
          }
        }

        /* 2.b. Check the spell is on class spell list */
        if (!(((spell_info[spell].min_level[CLASS_WIZARD]   < LVL_STAFF) && CLASS_LEVEL(ch, CLASS_WIZARD) > 0) ||
              ((spell_info[spell].min_level[CLASS_SORCERER] < LVL_STAFF) && CLASS_LEVEL(ch, CLASS_SORCERER) > 0) ||
              ((spell_info[spell].min_level[CLASS_BARD]     < LVL_STAFF) && CLASS_LEVEL(ch, CLASS_BARD) > 0) ||
              ((spell_info[spell].min_level[CLASS_CLERIC]   < LVL_STAFF) && CLASS_LEVEL(ch, CLASS_CLERIC) > 0) ||
              ((spell_info[spell].min_level[CLASS_DRUID]    < LVL_STAFF) && CLASS_LEVEL(ch, CLASS_DRUID) > 0) ||
              ((spell_info[spell].min_level[CLASS_PALADIN]  < LVL_STAFF) && CLASS_LEVEL(ch, CLASS_PALADIN) > 0) ||
              ((spell_info[spell].min_level[CLASS_RANGER]   < LVL_STAFF) && CLASS_LEVEL(ch, CLASS_RANGER) > 0)))
        {
          send_to_char(ch, "The spell on the scroll is outside your realm of knowledge.\r\n");
          return;
        }
      }
      /* 2.c. Check the relevant ability score */
      umd_ability_score = (skill_check(ch, ABILITY_USE_MAGIC_DEVICE, 15));
      bool passed = FALSE;
      if (spell_info[spell].min_level[CLASS_WIZARD] < LVL_STAFF)
        passed = (((GET_INT(ch) > umd_ability_score) ? GET_INT(ch) : umd_ability_score) > (10 + spellCircle(CLASS_WIZARD, spell)) ? TRUE : passed);
      if (spell_info[spell].min_level[CLASS_SORCERER] < LVL_STAFF)
        passed = (((GET_CHA(ch) > umd_ability_score) ? GET_CHA(ch) : umd_ability_score) > (10 + spellCircle(CLASS_SORCERER, spell)) ? TRUE : passed);
      if (spell_info[spell].min_level[CLASS_BARD] < LVL_STAFF)
        passed = (((GET_CHA(ch) > umd_ability_score) ? GET_CHA(ch) : umd_ability_score) > (10 + spellCircle(CLASS_BARD, spell)) ? TRUE : passed);
      if (spell_info[spell].min_level[CLASS_CLERIC] < LVL_STAFF)
        passed = (((GET_WIS(ch) > umd_ability_score) ? GET_WIS(ch) : umd_ability_score) > (10 + spellCircle(CLASS_CLERIC, spell)) ? TRUE : passed);
      if (spell_info[spell].min_level[CLASS_DRUID] < LVL_STAFF)
        passed = (((GET_WIS(ch) > umd_ability_score) ? GET_WIS(ch) : umd_ability_score) > (10 + spellCircle(CLASS_DRUID, spell)) ? TRUE : passed);
      if (spell_info[spell].min_level[CLASS_PALADIN] < LVL_STAFF)
        passed = (((GET_CHA(ch) > umd_ability_score) ? GET_CHA(ch) : umd_ability_score) > (10 + spellCircle(CLASS_PALADIN, spell)) ? TRUE : passed);
      if (spell_info[spell].min_level[CLASS_RANGER] < LVL_STAFF)
        passed = (((GET_WIS(ch) > umd_ability_score) ? GET_WIS(ch) : umd_ability_score) > (10 + spellCircle(CLASS_RANGER, spell)) ? TRUE : passed);
      if (passed == FALSE)
      {
        send_to_char(ch, "You are physically incapable of casting the spell inscribed on the scroll.\r\n");
        return;
      }
      /* 3. Check caster level */
      if ((CASTER_LEVEL(ch) < GET_OBJ_VAL(mag_item, 0)) &&
          (check_result && GET_LEVEL(ch) < GET_OBJ_VAL(mag_item, 0)))
      {
        /* Perform caster level check */
        dc = GET_OBJ_VAL(mag_item, 0) + 1;
        if (dice(1, 20) + (((check_result >= 0) && (CASTER_LEVEL(ch) < GET_LEVEL(ch))) ? GET_LEVEL(ch) : CASTER_LEVEL(ch)) < dc)
        {
          /* Fail */
          send_to_char(ch, "You try, but the spell on the scroll is far to powerful for you to cast.\r\n");
          return;
        } else {
          send_to_char(ch, "You release the powerful magics inscribed the scroll!\r\n");
        }
      }
      break;
    case SCMD_USE:
      /* Check the item type */
      switch (GET_OBJ_TYPE(mag_item)) {
        case ITEM_WEAPON:
          /* Special Abilities */
          break;
        case ITEM_WAND:
        case ITEM_STAFF:
          /* Check requirements for using a wand: Spell Trigger method */
          /* 1. Class must be able to cast the spell stored in the wand. Use Magic Device can bluff this. */
          spell = GET_OBJ_VAL(mag_item, 3);
          dc = 20;
          if ((check_result = skill_check(ch, ABILITY_USE_MAGIC_DEVICE, dc)) < 0)
          {
            if(spell_info[spell].min_level[CLASS_WIZARD]   < LVL_STAFF ||
               spell_info[spell].min_level[CLASS_SORCERER] < LVL_STAFF ||
               spell_info[spell].min_level[CLASS_BARD]     < LVL_STAFF)
            {
              if (!(CLASS_LEVEL(ch, CLASS_WIZARD)   > 0 ||
                  CLASS_LEVEL(ch, CLASS_SORCERER) > 0 ||
                  CLASS_LEVEL(ch, CLASS_BARD)     > 0))
              {
                send_to_char(ch, "You must be able to use arcane magic to use this %s.\r\n",
                             GET_OBJ_TYPE(mag_item) == ITEM_WAND ? "wand" : "staff");
                return;
              }
            } else {
              if(!(CLASS_LEVEL(ch, CLASS_CLERIC) > 0 ||
                   CLASS_LEVEL(ch, CLASS_DRUID) > 0 ||
                   CLASS_LEVEL(ch, CLASS_PALADIN) > 0 ||
                   CLASS_LEVEL(ch, CLASS_RANGER) > 0))
              {
                send_to_char(ch, "You must be able to cast divine magic to use this %s.\r\n",
                             GET_OBJ_TYPE(mag_item) == ITEM_WAND ? "wand" : "staff");
                return;
              }
            }

            /* 1.b. Check the spell is on class spell list */
            if (!(((spell_info[spell].min_level[CLASS_WIZARD]   < LVL_STAFF) && CLASS_LEVEL(ch, CLASS_WIZARD) > 0) ||
                  ((spell_info[spell].min_level[CLASS_SORCERER] < LVL_STAFF) && CLASS_LEVEL(ch, CLASS_SORCERER) > 0) ||
                  ((spell_info[spell].min_level[CLASS_BARD]     < LVL_STAFF) && CLASS_LEVEL(ch, CLASS_BARD) > 0) ||
                  ((spell_info[spell].min_level[CLASS_CLERIC]   < LVL_STAFF) && CLASS_LEVEL(ch, CLASS_CLERIC) > 0) ||
                  ((spell_info[spell].min_level[CLASS_DRUID]    < LVL_STAFF) && CLASS_LEVEL(ch, CLASS_DRUID) > 0) ||
                  ((spell_info[spell].min_level[CLASS_PALADIN]  < LVL_STAFF) && CLASS_LEVEL(ch, CLASS_PALADIN) > 0) ||
                  ((spell_info[spell].min_level[CLASS_RANGER]   < LVL_STAFF) && CLASS_LEVEL(ch, CLASS_RANGER) > 0)))
            {
              send_to_char(ch, "The spell stored in the %s is outside your realm of knowledge.\r\n",
                           GET_OBJ_TYPE(mag_item) == ITEM_WAND ? "wand" : "staff");
              return;
            }
          }

          break;
      }
      break;
  }

  mag_objectmagic(ch, mag_item, buf);
}

/* Activate a magic item with a COMMAND WORD! */
ACMD(do_utter) {
  int i = 0;
  int found = 0;
  struct obj_data *mag_item = NULL;

  skip_spaces(&argument);

  if (!*argument) {
    send_to_char(ch, "Utter what?\r\n");
    return;
  } else {
    send_to_char(ch, "You utter '%s'.\r\n", argument);
    act("$n utters a command word, too quietly to hear.", TRUE, ch, 0, 0, TO_ROOM);
  }

  /* Check all worn/wielded items and see if they have a command word. */
  for (i = 0; i < NUM_WEARS; i++) {
    mag_item = GET_EQ(ch, i);
    if (mag_item != NULL) {
      switch (i) { /* Different procedures for weapons and armors. */
        case WEAR_WIELD_1:
        case WEAR_WIELD_2:
        case WEAR_WIELD_2H:
          found += process_weapon_abilities(mag_item, ch, NULL, ACTMTD_COMMAND_WORD, argument);
          break;
        default:
          break;
      }
    }
  }
  if (found == 0)
    send_to_char(ch, "Nothing happens.\r\n");
  else
    USE_STANDARD_ACTION(ch);

}

ACMD(do_display) {
  size_t i;

  if (IS_NPC(ch)) {
    send_to_char(ch, "Monsters don't need displays.  Go away.\r\n");
    return;
  }
  skip_spaces(&argument);

  if (!*argument) {
    send_to_char(ch, "Usage: prompt { { H | M | V | X | T | R | E } | all |"
            " auto | none }\r\n");
    send_to_char(ch, "Notice this command is deprecated, we recommend using "
            " PREFEDIT instead.\r\n");
    return;
  }

  if (!str_cmp(argument, "auto")) {
    TOGGLE_BIT_AR(PRF_FLAGS(ch), PRF_DISPAUTO);
    send_to_char(ch, "Auto prompt %sabled.\r\n", PRF_FLAGGED(ch, PRF_DISPAUTO) ? "en" : "dis");
    return;
  }

  if (!str_cmp(argument, "on") || !str_cmp(argument, "all")) {
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPAUTO);

    SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPHP);
    SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPMANA);
    SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPMOVE);
    SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPEXP);
    SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPEXITS);
    SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPROOM);
    SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPMEMTIME);
  } else if (!str_cmp(argument, "off") || !str_cmp(argument, "none")) {
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPAUTO);

    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPHP);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPMANA);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPMOVE);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPEXP);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPEXITS);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPROOM);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPMEMTIME);
  } else {
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPAUTO);

    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPHP);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPMANA);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPMOVE);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPEXP);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPEXITS);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPROOM);
    REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_DISPMEMTIME);

    for (i = 0; i < strlen(argument); i++) {
      switch (LOWER(argument[i])) {
        case 'h':
          SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPHP);
          break;
        case 'm':
          SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPMANA);
          break;
        case 'v':
          SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPMOVE);
          break;
        case 'x':
          SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPEXP);
          break;
        case 't':
          SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPEXITS);
          break;
        case 'r':
          SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPROOM);
          break;
        case 'e':
          SET_BIT_AR(PRF_FLAGS(ch), PRF_DISPMEMTIME);
          break;
        default:
          send_to_char(ch, "Usage: prompt { { H | M | V | X | T | R | E } | all"
                  " | auto | none }\r\n");
          return;
      }
    }
  }

  send_to_char(ch, "%s", CONFIG_OK);
}

#define TOG_OFF 0
#define TOG_ON  1

ACMD(do_gen_tog) {
  long result;
  int i;
  char arg[MAX_INPUT_LENGTH];

  const char *tog_messages[][2] = {
    /*0*/
    {"You are now safe from summoning by other players.\r\n",
      "You may now be summoned by other players.\r\n"},
    /*1*/
    {"Nohassle disabled.\r\n",
      "Nohassle enabled.\r\n"},
    /*2*/
    {"Brief mode off.\r\n",
      "Brief mode on.\r\n"},
    /*3*/
    {"Compact mode off.\r\n",
      "Compact mode on.\r\n"},
    /*4*/
    {"You can now hear tells.\r\n",
      "You are now deaf to tells.\r\n"},
    /*5*/
    {"You can now hear auctions.\r\n",
      "You are now deaf to auctions.\r\n"},
    /*6*/
    {"You can now hear shouts.\r\n",
      "You are now deaf to shouts.\r\n"},
    /*7*/
    {"You can now hear gossip.\r\n",
      "You are now deaf to gossip.\r\n"},
    /*8*/
    {"You can now hear the congratulation messages.\r\n",
      "You are now deaf to the congratulation messages.\r\n"},
    /*9*/
    {"You can now hear the Wiz-channel.\r\n",
      "You are now deaf to the Wiz-channel.\r\n"},
    /*10*/
    {"You are no longer part of the Quest.\r\n",
      "Okay, you are part of the Quest!\r\n"},
    /*11*/
    {"You will no longer see the room flags.\r\n",
      "You will now see the room flags.\r\n"},
    /*12*/
    {"You will now have your communication repeated.\r\n",
      "You will no longer have your communication repeated.\r\n"},
    /*13*/
    {"HolyLight mode off.\r\n",
      "HolyLight mode on.\r\n"},
    /*14*/
    {"Nameserver_is_slow changed to NO; IP addresses will now be resolved.\r\n",
      "Nameserver_is_slow changed to YES; sitenames will no longer be resolved.\r\n"},
    /*15*/
    {"Autoexits disabled.\r\n",
      "Autoexits enabled.\r\n"},
    /*16*/
    {"Will no longer track through doors.\r\n",
      "Will now track through doors.\r\n"},
    /*17*/
    {"Will no longer clear screen in OLC.\r\n",
      "Will now clear screen in OLC.\r\n"},
    /*18*/
    {"Buildwalk Off.\r\n",
      "Buildwalk On.\r\n"},
    /*19*/
    {"AFK flag is now off.\r\n",
      "AFK flag is now on.\r\n"},
    /*20*/
    {"Autoloot disabled.\r\n",
      "Autoloot enabled.\r\n"},
    /*21*/
    {"Autogold disabled.\r\n",
      "Autogold enabled.\r\n"},
    /*22*/
    {"Autosplit disabled.\r\n",
      "Autosplit enabled.\r\n"},
    /*23*/
    {"Autosacrifice disabled.\r\n",
      "Autosacrifice enabled.\r\n"},
    /*24*/
    {"Autoassist disabled.\r\n",
      "Autoassist enabled.\r\n"},
    /*25*/
    {"Automap disabled.\r\n",
      "Automap enabled.\r\n"},
    /*26*/
    {"Autokey disabled.\r\n",
      "Autokey enabled.\r\n"},
    /*27*/
    {"Autodoor disabled.\r\n",
      "Autodoor enabled.\r\n"},
    /*28*/
    {"You are now able to see all clantalk.\r\n",
      "Clantalk channels disabled.\r\n"},
    /*29*/
    {"COLOR DISABLE\r\n",
      "COLOR ENABLE\r\n"},
    /*30*/
    {"SYSLOG DISABLE\r\n",
      "SYSLOG ENABLE\r\n"},
    /*31*/
    {"WIMPY DISABLE\r\n",
      "WIMPY ENABLE\r\n"},
    /*32*/
    {"PAGELENGTH DISABLE\r\n",
      "PAGELENGTH ENABLE\r\n"},
    /*33*/
    {"SCREENWIDTH DISABLE\r\n",
      "SCREENWIDTH DISABLE\r\n"},
    /*34*/
    {"Autoscan disabled.\r\n",
      "Autoscan enabled.\r\n"}
  };

  if (IS_NPC(ch))
    return;

  switch (subcmd) {
    case SCMD_NOSUMMON:
      result = PRF_TOG_CHK(ch, PRF_SUMMONABLE);
      break;
    case SCMD_NOHASSLE:
      result = PRF_TOG_CHK(ch, PRF_NOHASSLE);
      break;
    case SCMD_BRIEF:
      result = PRF_TOG_CHK(ch, PRF_BRIEF);
      break;
    case SCMD_COMPACT:
      result = PRF_TOG_CHK(ch, PRF_COMPACT);
      break;
    case SCMD_NOTELL:
      result = PRF_TOG_CHK(ch, PRF_NOTELL);
      break;
    case SCMD_NOAUCTION:
      result = PRF_TOG_CHK(ch, PRF_NOAUCT);
      break;
    case SCMD_NOSHOUT:
      result = PRF_TOG_CHK(ch, PRF_NOSHOUT);
      break;
    case SCMD_NOGOSSIP:
      result = PRF_TOG_CHK(ch, PRF_NOGOSS);
      break;
    case SCMD_NOGRATZ:
      result = PRF_TOG_CHK(ch, PRF_NOGRATZ);
      break;
    case SCMD_NOWIZ:
      result = PRF_TOG_CHK(ch, PRF_NOWIZ);
      break;
    case SCMD_QUEST:
      result = PRF_TOG_CHK(ch, PRF_QUEST);
      break;
    case SCMD_SHOWVNUMS:
      result = PRF_TOG_CHK(ch, PRF_SHOWVNUMS);
      break;
    case SCMD_NOREPEAT:
      result = PRF_TOG_CHK(ch, PRF_NOREPEAT);
      break;
    case SCMD_HOLYLIGHT:
      result = PRF_TOG_CHK(ch, PRF_HOLYLIGHT);
      break;
    case SCMD_AUTOEXIT:
      result = PRF_TOG_CHK(ch, PRF_AUTOEXIT);
      break;
    case SCMD_CLS:
      result = PRF_TOG_CHK(ch, PRF_CLS);
      break;
    case SCMD_BUILDWALK:
      if (GET_LEVEL(ch) < LVL_BUILDER) {
        send_to_char(ch, "Builders only, sorry.\r\n");
        return;
      }
      result = PRF_TOG_CHK(ch, PRF_BUILDWALK);
      if (PRF_FLAGGED(ch, PRF_BUILDWALK)) {
        one_argument(argument, arg);
        for (i = 0; *arg && *(sector_types[i]) != '\n'; i++)
          if (is_abbrev(arg, sector_types[i]))
            break;
        if (*(sector_types[i]) == '\n')
          i = 0;
        GET_BUILDWALK_SECTOR(ch) = i;
        send_to_char(ch, "Default sector type is %s\r\n", sector_types[i]);

        mudlog(CMP, GET_LEVEL(ch), TRUE,
                "OLC: %s turned buildwalk on. Allowed zone %d", GET_NAME(ch), GET_OLC_ZONE(ch));
      } else
        mudlog(CMP, GET_LEVEL(ch), TRUE,
              "OLC: %s turned buildwalk off. Allowed zone %d", GET_NAME(ch), GET_OLC_ZONE(ch));
      break;
    case SCMD_AFK:
      result = PRF_TOG_CHK(ch, PRF_AFK);
      if (PRF_FLAGGED(ch, PRF_AFK))
        act("$n has gone AFK.", TRUE, ch, 0, 0, TO_ROOM);
      else {
        act("$n has come back from AFK.", TRUE, ch, 0, 0, TO_ROOM);
        if (has_mail(GET_IDNUM(ch)))
          send_to_char(ch, "You have mail waiting.\r\n");
      }
      break;
    case SCMD_AUTOLOOT:
      result = PRF_TOG_CHK(ch, PRF_AUTOLOOT);
      break;
    case SCMD_AUTOGOLD:
      result = PRF_TOG_CHK(ch, PRF_AUTOGOLD);
      break;
    case SCMD_AUTOSPLIT:
      result = PRF_TOG_CHK(ch, PRF_AUTOSPLIT);
      break;
    case SCMD_AUTOSAC:
      result = PRF_TOG_CHK(ch, PRF_AUTOSAC);
      break;
    case SCMD_AUTOASSIST:
      result = PRF_TOG_CHK(ch, PRF_AUTOASSIST);
      break;
    case SCMD_AUTOMAP:
      result = PRF_TOG_CHK(ch, PRF_AUTOMAP);
      break;
    case SCMD_AUTOKEY:
      result = PRF_TOG_CHK(ch, PRF_AUTOKEY);
      break;
    case SCMD_AUTODOOR:
      result = PRF_TOG_CHK(ch, PRF_AUTODOOR);
      break;
    case SCMD_NOCLANTALK:
      result = PRF_TOG_CHK(ch, PRF_NOCLANTALK);
      break;
    case SCMD_AUTOSCAN:
      result = PRF_TOG_CHK(ch, PRF_AUTOSCAN);
      break;
    default:
      log("SYSERR: Unknown subcmd %d in do_gen_toggle.", subcmd);
      return;
  }

  if (result)
    send_to_char(ch, "%s", tog_messages[subcmd][TOG_ON]);
  else
    send_to_char(ch, "%s", tog_messages[subcmd][TOG_OFF]);

  return;
}

/*a general diplomacy skill - popularity increase is determined by SCMD */
struct diplomacy_data diplomacy_types[] = {
  { SCMD_MURMUR, SKILL_MURMUR, 0.75, 1}, /**< Murmur skill, 0.75% increase, 1 tick wait  */
  { SCMD_PROPAGANDA, SKILL_PROPAGANDA, 2.32, 3}, /**< Propaganda skill, 2.32% increase, 3 tick wait */
  { SCMD_LOBBY, SKILL_LOBBY, 6.0, 8}, /**< Lobby skill, 10% increase, 8 tick wait     */

  { 0, 0, 0.0, 0} /**< This must be the last line */
};

ACMD(do_diplomacy) {
  // need to make this do something Zusuk :P
}

void show_happyhour(struct char_data *ch) {
  char happyexp[80], happygold[80], happyqp[80], happytreasure[80];
  int secs_left;

  if ((IS_HAPPYHOUR) || (GET_LEVEL(ch) >= LVL_GRSTAFF)) {
    if (HAPPY_TIME)
      secs_left = ((HAPPY_TIME - 1) * SECS_PER_MUD_HOUR) + next_tick;
    else
      secs_left = 0;

    sprintf(happyqp, "%s+%d%%%s to Questpoints per quest\r\n", CCYEL(ch, C_NRM), HAPPY_QP, CCNRM(ch, C_NRM));
    sprintf(happygold, "%s+%d%%%s to Gold gained per kill\r\n", CCYEL(ch, C_NRM), HAPPY_GOLD, CCNRM(ch, C_NRM));
    sprintf(happyexp, "%s+%d%%%s to Experience per kill\r\n", CCYEL(ch, C_NRM), HAPPY_EXP, CCNRM(ch, C_NRM));
    sprintf(happytreasure, "%s+%d%%%s to Treasure Drop rate\r\n", CCYEL(ch, C_NRM), HAPPY_TREASURE, CCNRM(ch, C_NRM));

    send_to_char(ch, "LuminariMUD Happy Hour!\r\n"
            "------------------\r\n"
            "%s%s%s%sTime Remaining: %s%d%s hours %s%d%s mins %s%d%s secs\r\n",
            (IS_HAPPYEXP || (GET_LEVEL(ch) >= LVL_STAFF)) ? happyexp : "",
            (IS_HAPPYGOLD || (GET_LEVEL(ch) >= LVL_STAFF)) ? happygold : "",
            (IS_HAPPYTREASURE || (GET_LEVEL(ch) >= LVL_STAFF)) ? happytreasure : "",
            (IS_HAPPYQP || (GET_LEVEL(ch) >= LVL_STAFF)) ? happyqp : "",
            CCYEL(ch, C_NRM), (secs_left / 3600), CCNRM(ch, C_NRM),
            CCYEL(ch, C_NRM), (secs_left % 3600) / 60, CCNRM(ch, C_NRM),
            CCYEL(ch, C_NRM), (secs_left % 60), CCNRM(ch, C_NRM));
  } else {
    send_to_char(ch, "Sorry, there is currently no happy hour!\r\n");
  }
}

ACMD(do_happyhour) {
  char arg[MAX_INPUT_LENGTH], val[MAX_INPUT_LENGTH];
  int num;

  if (GET_LEVEL(ch) < LVL_STAFF) {
    show_happyhour(ch);
    return;
  }

  /* Only Imms get here, so check args */
  two_arguments(argument, arg, val);

  if (is_abbrev(arg, "experience")) {
    num = MIN(MAX((atoi(val)), 0), 1000);
    HAPPY_EXP = num;
    send_to_char(ch, "Happy Hour Exp rate set to +%d%%\r\n", HAPPY_EXP);
  } else if (is_abbrev(arg, "treasure")) {
    num = MIN(MAX((atoi(val)), TREASURE_PERCENT + 1), 99 - TREASURE_PERCENT);
    HAPPY_TREASURE = num;
    send_to_char(ch, "Happy Hour Treasure drop-rate set to +%d%%\r\n",
            HAPPY_TREASURE);
  } else if ((is_abbrev(arg, "gold")) || (is_abbrev(arg, "coins"))) {
    num = MIN(MAX((atoi(val)), 0), 1000);
    HAPPY_GOLD = num;
    send_to_char(ch, "Happy Hour Gold rate set to +%d%%\r\n", HAPPY_GOLD);
  } else if ((is_abbrev(arg, "time")) || (is_abbrev(arg, "ticks"))) {
    num = MIN(MAX((atoi(val)), 0), 1000);
    if (HAPPY_TIME && !num)
      game_info("Happyhour has been stopped!");
    else if (!HAPPY_TIME && num)
      game_info("A Happyhour has started!");

    HAPPY_TIME = num;
    send_to_char(ch, "Happy Hour Time set to %d ticks (%d hours %d mins and %d secs)\r\n",
            HAPPY_TIME,
            (HAPPY_TIME * SECS_PER_MUD_HOUR) / 3600,
            ((HAPPY_TIME * SECS_PER_MUD_HOUR) % 3600) / 60,
            (HAPPY_TIME * SECS_PER_MUD_HOUR) % 60);
  } else if ((is_abbrev(arg, "qp")) || (is_abbrev(arg, "questpoints"))) {
    num = MIN(MAX((atoi(val)), 0), 1000);
    HAPPY_QP = num;
    send_to_char(ch, "Happy Hour Questpoints rate set to +%d%%\r\n", HAPPY_QP);
  } else if (is_abbrev(arg, "show")) {
    show_happyhour(ch);
  } else if (is_abbrev(arg, "default")) {
    HAPPY_EXP = 100;
    HAPPY_GOLD = 50;
    HAPPY_QP = 50;
    HAPPY_TREASURE = 20;
    HAPPY_TIME = 48;
    game_info("A Happyhour has started!");
  } else {
    send_to_char(ch,
            "Usage: %shappyhour                 %s- show usage (this info)\r\n"
            "       %shappyhour show            %s- display current settings (what mortals see)\r\n"
            "       %shappyhour time <ticks>    %s- set happyhour time and start timer\r\n"
            "       %shappyhour qp <num>        %s- set qp percentage gain\r\n"
            "       %shappyhour exp <num>       %s- set exp percentage gain\r\n"
            "       %shappyhour gold <num>      %s- set gold percentage gain\r\n"
            "       %shappyhour treasure <num>  %s- set treasure drop-rate gain\r\n"
            "       \tyhappyhour default      \tw- sets a default setting for happyhour\r\n\r\n"
            "Configure the happyhour settings and start a happyhour.\r\n"
            "Currently 1 hour IRL = %d ticks\r\n"
            "If no number is specified, 0 (off) is assumed.\r\nThe command \tyhappyhour time\tn will therefore stop the happyhour timer.\r\n",
            CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
            CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
            CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
            CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
            CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
            CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
            CCYEL(ch, C_NRM), CCNRM(ch, C_NRM),
            (3600 / SECS_PER_MUD_HOUR));
  }
}
