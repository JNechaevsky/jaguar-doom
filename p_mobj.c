/* P_mobj.c */

#include "doomdef.h"
#include "p_local.h"
#include "sounds.h"

void G_PlayerReborn (int player);

#define		ITEMQUESIZE	32
mapthing_t	itemrespawnque[ITEMQUESIZE];
int			itemrespawntime[ITEMQUESIZE];
int			iquehead, iquetail;

/*
===============
=
= P_RemoveMobj
=
===============
*/

void P_RemoveMobj (mobj_t *mobj)
{
#ifndef MARS
	int		spot;
	
/* add to the respawnque for altdeath mode */

	if ((mobj->flags & MF_SPECIAL) &&
		!(mobj->flags & MF_DROPPED) &&
		(mobj->type != MT_INV) &&
		(mobj->type != MT_INS))
	{
		spot = iquehead&(ITEMQUESIZE-1);

		itemrespawnque[spot].x = mobj->spawnx;
		itemrespawnque[spot].y = mobj->spawny;
		itemrespawnque[spot].type = mobj->spawntype;
		itemrespawnque[spot].angle = mobj->spawnangle;
		itemrespawntime[spot] = ticon;
		iquehead++;
	}

#endif

/* unlink from sector and block lists */
	P_UnsetThingPosition (mobj);

/* unlink from mobj list */
	mobj->next->prev = mobj->prev;
	mobj->prev->next = mobj->next;
	Z_Free (mobj);
}


/*
===============
=
= P_RespawnSpecials
=
===============
*/

void P_RespawnSpecials (void)
{
#ifndef MARS
	fixed_t         x,y,z; 
	subsector_t 	*ss; 
	mobj_t			*mo;
	mapthing_t		*mthing;
	int				i;
	int				spot;
	
	if (netgame != gt_deathmatch)
		return;
		
	if (iquehead == iquetail)
		return;			/* nothing left to respawn */

	if (iquehead - iquetail > ITEMQUESIZE)
		iquetail = iquehead - ITEMQUESIZE;	/* loose some off the que */
		
	spot = iquetail&(ITEMQUESIZE-1);
	
	if (ticon - itemrespawntime[spot] < 30*15)
		return;			/* wait at least 30 seconds */

	mthing = &itemrespawnque[spot];
	
	x = mthing->x << FRACBITS; 
	y = mthing->y << FRACBITS; 
	  
/* spawn a teleport fog at the new spot */
	ss = R_PointInSubsector (x,y); 
	mo = P_SpawnMobj (x, y, ss->sector->floorheight , MT_IFOG); 

	S_StartSound (mo, sfx_itmbk);

/* find which type to spawn */
	for (i=0 ; i< NUMMOBJTYPES ; i++)
		if (mthing->type == mobjinfo[i].doomednum)
			break;

/* spawn it */
	if (mobjinfo[i].flags & MF_SPAWNCEILING)
		z = ONCEILINGZ;
	else
		z = ONFLOORZ;
	mo = P_SpawnMobj (x,y,z, i);
	mo->angle = ANG45 * (mthing->angle/45);
	mo->spawnx = mthing->x;
	mo->spawny = mthing->y;
	mo->spawntype = mthing->type;
	mo->spawnangle = mthing->angle;
	
/* pull it from the que */
	iquetail++;
#endif
}
	

/*
================
=
= P_SetMobjState
=
= Returns true if the mobj is still present
================
*/

boolean P_SetMobjState (mobj_t *mobj, statenum_t state)
{
	state_t	*st;
	
	if (state == S_NULL)
	{
		mobj->state = S_NULL;
		P_RemoveMobj (mobj);
		return false;
	}
	
	st = &states[state];

	mobj->state = st;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame;

	if (st->action)		/* call action functions when the state is set */
		st->action (mobj);	

	mobj->latecall = NULL;	/* make sure it doesn't come back to life... */
	
	return true;
}

/* 
=================== 
= 
= P_ExplodeMissile  
=
=================== 
*/ 

void P_ExplodeMissile (mobj_t *mo)
{
	mo->momx = mo->momy = mo->momz = 0;
	P_SetMobjState (mo, mobjinfo[mo->type].deathstate);
	mo->tics -= P_Random()&1;
	if (mo->tics < 1)
		mo->tics = 1;
	mo->flags &= ~MF_MISSILE;
	if (mo->info->deathsound)
		S_StartSound (mo, mo->info->deathsound);
}


/*
===============
=
= P_SpawnMobj
=
===============
*/

int zonetics;

