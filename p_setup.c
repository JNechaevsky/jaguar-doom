/* P_main.c */

#include "doomdef.h"
#include "p_local.h"

void	P_SpawnMapThing (mapthing_t *mthing);

int			numvertexes;
vertex_t	*vertexes;

int			numsegs;
seg_t		*segs;

int			numsectors;
sector_t	*sectors;

int			numsubsectors;
subsector_t	*subsectors;

int			numnodes;
node_t		*nodes;

int			numlines;
line_t		*lines;

int			numsides;
side_t		*sides;

short		*blockmaplump;			/* offsets in blockmap are from here */
short		*blockmap;
int			bmapwidth, bmapheight;	/* in mapblocks */
fixed_t		bmaporgx, bmaporgy;		/* origin of block map */
mobj_t		**blocklinks;			/* for thing chains */

byte		*rejectmatrix;			/* for fast sight rejection */

mapthing_t	deathmatchstarts[10], *deathmatch_p;
mapthing_t	playerstarts[MAXPLAYERS];

/*
=================
=
= P_LoadVertexes
=
=================
*/

void P_LoadVertexes (int lump)
{
#ifdef MARS
	numvertexes = W_LumpLength (lump) / sizeof(vertex_t);
	vertexes = (vertex_t *)(wadfileptr+BIGLONG(lumpinfo[lump].filepos));
#else
	byte		*data;
	int			i;
	mapvertex_t	*ml;
	vertex_t	*li;
	
	numvertexes = W_LumpLength (lump) / sizeof(mapvertex_t);
	vertexes = Z_Malloc (numvertexes*sizeof(vertex_t),PU_LEVEL,0);	
	data = I_TempBuffer ();	
	W_ReadLump (lump,data);
	
	
	ml = (mapvertex_t *)data;
	li = vertexes;
	for (i=0 ; i<numvertexes ; i++, li++, ml++)
	{
		li->x = LITTLESHORT(ml->x)<<FRACBITS;
		li->y = LITTLESHORT(ml->y)<<FRACBITS;
	}
#endif
}


/*
=================
=
= P_LoadSegs
=
=================
*/

void P_LoadSegs (int lump)
{
	byte		*data;
	int			i;
	mapseg_t	*ml;
	seg_t		*li;
	line_t	*ldef;
	int			linedef, side;
	
	numsegs = W_LumpLength (lump) / sizeof(mapseg_t);
	segs = Z_Malloc (numsegs*sizeof(seg_t),PU_LEVEL,0);	
	D_memset (segs, 0, numsegs*sizeof(seg_t));
	data = I_TempBuffer ();
	W_ReadLump (lump,data);
	
	ml = (mapseg_t *)data;
	li = segs;
	for (i=0 ; i<numsegs ; i++, li++, ml++)
	{
		li->v1 = &vertexes[LITTLESHORT(ml->v1)];
		li->v2 = &vertexes[LITTLESHORT(ml->v2)];
					
		li->angle = (LITTLESHORT(ml->angle))<<16;
		li->offset = (LITTLESHORT(ml->offset))<<16;
		linedef = LITTLESHORT(ml->linedef);
		ldef = &lines[linedef];
		li->linedef = ldef;
		side = LITTLESHORT(ml->side);
		li->sidedef = &sides[ldef->sidenum[side]];
		li->frontsector = sides[ldef->sidenum[side]].sector;
		if (ldef-> flags & ML_TWOSIDED)
			li->backsector = sides[ldef->sidenum[side^1]].sector;
		else
			li->backsector = 0;
			
		if (ldef->v1 == li->v1)
			ldef->fineangle = li->angle >> ANGLETOFINESHIFT;
	}
}


/*
=================
=
= P_LoadSubsectors
=
=================
*/

