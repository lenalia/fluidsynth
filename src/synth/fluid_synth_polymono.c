/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#include "fluid_synth.h"
#include "fluid_chan.h"



/**  API Poly/mono mode ******************************************************/

/**
 * Gets the list of basic channel informations from the synth instance.
 *
 * @param synth the synth instance
 * @param basic_channel_infos
 *  - If not NULL the function returns a pointer to an allocated array of #fluid_basic_channel_infos_t or NULL on allocation error. The caller must free this array when finished with it
 *  - If NULL the function returns only the count of basic channel
 *  - Each entry in the array is a fluid_basic_channels_infos_t information
 * 
 * @return 
 *  - Count of basic channel informations in the returned table. 
 *  - FLUID_FAILED.
 *    - synth is NULL.
 *    - allocation error.
 *
 * @note By default a synth instance has only one basic channel
 * on MIDI channel 0 in Poly Omni On state (i.e all MIDI channels are polyphonic).
 */
int fluid_synth_get_basic_channels(	fluid_synth_t* synth,
					fluid_basic_channel_infos_t **basic_channel_infos)
{
	int i,n_chan;	/* MIDI channel index and number */
	int n_basic_chan; /* Basic Channel number to return */
	/* checks parameters first */
	fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
	fluid_synth_api_enter(synth);
	n_chan = synth->midi_channels; /* MIDI Channels number */

	/* counts basic channels */
	for(i = 0, n_basic_chan = 0; i <  n_chan; i++)
	{
		if (synth->channel[i]->mode &  FLUID_CHANNEL_BASIC)
		{
			n_basic_chan++;
		}
	}

	if (basic_channel_infos && n_basic_chan) 
	{	
		/* allocates table for Basic Channel group only */
		fluid_basic_channel_infos_t * bci;	/* basics channels information table */
		int b; /* index in bci */
		bci = FLUID_ARRAY(fluid_basic_channel_infos_t, n_basic_chan );
		/* fills table */
		if (bci) for(i = 0, b=0; i <  n_chan; i++)
		{	
			fluid_channel_t* chan = synth->channel[i];
			if (chan->mode &  FLUID_CHANNEL_BASIC)
			{	/* This channel is a basic channel */
				bci[b].basicchan = i;	/* channel number */
				bci[b].mode = chan->mode &  FLUID_CHANNEL_MODE_MASK; /* MIDI mode:0,1,2,3 */
				bci[b].val = chan->mode_val;	/* value (for mode 3 only) */
				b++;
			}
		}
		else 
		{
			n_basic_chan = FLUID_FAILED; /* allocation error */
			FLUID_LOG(FLUID_ERR, "Out of memory");
		}
		*basic_channel_infos = bci; /* returns table */
	}
	fluid_synth_api_exit(synth);
	return n_basic_chan;
}

int fluid_synth_set_basic_channel_LOCAL(fluid_synth_t* synth, 
					int basicchan,int mode, int val);
/**
 * Sets a new list of basic channel informations into the synth instance.
 * This list replaces the previous list. 
 *
 * @param synth the synth instance.
 * @param n Number of entries in basic_channel_infos.
 * @param basic_channel_infos the list of basic channel infos to set.
 * If n is 0 or basic_channel_infos is NULL the function set one channel basic at
 * basicchan 0 in Omni On Poly (i.e all the MIDI channels are polyphonic).
 * 
 * @return
 *  - FLUID_OK on success.
 *  - FLUID_FAILED 
 *    - synth is NULL.
 *    - n, basicchan or val is outside MIDI channel count.
 *    - mode is invalid. 
 *    - val has a number of channels overlapping the next basic channel.
 * 
 * @note This API is the only one to replace all the basics channels in the 
 * synth instance.
 */
char * warning_msg ="resetbasicchannels: Different entries have the same basic channel.\n\
An entry supersedes a previous entry with the same basic channel.\n";