mobj_t *P_SpawnMobj (fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
	mobj_t		*mobj;
	state_t		*st;
	mobjinfo_t	*info;
	
	mobj = Z_Malloc (sizeof(*mobj), PU_LEVEL, NULL);

	D_memset (mobj, 0, sizeof (*mobj));
	info = &mobjinfo[type];
	
	mobj->type = type;
	mobj->info = info;
	mobj->x = x;
	mobj->y = y;
	mobj->radius = info->radius;
	mobj->height = info->height;
	mobj->flags = info->flags;
	mobj->health = info->spawnhealth;
	mobj->reactiontime = info->reactiontime;
	
/* do not set the state with P_SetMobjState, because action routines can't */
/* be called yet */
	st = &states[info->spawnstate];

	mobj->state = st;
	mobj->tics = st->tics;
	mobj->sprite = st->sprite;
	mobj->frame = st->frame;

/* set subsector and/or block links */
	P_SetThingPosition (mobj);
	
	mobj->floorz = mobj->subsector->sector->floorheight;
	mobj->ceilingz = mobj->subsector->sector->ceilingheight;
	if (z == ONFLOORZ)
		mobj->z = mobj->floorz;
	else if (z == ONCEILINGZ)
		mobj->z = mobj->ceilingz - mobj->info->height;
	else 
		mobj->z = z;
	
/* */
/* link into the mobj list */
/* */
	mobjhead.prev->next = mobj;
	mobj->next = &mobjhead;
	mobj->prev = mobjhead.prev;
	mobjhead.prev = mobj;

	return mobj;
}


/*============================================================================= */


/*
============
=
= P_SpawnPlayer
=
= Called when a player is spawned on the level 
= Most of the player structure stays unchanged between levels
============
*/

void P_SpawnPlayer (mapthing_t *mthing)
{
	player_t	*p;
	fixed_t		x,y,z;
	mobj_t		*mobj;
	int	i;

	if (!playeringame[mthing->type-1])
		return;						/* not playing */
		
	p = &players[mthing->type-1];

	if (p->playerstate == PST_REBORN)
		G_PlayerReborn (mthing->type-1);

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;
#if 0
if (mthing->type==1)
{
x = 0xffb00000;
y = 0xff500000;
}
#endif
	z = ONFLOORZ;
	mobj = P_SpawnMobj (x,y,z, MT_PLAYER);
	
	mobj->angle = ANG45 * (mthing->angle/45);

	mobj->player = p;
	mobj->health = p->health;
	p->mo = mobj;
	p->playerstate = PST_LIVE;	
	p->refire = 0;
	p->message = NULL;
	p->damagecount = 0;
	p->bonuscount = 0;
	p->extralight = 0;
	p->fixedcolormap = 0;
	p->viewheight = VIEWHEIGHT;
	P_SetupPsprites (p);		/* setup gun psprite	 */
	
	if (netgame == gt_deathmatch)
		for (i=0 ; i<NUMCARDS ; i++)
			p->cards[i] = true;		/* give all cards in death match mode			 */
}



/*
=================
=
= P_SpawnMapThing
=
= The fields of the mapthing should already be in host byte order
==================
*/

void P_SpawnMapThing (mapthing_t *mthing)
{
	int			i, bit;
	mobj_t		*mobj;
	fixed_t		x,y,z;
		
/* count deathmatch start positions */
	if (mthing->type == 11)
	{
		if (deathmatch_p < &deathmatchstarts[10])
			D_memcpy (deathmatch_p, mthing, sizeof(*mthing));
		deathmatch_p++;
		return;
	}
	
/* check for players specially */

#if 0
if (mthing->type > 4)
return;	/*DEBUG */
#endif

	if (mthing->type <= 4)
	{
		/* save spots for respawning in network games */
		if (mthing->type <= MAXPLAYERS)
		{
			playerstarts[mthing->type-1] = *mthing;
			if (netgame != gt_deathmatch)
				P_SpawnPlayer (mthing);
		}
		return;
	}

/* check for apropriate skill level */
	if ( (netgame != gt_deathmatch) && (mthing->options & 16) )
		return;
		
	if (gameskill == sk_baby)
		bit = 1;
	else if (gameskill == sk_nightmare)
		bit = 4;
	else
		bit = 1<<(gameskill-1);
	if (!(mthing->options & bit) )
		return;
	
#ifdef MARS
/* hack player corpses into something else, because player graphics */
/* aren't included */
	if (mthing->type == 10 || mthing->type == 12)	/* player corpse */
		mthing->type = 18;		/* possessed human corpse */
#endif


/* find which type to spawn */
	for (i=0 ; i< NUMMOBJTYPES ; i++)
		if (mthing->type == mobjinfo[i].doomednum)
			break;
	
	if (i==NUMMOBJTYPES)
		I_Error ("P_SpawnMapThing: Unknown type %i at (%i, %i)",mthing->type
		, mthing->x, mthing->y);


		
/* don't spawn keycards and players in deathmatch */
	if (netgame == gt_deathmatch && mobjinfo[i].flags & (MF_NOTDMATCH|MF_COUNTKILL) )
		return;
		
	
/* spawn it */

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;
	if (mobjinfo[i].flags & MF_SPAWNCEILING)
		z = ONCEILINGZ;
	else
		z = ONFLOORZ;
	mobj = P_SpawnMobj (x,y,z, i);
	if (mobj->tics > 0)
		mobj->tics = 1 + (P_Random () % mobj->tics);
	if (mobj->flags & MF_COUNTKILL)
		totalkills++;
	if (mobj->flags & MF_COUNTITEM)
		totalitems++;
		
	mobj->angle = ANG45 * (mthing->angle/45);
	if (mthing->options & MTF_AMBUSH)
		mobj->flags |= MF_AMBUSH;
		
	mobj->spawnx = mthing->x;
	mobj->spawny = mthing->y;
	mobj->spawntype = mthing->type;
	mobj->spawnangle = mthing->angle;
}


