/* GemRB - Infinity Engine Emulator
 * Copyright (C) 2003 The GemRB Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Header: /data/gemrb/cvs2svn/gemrb/gemrb/gemrb/plugins/AREImporter/AREImp.cpp,v 1.67 2004/08/22 22:09:54 avenger_teambg Exp $
 *
 */

#include "../../includes/win32def.h"
#include "AREImp.h"
#include "../Core/TileMapMgr.h"
#include "../Core/AnimationMgr.h"
#include "../Core/Interface.h"
#include "../Core/ActorMgr.h"
#include "../Core/FileStream.h"
#include "../Core/ImageMgr.h"
#include "../Core/Ambient.h"

#define DEF_OPEN   0
#define DEF_CLOSE  1
#define DEF_HOPEN  2
#define DEF_HCLOSE 3

#define DEF_COUNT 4

#define DOOR_HIDDEN 128

static char Sounds[DEF_COUNT][9] = {
	{-1},
};

AREImp::AREImp(void)
{
	autoFree = false;
	str = NULL;
	if (Sounds[0][0] == -1) {
		memset( Sounds, 0, sizeof( Sounds ) );
		int SoundTable = core->LoadTable( "defsound" );
		TableMgr* at = core->GetTable( SoundTable );
		if (at) {
			for (int i = 0; i < DEF_COUNT; i++) {
				strncpy( Sounds[i], at->QueryField( i, 0 ), 8 );
				if(Sounds[i][0]=='*') {
					Sounds[i][0]=0;
				}
			}
		}
		core->DelTable( SoundTable );
	}
}

AREImp::~AREImp(void)
{
	if (autoFree && str) {
		delete( str );
	}
}

//this is the same as the function in the creature, you might want to rationalize it
CREItem* AREImp::GetItem()
{
	CREItem *itm = new CREItem();

	str->Read( itm->ItemResRef, 8 );
	str->Read( &itm->Unknown08, 2 );
	str->Read( &itm->Usages[0], 2 );
	str->Read( &itm->Usages[1], 2 );
	str->Read( &itm->Usages[2], 2 );
	str->Read( &itm->Flags, 4 );

	return itm;
}

bool AREImp::Open(DataStream* stream, bool autoFree)
{
	if (stream == NULL) {
		return false;
	}
	if (this->autoFree && str) {
		delete( str );
	}
	str = stream;
	this->autoFree = autoFree;
	char Signature[8];
	str->Read( Signature, 8 );
	int bigheader;
	if (strncmp( Signature, "AREAV1.0", 8 ) != 0) {
		if (strncmp( Signature, "AREAV9.1", 8 ) != 0) {
			return false;
		} else {
			bigheader = 16;
		}
	} else {
		bigheader = 0;
	}
	//TEST VERSION: SKIPPING VALUES
	str->Read( WEDResRef, 8 );
	str->Read( &LastSave, 4);
	str->Read( &AreaFlags, 4);
	str->Seek( 0x48 + bigheader, GEM_STREAM_START );
	str->Read( &AreaType, 2);
	str->Read( &WRain, 2);
	str->Read( &WSnow, 2);
	str->Read( &WFog, 2);
	str->Read( &WLightning, 2);
	str->Read( &WUnknown, 2);
	str->Read( &ActorOffset, 4 );
	str->Read( &ActorCount, 2 );
	str->Seek( 0x5A + bigheader, GEM_STREAM_START );
	str->Read( &InfoPointsCount, 2 );
	str->Read( &InfoPointsOffset, 4 );
	str->Seek( 0x68 + bigheader, GEM_STREAM_START );
	str->Read( &EntrancesOffset, 4 );
	str->Read( &EntrancesCount, 4 );
	str->Read( &ContainersOffset, 4 );
	str->Read( &ContainersCount, 2 );
	str->Read( &ItemsCount, 2 );
	str->Read( &ItemsOffset, 4 );
	str->Read( &VerticesOffset, 4 );
	str->Read( &VerticesCount, 2 );
	str->Read( &AmbiCount, 2 );
	str->Read( &AmbiOffset, 4 );
	str->Read( &VariablesOffset, 4 );
	str->Read( &VariablesCount, 4 );
	ieDword tmp;
	str->Read( &tmp, 4 );
	str->Read( Script, 8 );
	Script[8] = 0;
	str->Seek( 0xA4 + bigheader, GEM_STREAM_START );
	str->Read( &DoorsCount, 4 );
	str->Read( &DoorsOffset, 4 );
	str->Read( &AnimCount, 4 );
	str->Read( &AnimOffset, 4 );
	str->Seek( 8, GEM_CURRENT_POS ); //skipping some
	str->Read( &SongHeader, 4 );
	str->Read( &RestHeader, 4 );
	return true;
}