int fluid_synth_reset_basic_channels(fluid_synth_t* synth, 
                             int n, 
                             fluid_basic_channel_infos_t *basic_channel_infos)
{
    int i,n_chan;
    int result;
    /* checks parameters first */
    fluid_return_val_if_fail (synth != NULL, FLUID_FAILED);
    fluid_return_val_if_fail (n >= 0, FLUID_FAILED);
    fluid_synth_api_enter(synth);
    
    n_chan = synth->midi_channels; /* MIDI Channels number */
    if (n > n_chan)
    {
        FLUID_API_RETURN(FLUID_FAILED);
    }
    
    /* Checks if information are valid  */
    if(n && basic_channel_infos ) for (i = 0; i < n; i++)
    {
		if (	basic_channel_infos[i].basicchan < 0 || 
			basic_channel_infos[i].basicchan >= n_chan ||
			basic_channel_infos[i].mode < 0 ||
			basic_channel_infos[i].mode >= FLUID_CHANNEL_MODE_LAST ||
			basic_channel_infos[i].val < 0 ||
			basic_channel_infos[i].basicchan + basic_channel_infos[i].val > n_chan)
			{
				FLUID_API_RETURN(FLUID_FAILED);
			}
    }

    /* Clears previous list of basic channel */
    for(i = 0; i <  n_chan; i++)
	{
		fluid_channel_reset_basic_channel_info(synth->channel[i]);
		synth->channel[i]->mode_val = 0; 
    }
    if(n && basic_channel_infos)
    {
		result = FLUID_OK;

		/* Sets the new list of basic channel */
		for (i = 0; i < n; i++)
		{
			int basic_chan = basic_channel_infos[i].basicchan;
			if (synth->channel[basic_chan]->mode &  FLUID_CHANNEL_BASIC)
			{
				/* Different entries have the same basic channel. 
				An entry supersedes a previous entry with the same 
				basic channel.*/
				FLUID_LOG(FLUID_INFO, warning_msg);
			}
			else
			{	/* Set Basic channel first */
				synth->channel[basic_chan]->mode |= FLUID_CHANNEL_BASIC;
			}
		}

		for (i = 0; i < n; i++)
		{
			int r =fluid_synth_set_basic_channel_LOCAL( synth, 
					basic_channel_infos[i].basicchan,
					basic_channel_infos[i].mode,
					basic_channel_infos[i].val);
			if (result == FLUID_OK)
			{
				result = r;
			}
		}
    }
    else 
	{
		result = fluid_synth_set_basic_channel_LOCAL( synth, 0, FLUID_CHANNEL_MODE_OMNION_POLY,0);
	}
    FLUID_API_RETURN(result);
}

/**
 * Changes the mode of an existing basic channel or inserts a new basic channel part.
 *
 * - If basicchan is already a basic channel, the mode is changed.
 * - If basicchan is not a basic channel, a new basic channel part is inserted between
 * the previous basic channel and the next basic channel. val value of the previous
 * basic channel will be narrowed if necessary. 
 * 
 * @param synth the synth instance.
 * @param modeInfos pointer to a #fluid_basic_channel_infos_t struct.
 * @note about information in #fluid_basic_channel_infos_t struct:  
 *   - basicchan, the basic bhannel number (0 to MIDI channel count-1).
 *   - mode, the MIDI mode to use for basicchan (0 to 3).
 *   - val, number of monophonic channels (for mode 3 only) (0 to MIDI channel count).
 * 
 * @return 
 * - FLUID_OK on success.
 * - FLUID_FAILED
 *   - basic_channel_infos is NULL.
 *   - synth is NULL.
 *   - chan or val is outside MIDI channel count.
 *   - mode is invalid.
 *   - val has a number of channels overlapping the next basic channel.
 */
int fluid_synth_set_basic_channel(fluid_synth_t* synth, 
								fluid_basic_channel_infos_t *basic_channel_infos)
{
    int result;
    int chan ;
    int mode ;
    int val ;

	fluid_return_val_if_fail (basic_channel_infos!= NULL, FLUID_FAILED);
    chan = basic_channel_infos->basicchan;
    mode = basic_channel_infos->mode;
    val = basic_channel_infos->val;
    fluid_return_val_if_fail (mode >= 0, FLUID_FAILED);
    fluid_return_val_if_fail (mode < FLUID_CHANNEL_MODE_LAST, FLUID_FAILED);
    fluid_return_val_if_fail (val >= 0, FLUID_FAILED);
    FLUID_API_ENTRY_CHAN(FLUID_FAILED);
    /**/
    if (chan + val > synth->midi_channels)
	{
        FLUID_API_RETURN(FLUID_FAILED);
	}

    result = fluid_synth_set_basic_channel_LOCAL(synth,chan,mode,val);
    /**/
    FLUID_API_RETURN(result);
}