void P_LoadSubsectors (int lump)
{
	byte			*data;
	int				i;
	mapsubsector_t	*ms;
	subsector_t		*ss;
	
	numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);
	subsectors = Z_Malloc (numsubsectors*sizeof(subsector_t),PU_LEVEL,0);	
	data = I_TempBuffer ();
	W_ReadLump (lump,data);

	ms = (mapsubsector_t *)data;
	D_memset (subsectors,0, numsubsectors*sizeof(subsector_t));
	ss = subsectors;
	for (i=0 ; i<numsubsectors ; i++, ss++, ms++)
	{
		ss->numlines = LITTLESHORT(ms->numsegs);
		ss->firstline = LITTLESHORT(ms->firstseg);
	}
}


/*
=================
=
= P_LoadSectors
=
=================
*/

void P_LoadSectors (int lump)
{
	byte			*data;
	int				i;
	mapsector_t		*ms;
	sector_t		*ss;
			
	numsectors = W_LumpLength (lump) / sizeof(mapsector_t);
	sectors = Z_Malloc (numsectors*sizeof(sector_t),PU_LEVEL,0);	
	D_memset (sectors, 0, numsectors*sizeof(sector_t));
	data = I_TempBuffer ();
	W_ReadLump (lump,data);
	
	ms = (mapsector_t *)data;
	ss = sectors;
	for (i=0 ; i<numsectors ; i++, ss++, ms++)
	{
		ss->floorheight = LITTLESHORT(ms->floorheight)<<FRACBITS;
		ss->ceilingheight = LITTLESHORT(ms->ceilingheight)<<FRACBITS;
		ss->floorpic = R_FlatNumForName(ms->floorpic);
		if (!D_strncasecmp (ms->ceilingpic,"F_SKY1",6) )
			ss->ceilingpic = -1;
		else
		{
			ss->ceilingpic = R_FlatNumForName(ms->ceilingpic);
		}
		ss->lightlevel = LITTLESHORT(ms->lightlevel);
		ss->special = LITTLESHORT(ms->special);
		ss->tag = LITTLESHORT(ms->tag);
		ss->thinglist = NULL;
	}
}


/*
=================
=
= P_LoadNodes
=
=================
*/

void P_LoadNodes (int lump)
{
#ifdef MARS
	numnodes = W_LumpLength (lump) / sizeof(node_t);
	nodes = (node_t *)(wadfileptr+BIGLONG(lumpinfo[lump].filepos));
#else
	byte		*data;
	int			i,j,k;
	mapnode_t	*mn;
	node_t		*no;
	
	numnodes = W_LumpLength (lump) / sizeof(mapnode_t);
	nodes = Z_Malloc (numnodes*sizeof(node_t),PU_LEVEL,0);	
	data = I_TempBuffer ();
	W_ReadLump (lump,data);
	
	mn = (mapnode_t *)data;
	no = nodes;
	for (i=0 ; i<numnodes ; i++, no++, mn++)
	{
		no->x = LITTLESHORT(mn->x)<<FRACBITS;
		no->y = LITTLESHORT(mn->y)<<FRACBITS;
		no->dx = LITTLESHORT(mn->dx)<<FRACBITS;
		no->dy = LITTLESHORT(mn->dy)<<FRACBITS;
		for (j=0 ; j<2 ; j++)
		{
			no->children[j] = (unsigned short)LITTLESHORT(mn->children[j]);
			for (k=0 ; k<4 ; k++)
				no->bbox[j][k] = LITTLESHORT(mn->bbox[j][k])<<FRACBITS;
		}
	}
#endif
}



/*
=================
=
= P_LoadThings
=
=================
*/

void P_LoadThings (int lump)
{
	byte			*data;
	int				i;
	mapthing_t		*mt;
	int				numthings;
	
	data = I_TempBuffer ();
	W_ReadLump (lump,data);
	numthings = W_LumpLength (lump) / sizeof(mapthing_t);
	
	mt = (mapthing_t *)data;
	for (i=0 ; i<numthings ; i++, mt++)
	{
		mt->x = LITTLESHORT(mt->x);
		mt->y = LITTLESHORT(mt->y);
		mt->angle = LITTLESHORT(mt->angle);
		mt->type = LITTLESHORT(mt->type);
		mt->options = LITTLESHORT(mt->options);
		P_SpawnMapThing (mt);
	}
}