/*
===============================================================================

						GAME SPAWN FUNCTIONS

===============================================================================
*/

/*
================
=
= P_SpawnPuff
=
================
*/

extern fixed_t attackrange;

void P_SpawnPuff (fixed_t x, fixed_t y, fixed_t z)
{
	mobj_t	*th;
	
	z += ((P_Random()-P_Random())<<10);
	th = P_SpawnMobj (x,y,z, MT_PUFF);
	th->momz = FRACUNIT;
	th->tics -= P_Random()&1;
	if (th->tics < 1)
		th->tics = 1;
		
/* don't make punches spark on the wall */
	if (attackrange == MELEERANGE)
		P_SetMobjState (th, S_PUFF3);
}


/*
================
=
= P_SpawnBlood
=
================
*/

void P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, int damage)
{
	mobj_t	*th;
	
	z += ((P_Random()-P_Random())<<10);
	th = P_SpawnMobj (x,y,z, MT_BLOOD);
	th->momz = FRACUNIT*2;
	th->tics -= P_Random()&1;
	if (th->tics<1)
		th->tics = 1;
	if (damage <= 12 && damage >= 9)
		P_SetMobjState (th,S_BLOOD2);
	else if (damage < 9)
		P_SetMobjState (th,S_BLOOD3);
}

/*
================
=
= P_CheckMissileSpawn
=
= Moves the missile forward a bit and possibly explodes it right there
=
================
*/

void P_CheckMissileSpawn (mobj_t *th)
{
	th->x += (th->momx>>1);
	th->y += (th->momy>>1);	/* move a little forward so an angle can */
							/* be computed if it immediately explodes */
	th->z += (th->momz>>1);
	if (!P_TryMove (th, th->x, th->y))
		P_ExplodeMissile (th);
}

/*
================
=
= P_SpawnMissile
=
================
*/

void P_SpawnMissile (mobj_t *source, mobj_t *dest, mobjtype_t type)
{
	mobj_t		*th;
	angle_t		an;
	int			dist;
	int			speed;
	
	th = P_SpawnMobj (source->x,source->y, source->z + 4*8*FRACUNIT, type);
	if (th->info->seesound)
		S_StartSound (source, th->info->seesound);
	th->target = source;		/* where it came from */
	an = R_PointToAngle2 (source->x, source->y, dest->x, dest->y);	
	th->angle = an;
	an >>= ANGLETOFINESHIFT;
	speed = th->info->speed >> 16;
	th->momx = speed * finecosine[an];
	th->momy = speed * finesine[an];
	
	dist = P_AproxDistance (dest->x - source->x, dest->y - source->y);
	dist = dist / th->info->speed;
	if (dist < 1)
		dist = 1;
	th->momz = (dest->z - source->z) / dist;
	P_CheckMissileSpawn (th);
}


/*
================
=
= P_SpawnPlayerMissile
=
= Tries to aim at a nearby monster
================
*/

void P_SpawnPlayerMissile (mobj_t *source, mobjtype_t type)
{
	mobj_t			*th;
	angle_t			an;
	fixed_t			x,y,z, slope;
	int				speed;
			
/* */
/* see which target is to be aimed at */
/* */
	an = source->angle;
	slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
	if (!linetarget)
	{
		an += 1<<26;
		slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
		if (!linetarget)
		{
			an -= 2<<26;
			slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
		}
		if (!linetarget)
		{
			an = source->angle;
			slope = 0;
		}
	}
	
	x = source->x;
	y = source->y;
	z = source->z + 4*8*FRACUNIT;
	
	th = P_SpawnMobj (x,y,z, type);
	if (th->info->seesound)
		S_StartSound (source, th->info->seesound);
	th->target = source;
	th->angle = an;
	
	speed = th->info->speed >> 16;
	
	th->momx = speed * finecosine[an>>ANGLETOFINESHIFT];
	th->momy = speed * finesine[an>>ANGLETOFINESHIFT];
	th->momz = speed * slope;

	P_CheckMissileSpawn (th);
}