/**
 * Changes the mode of an existing basic channel or inserts a new basic channel part.
 *
 * - If basicchan is already a basic channel, the mode is changed.
 * - If basicchan is not a basic channel, a new basic channel part is inserted between
 * the previous basic channel and the next basic channel. val value of the previous
 * basic channel will be narrowed if necessary. 
 * 
 * The function is used internally by API fluid_synth_reset_basic_channels() and
 * fluid_synth_set_basic_channel().
 * 
 * @param synth the synth instance.
 * @param basicchan the Basic Channel number (0 to MIDI channel count-1).
 * @param mode the MIDI mode to use for basicchan (0 to 3).
 * @param val Number of monophonic channels (for mode 3 only) (0 to MIDI channel count).
 * 
 * @return 
 * - FLUID_OK on success.
 * - FLUID_FAILED
 *   - basicchan is outside MIDI channel count.
 *   - val has a number of channels overlapping the next basic channel.
 */

int fluid_synth_set_basic_channel_LOCAL(fluid_synth_t* synth, 
					int basicchan,int mode, int val)
{
	int n_chan = synth->midi_channels; /* MIDI Channels number */
	int result = FLUID_FAILED; /* default return */
	if (basicchan < n_chan)
	{
		static const char * warning_msg1 = "Basic channel %d has been narrowed to %d channels.";
		static const char * warning_msg2 = "Basic channel %d has number of channels that overlaps\n\
the next basic channel\n";
		int last_begin_range; /* Last channel num inside the beginning range + 1. */
		int last_end_range; /* Last channel num inside the ending range + 1. */
		int prev_basic_chan = -1 ; // Previous basic channel
		int i;
		result = FLUID_OK;
		if ( !(synth->channel[basicchan]->mode &  FLUID_CHANNEL_BASIC))
		{	/* a new basic channel is inserted between previous basic channel 
			and the next basic channel.	*/
			if (synth->channel[basicchan]->mode &  FLUID_CHANNEL_ENABLED)
			{ /* val value of the previous basic channel need to be narrowed */
				for (i = basicchan - 1; i >=0; i--)
				{	/* searchs previous basic channel */
					if (synth->channel[i]->mode &  FLUID_CHANNEL_BASIC)
					{	/* i is the previous basic channel */
						prev_basic_chan = i;
						break;
					}
				}
			}	
		}

		/* last_end_range: next basic channel  or midi channels count  */
		for (last_end_range = basicchan +1; last_end_range < n_chan; last_end_range++)
		{
			if (synth->channel[last_end_range]->mode &  FLUID_CHANNEL_BASIC) break;
		}
		/* Now last_begin_range is set */
		switch (mode = mode &  FLUID_CHANNEL_MODE_MASK)
		{
			case FLUID_CHANNEL_MODE_OMNION_POLY:	/* Mode 0 and 1 */
			case FLUID_CHANNEL_MODE_OMNION_MONO:
				last_begin_range = last_end_range;
				break;
			case FLUID_CHANNEL_MODE_OMNIOFF_POLY:		/* Mode 2 */
				last_begin_range = basicchan + 1;
				break;
			case FLUID_CHANNEL_MODE_OMNIOFF_MONO:		/* Mode 3 */
				if (val)
				{
					last_begin_range = basicchan + val;
				}
				else
				{
					last_begin_range = last_end_range;
				}
		}
		/* Check if val overlaps the next basic channel */
		if (last_begin_range > last_end_range)
		{	
			/* val have number of channels that overlaps the next basic channel */
			FLUID_LOG(FLUID_INFO,warning_msg2,basicchan);
			return FLUID_FAILED;
		}
		/* if previous basic channel exists, val is narrowed */
		if(prev_basic_chan >= 0)
		{
			synth->channel[prev_basic_chan]->mode_val = basicchan - prev_basic_chan;
			FLUID_LOG(FLUID_INFO,warning_msg1,prev_basic_chan,basicchan - prev_basic_chan);
		}

		/* val is limited up to last_begin_range */
		val = last_begin_range - basicchan;
		/* Set the Mode to the range zone: Beginning range + Ending range */
		for (i = basicchan; i < last_end_range; i++)
		{	
			int new_mode = mode; /* OMNI_OFF/ON, MONO/POLY ,others bits are zero */
			/* MIDI specs: when mode is changed, channel must receive
			 ALL_NOTES_OFF */
			fluid_synth_all_notes_off_LOCAL (synth, i);
			/* basicchan only is marked Basic Channel */
			if (i == basicchan)
			{
				new_mode |= FLUID_CHANNEL_BASIC; 
			}
			else
			{
				val =0; /* val is 0 for other channel than basic channel */
			}
			/* Channel in beginning zone are enabled */
			if (i < last_begin_range)
			{
				new_mode |= FLUID_CHANNEL_ENABLED;
			}
			/* Channel in ending zone are disabled */
			else
			{
				new_mode = 0;
			}
			/* Now mode is OMNI OFF/ON,MONO/POLY, BASIC_CHANNEL or not
			   ENABLED or not */	
			fluid_channel_set_basic_channel_info(synth->channel[i],new_mode);
			synth->channel[i]->mode_val = val;
		}
	}
	return result;
}