/*
=================
=
= P_LoadLineDefs
=
= Also counts secret lines for intermissions
=================
*/

void P_LoadLineDefs (int lump)
{
	byte			*data;
	int				i;
	maplinedef_t	*mld;
	line_t			*ld;
	vertex_t		*v1, *v2;
	
	numlines = W_LumpLength (lump) / sizeof(maplinedef_t);
	lines = Z_Malloc (numlines*sizeof(line_t),PU_LEVEL,0);	
	D_memset (lines, 0, numlines*sizeof(line_t));
	data = I_TempBuffer ();
	W_ReadLump (lump,data);
	
	mld = (maplinedef_t *)data;
	ld = lines;
	for (i=0 ; i<numlines ; i++, mld++, ld++)
	{
		ld->flags = LITTLESHORT(mld->flags);
		ld->special = LITTLESHORT(mld->special);
		ld->tag = LITTLESHORT(mld->tag);
		v1 = ld->v1 = &vertexes[LITTLESHORT(mld->v1)];
		v2 = ld->v2 = &vertexes[LITTLESHORT(mld->v2)];
		ld->dx = v2->x - v1->x;
		ld->dy = v2->y - v1->y;
		if (!ld->dx)
			ld->slopetype = ST_VERTICAL;
		else if (!ld->dy)
			ld->slopetype = ST_HORIZONTAL;
		else
		{
			if (FixedDiv (ld->dy , ld->dx) > 0)
				ld->slopetype = ST_POSITIVE;
			else
				ld->slopetype = ST_NEGATIVE;
		}
		
		if (v1->x < v2->x)
		{
			ld->bbox[BOXLEFT] = v1->x;
			ld->bbox[BOXRIGHT] = v2->x;
		}
		else
		{
			ld->bbox[BOXLEFT] = v2->x;
			ld->bbox[BOXRIGHT] = v1->x;
		}
		if (v1->y < v2->y)
		{
			ld->bbox[BOXBOTTOM] = v1->y;
			ld->bbox[BOXTOP] = v2->y;
		}
		else
		{
			ld->bbox[BOXBOTTOM] = v2->y;
			ld->bbox[BOXTOP] = v1->y;
		}
		ld->sidenum[0] = LITTLESHORT(mld->sidenum[0]);
		ld->sidenum[1] = LITTLESHORT(mld->sidenum[1]);
		if (ld->sidenum[0] != -1)
			ld->frontsector = sides[ld->sidenum[0]].sector;
		else
			ld->frontsector = 0;
		if (ld->sidenum[1] != -1)
			ld->backsector = sides[ld->sidenum[1]].sector;
		else
			ld->backsector = 0;
	}
}


/*
=================
=
= P_LoadSideDefs
=
=================
*/

void P_LoadSideDefs (int lump)
{
	byte			*data;
	int				i;
	mapsidedef_t	*msd;
	side_t			*sd;
	
	for (i=0 ; i<numtextures ; i++)
		textures[i].usecount = 0;
		
	numsides = W_LumpLength (lump) / sizeof(mapsidedef_t);
	sides = Z_Malloc (numsides*sizeof(side_t),PU_LEVEL,0);	
	D_memset (sides, 0, numsides*sizeof(side_t));
	data = I_TempBuffer ();
	W_ReadLump (lump,data);
	
	msd = (mapsidedef_t *)data;
	sd = sides;
	for (i=0 ; i<numsides ; i++, msd++, sd++)
	{
		sd->textureoffset = LITTLESHORT(msd->textureoffset)<<FRACBITS;
		sd->rowoffset = LITTLESHORT(msd->rowoffset)<<FRACBITS;
		sd->toptexture = R_TextureNumForName(msd->toptexture);
		sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
		sd->midtexture = R_TextureNumForName(msd->midtexture);
		sd->sector = &sectors[LITTLESHORT(msd->sector)];
		
		textures[sd->toptexture].usecount++;
		textures[sd->bottomtexture].usecount++;
		textures[sd->midtexture].usecount++;
	}
}