Map* AREImp::GetMap(const char *ResRef)
{
	unsigned int i,x;

	Map* map = new Map();
	map->AreaFlags=AreaFlags;
	map->AreaType=AreaType;

	//we have to set this here because the actors will receive their
	//current area setting here
	strncpy(map->scriptName, ResRef, 8);
	map->scriptName[8]=0;

	if (!core->IsAvailable( IE_WED_CLASS_ID )) {
		printf( "[AREImporter]: No Tile Map Manager Available.\n" );
		return false;
	}
	TileMapMgr* tmm = ( TileMapMgr* ) core->GetInterface( IE_WED_CLASS_ID );
	DataStream* wedfile = core->GetResourceMgr()->GetResource( WEDResRef, IE_WED_CLASS_ID );
	tmm->Open( wedfile );
	TileMap* tm = tmm->GetTileMap();

	map->Scripts[0] = new GameScript( Script, IE_SCRIPT_AREA );
	map->MySelf = map;
	if (map->Scripts[0]) {
		map->Scripts[0]->MySelf = map;
	}

	char TmpResRef[9];
	strcpy( TmpResRef, WEDResRef );
	strcat( TmpResRef, "LM" );

	ImageMgr* lm = ( ImageMgr* ) core->GetInterface( IE_BMP_CLASS_ID );
	DataStream* lmstr = core->GetResourceMgr()->GetResource( TmpResRef, IE_BMP_CLASS_ID );
	lm->Open( lmstr, true );

	strcpy( TmpResRef, WEDResRef );
	strcat( TmpResRef, "SR" );

	ImageMgr* sr = ( ImageMgr* ) core->GetInterface( IE_BMP_CLASS_ID );
	DataStream* srstr = core->GetResourceMgr()->GetResource( TmpResRef, IE_BMP_CLASS_ID );
	sr->Open( srstr, true );

	// Small map for MapControl
	ImageMgr* sm = ( ImageMgr* ) core->GetInterface( IE_MOS_CLASS_ID );
	DataStream* smstr = core->GetResourceMgr()->GetResource( WEDResRef, IE_MOS_CLASS_ID );
	sm->Open( smstr, true );


	str->Seek( SongHeader, GEM_STREAM_START );
	//5 is the number of song indices
	for (i = 0; i < 5; i++) {
		str->Read( map->SongHeader.SongList + i, 4 );
	}

	str->Seek( RestHeader, GEM_STREAM_START );
	for (i = 0; i < 10; i++) {
		str->Read( map->RestHeader.Strref + i, 4 );
	}
	for (i = 0; i < 10; i++) {
		str->Read( map->RestHeader.Creature + i, 8 );
	}
	str->Read( &map->RestHeader.CreatureNum, 2 );
	str->Seek( 14, GEM_CURRENT_POS );
	str->Read( &map->RestHeader.DayChance, 2 );
	str->Read( &map->RestHeader.NightChance, 2 );

	printf( "Loading doors\n" );
	//Loading Doors
	for (i = 0; i < DoorsCount; i++) {
		str->Seek( DoorsOffset + ( i * 0xc8 ), GEM_STREAM_START );
		int count;
		ieDword Flags, OpenFirstVertex, ClosedFirstVertex;
		ieWord OpenVerticesCount, ClosedVerticesCount;
		char LongName[33], ShortName[9];
		ieWord minX, maxX, minY, maxY;
		ieDword cursor;
		Region BBClosed, BBOpen;
		str->Read( LongName, 32 );
		LongName[32] = 0;
		str->Read( ShortName, 8 );
		ShortName[8] = 0;
		str->Read( &Flags, 4 );
		str->Read( &OpenFirstVertex, 4 );
		str->Read( &OpenVerticesCount, 2 );
		str->Read( &ClosedVerticesCount, 2 );
		str->Read( &ClosedFirstVertex, 4 );
		str->Read( &minX, 2 );
		str->Read( &minY, 2 );
		str->Read( &maxX, 2 );
		str->Read( &maxY, 2 );
		BBOpen.x = minX;
		BBOpen.y = minY;
		BBOpen.w = maxX - minX;
		BBOpen.h = maxY - minY;
		str->Read( &minX, 2 );
		str->Read( &minY, 2 );
		str->Read( &maxX, 2 );
		str->Read( &maxY, 2 );
		BBClosed.x = minX;
		BBClosed.y = minY;
		BBClosed.w = maxX - minX;
		BBClosed.h = maxY - minY;
		str->Seek( 0x10, GEM_CURRENT_POS );
		char OpenResRef[9], CloseResRef[9];
		str->Read( OpenResRef, 8 );
		OpenResRef[8] = 0;
		str->Read( CloseResRef, 8 );
		CloseResRef[8] = 0;
		str->Read( &cursor, 4 );
		str->Seek( 36, GEM_CURRENT_POS );
		Point toOpen[2];
		str->Read( &toOpen[0].x, 2 );
		str->Read( &toOpen[0].y, 2 );
		str->Read( &toOpen[1].x, 2 );
		str->Read( &toOpen[1].y, 2 );
		//Reading Open Polygon
		str->Seek( VerticesOffset + ( OpenFirstVertex * 4 ), GEM_STREAM_START );
		Point* points = ( Point* )
			malloc( OpenVerticesCount*sizeof( Point ) );
		for (x = 0; x < OpenVerticesCount; x++) {
			str->Read( &points[x].x, 2 );
			str->Read( &points[x].y, 2 );
		}
		Gem_Polygon* open = new Gem_Polygon( points, OpenVerticesCount, &BBOpen );
//		open->BBox = BBOpen;
		free( points );
		//Reading Closed Polygon
		str->Seek( VerticesOffset + ( ClosedFirstVertex * 4 ),
				GEM_STREAM_START );
		points = ( Point * ) malloc( ClosedVerticesCount * sizeof( Point ) );
		for (x = 0; x < ClosedVerticesCount; x++) {
			str->Read( &points[x].x, 2 );
			str->Read( &points[x].y, 2 );
		}
		Gem_Polygon* closed = new Gem_Polygon( points, ClosedVerticesCount, &BBClosed );
//		closed->BBox = BBClosed;
		free( points );
		//Getting Door Information from the WED File
		bool BaseClosed;
		unsigned short * indices = tmm->GetDoorIndices( ShortName, &count, BaseClosed );
		Door* door;
		door = tm->AddDoor( ShortName, Flags, BaseClosed,
					indices, count, open, closed );
		door->Cursor = cursor;
		door->toOpen[0] = toOpen[0];
		door->toOpen[1] = toOpen[1];
		//Leave the default sound untouched
		if (OpenResRef[0])
			memcpy( door->OpenSound, OpenResRef, 9 );
		else {
			if (Flags & DOOR_HIDDEN)
				memcpy( door->OpenSound, Sounds[DEF_HOPEN], 9 );
			else
				memcpy( door->OpenSound, Sounds[DEF_OPEN], 9 );
		}
		if (CloseResRef[0])
			memcpy( door->CloseSound, CloseResRef, 9 );
		else {
			if (Flags & DOOR_HIDDEN)
				memcpy( door->CloseSound, Sounds[DEF_HCLOSE], 9 );
			else
				memcpy( door->CloseSound, Sounds[DEF_CLOSE], 9 );
		}
	}
	printf( "Loading containers\n" );
	//Loading Containers
	for (i = 0; i < ContainersCount; i++) {
		str->Seek( ContainersOffset + ( i * 0xC0 ), GEM_STREAM_START );
		ieWord Type, LockDiff, Locked, Unknown;
		ieWord TrapDetDiff, TrapRemDiff, Trapped, TrapDetected;
		ieDword ItemIndex, ItemCount;
		char Name[33];
		Point p;
		str->Read( Name, 32 );
		Name[32] = 0;
		str->Read( &p.x, 2 );
		str->Read( &p.y, 2 );
		str->Read( &Type, 2 );
		str->Read( &LockDiff, 2 );
		str->Read( &Locked, 2 );
		str->Read( &Unknown, 2 );
		str->Read( &TrapDetDiff, 2 );
		str->Read( &TrapRemDiff, 2 );
		str->Read( &Trapped, 2 );
		str->Read( &TrapDetected, 2 );
		str->Seek( 4, GEM_CURRENT_POS );
		Region bbox;
		str->Read( &bbox.x, 2 );
		str->Read( &bbox.y, 2 );
		str->Read( &bbox.w, 2 );
		str->Read( &bbox.h, 2 );
		bbox.w -= bbox.x;
		bbox.h -= bbox.y;
		str->Read( &ItemIndex, 4 );
		str->Read( &ItemCount, 4 );
		str->Read( Script, 8 );
		Script[8]=0;
		ieDword firstIndex, vertCount;
		str->Read( &firstIndex, 4 );
		str->Read( &vertCount, 4 );
		str->Seek( VerticesOffset + ( firstIndex * 4 ), GEM_STREAM_START );
		Point* points = ( Point* ) malloc( vertCount*sizeof( Point ) );
		for (unsigned int x = 0; x < vertCount; x++) {
			str->Read( &points[x].x, 2 );
			str->Read( &points[x].y, 2 );
		}
		Gem_Polygon* poly = new Gem_Polygon( points, vertCount, &bbox );
		free( points );
//		poly->BBox = bbox;
		Container* c = tm->AddContainer( Name, Type, poly );
		c->LockDifficulty = LockDiff;
		c->Locked = Locked;
		c->TrapDetectionDiff = TrapDetDiff;
		c->TrapRemovalDiff = TrapRemDiff;
		c->Trapped = Trapped;
		c->TrapDetected = TrapDetected;
		//reading items into a container
		str->Seek( ItemsOffset+( ItemIndex * 0x14 ), GEM_STREAM_START);
		while(ItemCount--) {
			c->inventory.AddItem( GetItem());
		}
		if (Script[0] != 0) {
			c->Scripts[0] = new GameScript( Script, IE_SCRIPT_TRIGGER );
			c->Scripts[0]->MySelf = c;
		} else
			c->Scripts[0] = NULL;
	}
	printf( "Loading regions\n" );
	//Loading InfoPoints
	for (i = 0; i < InfoPointsCount; i++) {
		str->Seek( InfoPointsOffset + ( i * 0xC4 ), GEM_STREAM_START );
		ieWord Type, VertexCount;
		ieDword FirstVertex, Cursor, Flags;
		ieWord TrapDetDiff, TrapRemDiff, Trapped, TrapDetected;
		ieWord LaunchX, LaunchY;
		char Name[33], Script[9], Key[9], Destination[9], Entrance[33];
		str->Read( Name, 32 );
		Name[32] = 0;
		str->Read( &Type, 2 );
		Region bbox;
		str->Read( &bbox.x, 2 );
		str->Read( &bbox.y, 2 );
		str->Read( &bbox.w, 2 );
		str->Read( &bbox.h, 2 );
		bbox.w -= bbox.x;
		bbox.h -= bbox.y;
		str->Read( &VertexCount, 2 );
		str->Read( &FirstVertex, 4 );
		str->Seek( 4, GEM_CURRENT_POS );
		str->Read( &Cursor, 4 );
		str->Read( Destination, 8 );
		Destination[8] = 0;
		str->Read( Entrance, 32 );
		Entrance[32] = 0;
		str->Read( &Flags, 4 );
		ieStrRef StrRef;
		str->Read( &StrRef, 4 );
		str->Read( &TrapDetDiff, 2 );
		str->Read( &TrapRemDiff, 2 );
		str->Read( &Trapped, 2 );
		str->Read( &TrapDetected, 2 );
		str->Read( &LaunchX, 2 );
		str->Read( &LaunchY, 2 );
		str->Read( Key, 8 );
		Key[8] = 0;
		//don't even bother reading the script if it isn't trapped
		if(Trapped || Type) {
			str->Read( Script, 8 );
			Script[8] = 0;
		}
		else {
			Script[0] = 0;
		}
		char* string = core->GetString( StrRef );
		str->Seek( VerticesOffset + ( FirstVertex * 4 ), GEM_STREAM_START );
		Point* points = ( Point* ) malloc( VertexCount*sizeof( Point ) );
		for (x = 0; x < VertexCount; x++) {
			str->Read( &points[x].x, 2 );
			str->Read( &points[x].y, 2 );
		}
		Gem_Polygon* poly = new Gem_Polygon( points, VertexCount, &bbox);
		free( points );
//		poly->BBox = bbox;
		InfoPoint* ip = tm->AddInfoPoint( Name, Type, poly );
		ip->TrapDetectionDifficulty = TrapDetDiff;
		ip->TrapRemovalDifficulty = TrapRemDiff;
		//we don't need this flag, because the script is loaded
		//only if it exists
		ip->TrapDetected = TrapDetected;
		ip->TrapLaunchX = LaunchX;
		ip->TrapLaunchY = LaunchY;
		ip->Cursor = Cursor;
		ip->overHeadText = string;
		ip->textDisplaying = 0;
		ip->timeStartDisplaying = 0;
		ip->XPos = bbox.x + ( bbox.w / 2 );
		ip->YPos = bbox.y + ( bbox.h / 2 );
		ip->Flags = Flags;
		strcpy( ip->Destination, Destination );
		strcpy( ip->EntranceName, Entrance );
		//ip->triggered = false;
		//strcpy(ip->Script, Script);
		if (Script[0] != 0) {
			ip->Scripts[0] = new GameScript( Script, IE_SCRIPT_TRIGGER );
			ip->Scripts[0]->MySelf = ip;
		} else
			ip->Scripts[0] = NULL;
	}
	//we need this so we can filter global actors
	Game *game=core->GetGame();
	printf( "Loading actors\n" );
	//Loading Actors
	str->Seek( ActorOffset, GEM_STREAM_START );
	if (!core->IsAvailable( IE_CRE_CLASS_ID )) {
		printf( "[AREImporter]: No Actor Manager Available, skipping actors\n" );
	} else {
		ActorMgr* actmgr = ( ActorMgr* ) core->GetInterface( IE_CRE_CLASS_ID );
		for (i = 0; i < ActorCount; i++) {
			char DefaultName[33];
			char CreResRef[9];
			ieDword TalkCount;
			ieDword Orientation, Schedule;
			ieWord XPos, YPos, XDes, YDes;
			str->Read( DefaultName, 32);
			DefaultName[32]=0;
			str->Read( &XPos, 2 );
			str->Read( &YPos, 2 );
			str->Read( &XDes, 2 );
			str->Read( &YDes, 2 );
			str->Seek( 12, GEM_CURRENT_POS );
			str->Read( &Orientation, 4 );
			str->Seek( 8, GEM_CURRENT_POS );
			str->Read( &Schedule, 4 );
			str->Read( &TalkCount, 4 );
			str->Seek( 56, GEM_CURRENT_POS );
			str->Read( CreResRef, 8 );
			CreResRef[8] = 0;
			DataStream* crefile;
			ieDword CreOffset, CreSize;
			str->Read( &CreOffset, 4 );
			str->Read( &CreSize, 4 );
			str->Seek( 128, GEM_CURRENT_POS );
			if (CreOffset != 0) {
				char cpath[_MAX_PATH];
				strcpy( cpath, core->GamePath );
				strcat( cpath, str->filename );
				_FILE* str = _fopen( cpath, "rb" );
				FileStream* fs = new FileStream();
				fs->Open( str, CreOffset, CreSize, true );
				crefile = fs;
			} else {
				crefile = core->GetResourceMgr()->GetResource( CreResRef, IE_CRE_CLASS_ID );
			}
			actmgr->Open( crefile, true );
			Actor* ab = actmgr->GetActor();
			if(!ab)
				continue;
			ab->XPos = XPos;
			ab->YPos = YPos;
			ab->XDes = XDes;
			ab->YDes = YDes;
			//copying the area name into the actor
			strcpy(ab->Area, map->scriptName);
			//copying the scripting name into the actor
			//this hack allows iwd starting cutscene to work
			if(stricmp(ab->scriptName,"none")==0) {
				ab->SetScriptName(DefaultName);
			}
	
			if (ab->BaseStats[IE_STATE_ID] & STATE_DEAD)
				ab->StanceID = IE_ANI_SLEEP;
			else
				ab->StanceID = IE_ANI_AWAKE;
			
			ab->Orientation = ( unsigned char ) Orientation;
			ab->TalkCount = TalkCount;
			//hack to not load global actors to area
			if(!game->FindPC(ab->scriptName) && !game->FindNPC(ab->scriptName) ) {
				map->AddActor( ab );
			} else {
				delete ab;
			}
		}
		core->FreeInterface( actmgr );
	}
	str->Seek( AnimOffset, GEM_STREAM_START );
	if (!core->IsAvailable( IE_BAM_CLASS_ID )) {
		printf( "[AREImporter]: No Animation Manager Available, skipping animations\n" );
	} else {
		for (i = 0; i < AnimCount; i++) {
			Animation* anim;
			str->Seek( 32, GEM_CURRENT_POS );
			ieWord animX, animY;
			str->Read( &animX, 2 );
			str->Read( &animY, 2 );
			str->Seek( 4, GEM_CURRENT_POS );
			char animBam[9];
			str->Read( animBam, 8 );
			animBam[8] = 0;
			ieWord animCycle, animFrame;
			str->Read( &animCycle, 2 );
			str->Read( &animFrame, 2 );
			ieDword animFlags;
			str->Read( &animFlags, 4 );
			str->Seek( 20, GEM_CURRENT_POS );
			unsigned char mode = ( ( animFlags & 2 ) != 0 ) ?
				IE_SHADED : IE_NORMAL;
			AnimationFactory* af = ( AnimationFactory* )
			core->GetResourceMgr()->GetFactoryResource( animBam, IE_BAM_CLASS_ID );
			anim = af->GetCycle( ( unsigned char ) animCycle );
			if (!anim)
				anim = af->GetCycle( 0 );
			anim->x = animX;
			anim->y = animY;
			anim->BlitMode = mode;
			anim->autofree = false;
			strcpy( anim->ResRef, animBam );
			map->AddAnimation( anim );
		}
	}
	printf( "Loading entrances\n" );
	//Loading Entrances
	str->Seek( EntrancesOffset, GEM_STREAM_START );
	for (i = 0; i < EntrancesCount; i++) {
		char Name[33];
		ieWord XPos, YPos, Face;
		str->Read( Name, 32 );
		Name[32] = 0;
		str->Read( &XPos, 2 );
		str->Read( &YPos, 2 );
		str->Read( &Face, 2 );
		str->Seek( 66, GEM_CURRENT_POS );
		map->AddEntrance( Name, XPos, YPos, Face );
	}

	printf( "Loading variables\n" );
	//Loading Variables
	map->vars=new Variables();
	map->vars->SetType( GEM_VARIABLES_INT );

	str->Seek( VariablesOffset, GEM_STREAM_START );
	for (i = 0; i < VariablesCount; i++) {
		char Name[33];
		ieDword Value;
		str->Read( Name, 32 );
		Name[32] = 0;
		str->Seek( 8, GEM_CURRENT_POS );
		str->Read( &Value, 4 );
		str->Seek( 40, GEM_CURRENT_POS );
		map->vars->SetAt( Name, Value );
	}
	
	printf( "Loading ambients\n" );
	str->Seek( AmbiOffset, GEM_STREAM_START );
	for (i = 0; i < AmbiCount; i++) {
		Ambient *ambi = new Ambient();
		ieResRef sounds[10];
		ieWord numsounds;

		str->Read( &ambi->name, 32 );
		str->Read( &ambi->origin.x, 2 );
		str->Read( &ambi->origin.y, 2 );
		str->Read( &ambi->radius, 2 );
		str->Read( &ambi->height, 2 );
		str->Seek( 6, GEM_CURRENT_POS );
		str->Read( &ambi->gain, 2 );
		str->Read( &sounds, 80 );
		str->Read( &numsounds, 2 );
		str->Seek( 2, GEM_CURRENT_POS );
		str->Read( &ambi->interval, 4 );
		str->Read( &ambi->perset, 4 );
		str->Read( &ambi->appearance, 4 );
		str->Read( &ambi->flags, 4 );
		str->Seek( 64, GEM_CURRENT_POS );
		
		for (int i = 0; i < numsounds; ++i) {
			char *sound = (char *) malloc(9);
			memcpy(sound, sounds[i], 8);
			sound[8]=0;
			ambi->sounds.push_back(sound);
		}
		map->AddAmbient(ambi);
	}

	map->AddTileMap( tm, lm, sr, sm );
	core->FreeInterface( tmm );
	return map;
}