/**
 * Returns poly mono mode informations from any MIDI channel.
 *
 * @param synth the synth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param modeInfos Pointer to a #fluid_basic_channel_infos_t struct
 * @note about informations returned in #fluid_basic_channel_infos_t struct: 
 *   - basicchan, the basic channel chan belongs to. ( -1 if chan is
 *      disabled).
 *   - mode, the flags (see enum fluid_channel_mode_flags) of chan.
 *   - val, if chan is a basic channel, the number of MIDI channels belonging
 *          to this basic channel is returned , otherwise this field is 0.
 * 
 * @return
 * - FLUID_OK on success.
 * - FLUID_FAILED 
 *   - synth is NULL.
 *   - chan is outside MIDI channel count.
 *   - modeInfos is NULL.
 */
int fluid_synth_get_channel_mode(fluid_synth_t* synth, int chan,
				fluid_basic_channel_infos_t  *mode_infos)
{
	/* checks parameters first */
	fluid_return_val_if_fail (mode_infos!= NULL, FLUID_FAILED);
	FLUID_API_ENTRY_CHAN(FLUID_FAILED);
	/**/
	/* if chan is enabled , we search the basic channel chan belongs to 
	  otherwise chan doesn't belong to any basic channel part*/
	mode_infos->basicchan= -1; /* means no basic channel found */
	if (synth->channel[chan]->mode &  FLUID_CHANNEL_ENABLED)
	{ /* chan is enabled , we search the basic channel chan belongs to */
		int i;
		for (i = chan ; i >=0; i--)
		{
			if (synth->channel[i]->mode &  FLUID_CHANNEL_BASIC)
			{	/* i is the basic channel */
				mode_infos->basicchan = i;
				break;
			}
		}
	}	
	mode_infos->mode = synth->channel[chan]->mode;
	mode_infos->val = synth->channel[chan]->mode_val;
	/**/
	FLUID_API_RETURN(FLUID_OK);
}
/**  API legato mode *********************************************************/

/**
 * Sets the legato mode of a channel.
 * 
 * @param synth the synth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param legatomode The legato mode as indicated by #fluid_channel_legato_mode
 *
 * @return
 * - FLUID_OK on success.
 * - FLUID_FAILED 
 *   - synth is NULL.
 *   - chan is outside MIDI channel count.
 *   - legatomode is invalid.
 */
int fluid_synth_set_legato_mode(fluid_synth_t* synth, int chan, int legatomode)
{
	/* checks parameters first */
	fluid_return_val_if_fail (legatomode >= 0, FLUID_FAILED);
	fluid_return_val_if_fail (legatomode < FLUID_CHANNEL_LEGATO_MODE_LAST, FLUID_FAILED);
	FLUID_API_ENTRY_CHAN(FLUID_FAILED);
	/**/
	synth->channel[chan]->legatomode = legatomode;
	/**/
	FLUID_API_RETURN(FLUID_OK);
}