/*
=================
=
= P_LoadBlockMap
=
=================
*/

void P_LoadBlockMap (int lump)
{
	int		count;
	
#ifdef MARS
	blockmaplump = (short *)(wadfileptr+BIGLONG(lumpinfo[lump].filepos));
	blockmap = blockmaplump+4;
#else
	int		i;
	
	blockmaplump = W_CacheLumpNum (lump,PU_LEVEL);
	blockmap = blockmaplump+4;
	count = W_LumpLength (lump)/2;
	for (i=0 ; i<count ; i++)
		blockmaplump[i] = LITTLESHORT(blockmaplump[i]);
#endif
		
	bmaporgx = blockmaplump[0]<<FRACBITS;
	bmaporgy = blockmaplump[1]<<FRACBITS;
	bmapwidth = blockmaplump[2];
	bmapheight = blockmaplump[3];
	
/* clear out mobj chains */
	count = sizeof(*blocklinks)* bmapwidth*bmapheight;
	blocklinks = Z_Malloc (count,PU_LEVEL, 0);
	D_memset (blocklinks, 0, count);
}




/*
=================
=
= P_GroupLines
=
= Builds sector line lists and subsector sector numbers
= Finds block bounding boxes for sectors
=================
*/

void P_GroupLines (void)
{
	line_t		**linebuffer;
	int			i, j, total;
	sector_t	*sector;
	subsector_t	*ss;
	seg_t		*seg;
	int			block;
	line_t		*li;
	fixed_t		bbox[4];
	
/* look up sector number for each subsector */
	ss = subsectors;
	for (i=0 ; i<numsubsectors ; i++, ss++)
	{
		seg = &segs[ss->firstline];
		ss->sector = seg->sidedef->sector;
	}

/* count number of lines in each sector */
	li = lines;
	total = 0;
	for (i=0 ; i<numlines ; i++, li++)
	{
		total++;
		li->frontsector->linecount++;
		if (li->backsector && li->backsector != li->frontsector)
		{
			li->backsector->linecount++;
			total++;
		}
	}
	
/* build line tables for each sector	 */
	linebuffer = Z_Malloc (total*4, PU_LEVEL, 0);
	sector = sectors;
	for (i=0 ; i<numsectors ; i++, sector++)
	{
		M_ClearBox (bbox);
		sector->lines = linebuffer;
		li = lines;
		for (j=0 ; j<numlines ; j++, li++)
		{
			if (li->frontsector == sector || li->backsector == sector)
			{
				*linebuffer++ = li;
				M_AddToBox (bbox, li->v1->x, li->v1->y);
				M_AddToBox (bbox, li->v2->x, li->v2->y);
			}
		}
		if (linebuffer - sector->lines != sector->linecount)
			I_Error ("P_GroupLines: miscounted");
			
		/* set the degenmobj_t to the middle of the bounding box */
		sector->soundorg.x = (bbox[BOXRIGHT]+bbox[BOXLEFT])/2;
		sector->soundorg.y = (bbox[BOXTOP]+bbox[BOXBOTTOM])/2;
		
		/* adjust bounding box to map blocks */
		block = (bbox[BOXTOP]-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block >= bmapheight ? bmapheight-1 : block;
		sector->blockbox[BOXTOP]=block;

		block = (bbox[BOXBOTTOM]-bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block < 0 ? 0 : block;
		sector->blockbox[BOXBOTTOM]=block;

		block = (bbox[BOXRIGHT]-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block >= bmapwidth ? bmapwidth-1 : block;
		sector->blockbox[BOXRIGHT]=block;

		block = (bbox[BOXLEFT]-bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
		block = block < 0 ? 0 : block;
		sector->blockbox[BOXLEFT]=block;

	}
	
}

/*============================================================================= */


/*
=================
=
= P_LoadingPlaque
=
=================
*/

void P_LoadingPlaque (void)
{
	jagobj_t	*pl;

	pl = W_CacheLumpName ("loading", PU_STATIC);	
	DrawPlaque (pl);
	Z_Free (pl);
}	

/*============================================================================= */


/*
=================
=
= P_SetupLevel
=
=================
*/

void P_SetupLevel (int map, skill_t skill)
{
	int		i;
	static char	lumpname[16];
	int		lumpnum;
	mobj_t	*mobj;
	extern	int	cy;
	
	M_ClearRandom ();

	P_LoadingPlaque ();
	
D_printf ("P_SetupLevel(%i,%i)\n",map,skill);
	
	totalkills = totalitems = totalsecret = 0;
	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		players[i].killcount = players[i].secretcount 
		= players[i].itemcount = 0;
	}

Z_CheckHeap (mainzone);		
#ifndef MARS
Z_CheckHeap (refzone);
#endif

	Z_FreeTags (mainzone);
/*PrintHex (1,1,Z_FreeMemory (mainzone)); */

	P_InitThinkers ();
	
/* */
/* look for a regular (development) map first */
/* */
	lumpname[0] = 'M';
	lumpname[1] = 'A';
	lumpname[2] = 'P';
	lumpname[3] = '0' + map/10;
	lumpname[4] = '0' + map%10;
	lumpname[5] = 0;
	
	lumpnum = W_GetNumForName (lumpname);
	
/* note: most of this ordering is important	 */
	P_LoadBlockMap (lumpnum+ML_BLOCKMAP);
	P_LoadVertexes (lumpnum+ML_VERTEXES);
	P_LoadSectors (lumpnum+ML_SECTORS);
	P_LoadSideDefs (lumpnum+ML_SIDEDEFS);
	P_LoadLineDefs (lumpnum+ML_LINEDEFS);
	P_LoadSubsectors (lumpnum+ML_SSECTORS);
	P_LoadNodes (lumpnum+ML_NODES);
	P_LoadSegs (lumpnum+ML_SEGS);
	
#ifdef MARS
	rejectmatrix = (byte *)(wadfileptr+BIGLONG(lumpinfo[lumpnum+ML_REJECT].filepos));
#else
	rejectmatrix = W_CacheLumpNum (lumpnum+ML_REJECT,PU_LEVEL);
#endif

	P_GroupLines ();

	deathmatch_p = deathmatchstarts;
	P_LoadThings (lumpnum+ML_THINGS);
	
/* */
/* if deathmatch, randomly spawn the active players */
/* */
	if (netgame == gt_deathmatch)
	{
		for (i=0 ; i<MAXPLAYERS ; i++)
			if (playeringame[i])
			{	/* must give a player spot before deathmatchspawn */
				mobj = P_SpawnMobj (deathmatchstarts[0].x<<16
				,deathmatchstarts[0].y<<16,0, MT_PLAYER);
				players[i].mo = mobj;
				G_DeathMatchSpawnPlayer (i);
				P_RemoveMobj (mobj);
			}
	}
	
/* set up world state */
	P_SpawnSpecials ();
	ST_InitEveryLevel ();
	
/*printf ("free memory: 0x%x\n", Z_FreeMemory(mainzone)); */

	cy = 4;

#ifdef JAGUAR
{
extern byte *debugscreen;
	D_memset (debugscreen,0,32*224);
	
}
#endif

	iquehead = iquetail = 0;
	gamepaused = false;
}


/*
=================
=
= P_Init
=
=================
*/

void P_Init (void)
{
	P_InitSwitchList ();
	P_InitPicAnims ();
#ifndef MARS
	pausepic = W_CacheLumpName ("PAUSED",PU_STATIC);
#endif
}



