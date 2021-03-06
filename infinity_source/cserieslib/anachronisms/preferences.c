/*
PREFERENCES.C
Tuesday, September 29, 1992 11:07:22 AM

Tuesday, September 29, 1992 11:07:24 AM
	Adapted from some internet code, modified heavily in the spirit of the old PREFERENCES.C
Sunday, November 15, 1992 8:29:11 PM
	made to compile for Pathways into Darkness.  there was some confusion about how to properly
	locate the preferences folder, we’re assuming everybody was talking about FindFolder.
	What does the MPW FindFolder glue do under system six?
Sunday, February 21, 1993 10:44:33 AM
	added version checking.
Thursday, March 25, 1993 6:01:43 PM
	when looking for the preferences folder under system six, MPW’s FindFolder glue gives us
	back the system folder.  useless code (SysEnvirons call, etc.) removed.
Sunday, April 4, 1993 6:03:07 PM
	changed NewHandle to NewHandleClear in are_preferences_out_of_data for no reason.
Wednesday, July 28, 1993 8:59:24 AM
	surrounded FSRead, FSWrite with HLock, HUnlock (and saved handle state in write_pref_file).
Wednesday, December 8, 1993 2:16:37 PM
	the preferences are now allocated in a pointer.  are_preferences_out_of_date was removed
	and read_preferences_file does all the magic now.
*/

#include "macintosh_cseries.h"
#include "preferences.h"

#include <Folders.h>

#ifdef mpwc
#pragma segment modules
#endif

/* ---------- constants */

/* ---------- structures */

struct preferences_info
{
	short prefVRefNum;
	long prefDirID;
	
	char prefName[1];
};

/* ---------- globals */

static struct preferences_info *prefInfo;

/* ----------- code */

OSErr write_preferences_file(
	void *preferences)
{
	long size;
	OSErr error;
	short refNum;
	
	error= HOpen(prefInfo->prefVRefNum, prefInfo->prefDirID, prefInfo->prefName, fsRdWrPerm, &refNum);
	if (error==noErr)
	{
		size= GetPtrSize(preferences);
		assert(size>0);
		
		error= FSWrite(refNum, &size, preferences);
		FSClose(refNum);
	}
	
	return error;
}

/* expected_version should be >0.  if initialize_preferences() gets a version number of zero
	it means the prefs are being initialized for the first time.  this should only be called
	once, at startup. */
OSErr read_preferences_file(
	void **preferences,
	char *prefName,
	OSType prefCreator,
	OSType prefType,
	short expected_version,
	long expected_size,
	void (*initialize_preferences)(void *preferences))
{
	CInfoPBPtr infoPB;
	boolean initialize_and_write= FALSE;
	short refNum;
	OSErr error;
	long actual_size, size;

	/* allocate space for our global structure to keep track of the prefs file */
	prefInfo= (struct preferences_info *) NewPtr(sizeof(struct preferences_info)+*prefName);
	BlockMove(prefName, prefInfo->prefName, *prefName+1);

	/* allocate space for the prefs themselves */
	*preferences= NewPtrClear(expected_size);

	/* allocate a parameter block for PBGetCatInfo */
	infoPB= (CInfoPBPtr) NewPtrClear(sizeof(CInfoPBRec));

	if (prefInfo&&infoPB&&preferences)
	{
		/* check for the preferences folder using FindFolder, creating it if necessary */
		FindFolder(kOnSystemDisk, kPreferencesFolderType, kCreateFolder,
			&prefInfo->prefVRefNum, &prefInfo->prefDirID);

		/* does the preferences file exist? */
		infoPB->hFileInfo.ioFDirIndex= 0;
		infoPB->hFileInfo.ioVRefNum= prefInfo->prefVRefNum;
		infoPB->hFileInfo.ioDirID= prefInfo->prefDirID;
		infoPB->hFileInfo.ioNamePtr= prefInfo->prefName;
		error= PBGetCatInfo(infoPB, FALSE);
		switch (error)
		{
			case noErr:
				error= HOpen(prefInfo->prefVRefNum, prefInfo->prefDirID,
					prefInfo->prefName, fsRdWrPerm, &refNum);
				if (error==noErr)
				{
					/* read as many bytes as we can or as will fit into our buffer from the
						preferences file */
					actual_size= infoPB->hFileInfo.ioFlLgLen;
					size= MIN(expected_size, actual_size);
					error= FSRead(refNum, &size, *preferences);
					FSClose(refNum);
					
					if (error==noErr)
					{
						/* if we've got the wrong size or the wrong version, reinitialize */
						if (actual_size!=expected_size||*((short*)*preferences)!=expected_version)
						{
							initialize_preferences(*preferences);
							error= write_preferences_file(*preferences);
						}
					}
				}
				break;
			
			case fnfErr:
				error= HCreate(prefInfo->prefVRefNum, prefInfo->prefDirID, prefInfo->prefName,
					prefCreator, prefType);
				if (error==noErr)
				{
					initialize_preferences(*preferences);
					error= write_preferences_file(*preferences);
				}
				break;
			
			/* errors besides noErr and fnfErr get returned */
		}
		
		DisposePtr((Ptr)infoPB);
	}
	else
	{
		error= MemError();
	}
	
	if (error!=noErr)
	{
		/* if something is broken, make sure we at least return valid prefs */
		initialize_preferences(*preferences);
	}
	
	return error;
}