/**
 * Gets the legato mode of a channel.
 *
 * @param synth the synth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param legatomode The legato mode as indicated by #fluid_channel_legato_mode
 *
 * @return
 * - FLUID_OK on success.
 * - FLUID_FAILED 
 *   - synth is NULL.
 *   - chan is outside MIDI channel count.
 *   - legatomode is NULL.
 */
int fluid_synth_get_legato_mode(fluid_synth_t* synth, int chan, int *legatomode)
{
	/* checks parameters first */
	fluid_return_val_if_fail (legatomode!= NULL, FLUID_FAILED);
	FLUID_API_ENTRY_CHAN(FLUID_FAILED);
	/**/
	* legatomode = synth->channel[chan]->legatomode;
	/**/
	FLUID_API_RETURN(FLUID_OK);
}

/**  API portamento mode *********************************************************/

/**
 * Sets the portamento mode of a channel.
 *
 * @param synth the synth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param portamentomode The portamento mode as indicated by #fluid_channel_portamento_mode
 * @return
 * - FLUID_OK on success.
 * - FLUID_FAILED 
 *   - synth is NULL.
 *   - chan is outside MIDI channel count.
 *   - portamentomode is invalid.
 */
int fluid_synth_set_portamento_mode(fluid_synth_t* synth, int chan,
					int portamentomode)
{
	/* checks parameters first */
	fluid_return_val_if_fail (portamentomode >= 0, FLUID_FAILED);
	fluid_return_val_if_fail (portamentomode < FLUID_CHANNEL_PORTAMENTO_MODE_LAST, FLUID_FAILED);
	FLUID_API_ENTRY_CHAN(FLUID_FAILED);
	/**/
	synth->channel[chan]->portamentomode = portamentomode;
	/**/
	FLUID_API_RETURN(FLUID_OK);
}

/**
 * Gets the portamento mode of a channel.
 *
 * @param synth the synth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param portamentomode Pointer to the portamento mode as indicated by #fluid_channel_portamento_mode
 * @return
 * - FLUID_OK on success.
 * - FLUID_FAILED 
 *   - synth is NULL.
 *   - chan is outside MIDI channel count.
 *   - portamentomode is NULL.
 */
int fluid_synth_get_portamento_mode(fluid_synth_t* synth, int chan,
					int *portamentomode)
{
	/* checks parameters first */
	fluid_return_val_if_fail (portamentomode!= NULL, FLUID_FAILED);
	FLUID_API_ENTRY_CHAN(FLUID_FAILED);
	/**/
	* portamentomode = synth->channel[chan]->portamentomode;
	/**/
	FLUID_API_RETURN(FLUID_OK);
}

/**  API breath mode *********************************************************/

/**
 * Sets the breath mode of a channel.
 *
 * @param synth the synth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param breathmode The breath mode as indicated by #fluid_channel_breath_flags
 *
 * @return
 * - FLUID_OK on success.
 * - FLUID_FAILED 
 *   - synth is NULL.
 *   - chan is outside MIDI channel count.
 */
int fluid_synth_set_breath_mode(fluid_synth_t* synth, int chan, int breathmode)
{
	/* checks parameters first */
	FLUID_API_ENTRY_CHAN(FLUID_FAILED);
	/**/
	fluid_channel_set_breath_info(synth->channel[chan],breathmode);
	/**/
	FLUID_API_RETURN(FLUID_OK);
}

/**
 * Gets the breath mode of a channel.
 *
 * @param synth the synth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param breathmode Pointer to the returned breath mode as indicated by #fluid_channel_breath_flags
 *
 * @return
 * - FLUID_OK on success.
 * - FLUID_FAILED 
 *   - synth is NULL.
 *   - chan is outside MIDI channel count.
 *   - breathmode is NULL.
 */
int fluid_synth_get_breath_mode(fluid_synth_t* synth, int chan, int *breathmode)
{
	/* checks parameters first */
	fluid_return_val_if_fail (breathmode!= NULL, FLUID_FAILED);
	FLUID_API_ENTRY_CHAN(FLUID_FAILED);
	/**/
	* breathmode = fluid_channel_get_breath_info(synth->channel[chan]);
	/**/
	FLUID_API_RETURN(FLUID_OK);
}

