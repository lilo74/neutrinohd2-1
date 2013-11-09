/*
	Neutrino-GUI  -   DBoxII-Project
	
	$Id: infoviewer.cpp 2013/10/12 mohousch Exp $

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <unistd.h>

#include <map>

#include <fcntl.h>

#include <gui/widget/progressbar.h>
#include <gui/infoviewer.h>

#include <gui/widget/icons.h>
#include <gui/widget/hintbox.h>

#include <daemonc/remotecontrol.h>

#include <global.h>
#include <neutrino.h>
#include <gui/pictureviewer.h>

#include <gui/movieplayer.h>


#include <sys/timeb.h>
#include <time.h>
#include <sys/param.h>

/*zapit includes*/
#include <satconfig.h>
#include <frontend_c.h>

#include <video_cs.h>
#include <system/debug.h>


void sectionsd_getEventsServiceKey(t_channel_id serviceUniqueKey, CChannelEventList &eList, char search = 0, std::string search_text = "");
void sectionsd_getCurrentNextServiceKey(t_channel_id uniqueServiceKey, CSectionsdClient::responseGetCurrentNextInfoChannelID& current_next );

extern CRemoteControl * g_RemoteControl;		/* neutrino.cpp */
extern CPictureViewer * g_PicViewer;			// neutrino.cpp

extern cVideo * videoDecoder;				// libdvbapi
extern CFrontend * live_fe;				// zapit.cpp
extern fe_map_t femap;					// zapit.cpp
extern CFrontend * getFE(int index);			// zapit.cpp
extern int FrontendCount;				// defined in zapit.cpp
extern CWebTV * webtv;					// defined in neutrino.cpp


#define COL_INFOBAR_BUTTONS            (COL_INFOBAR_SHADOW + 1)
#define COL_INFOBAR_BUTTONS_BACKGROUND (COL_INFOBAR_SHADOW_PLUS_1)

#define SHADOW_OFFSET 	6
#define borderwidth 	4
#define LEFT_OFFSET 	5
#define RIGHT_OFFSET	5

// in us
#define LCD_UPDATE_TIME_TV_MODE (60 * 1000 * 1000)

int time_left_width;
int time_dot_width;
int time_width;
int time_height;
bool newfreq = true;
char old_timestr[10];
static event_id_t last_curr_id = 0, last_next_id = 0;

static bool sortByDateTime(const CChannelEvent& a, const CChannelEvent& b)
{
        return a.startTime < b.startTime;
}

extern int timeshift;
extern bool autoshift;
extern uint32_t shift_timer;

#define RED_BAR 		40
#define YELLOW_BAR 		70
#define GREEN_BAR 		100

#define BAR_WIDTH 		72
#define TIMESCALE_BAR_HEIGHT	6
#define SIGSCALE_BAR_HEIGHT	8
#define SNRSCALE_BAR_HEIGHT	8

#define REC_INFOBOX_WIDTH	300
#define REC_INFOBOX_HEIGHT	20

#define SAT_INFOBOX_HEIGHT	30

#define BUTTON_BAR_HEIGHT	20
#define CHANNAME_HEIGHT		35

#define MOVIE_TIMEBAR_H		40

extern std::string ext_channel_name;	// defined in vcrcontrol.cpp
int m_CA_Status;
extern bool timeset;			// defined in sectionsd.cpp

static int icon_w_subt, icon_h_subt;
static int icon_w_vtxt, icon_h_vtxt;
static int icon_w_aspect, icon_h_aspect;
static int icon_w_dd, icon_h_dd;
static int icon_w_sd, icon_h_sd;
static int icon_w_reso, icon_h_reso;
static int icon_w_ca, icon_h_ca;
static int icon_w_rt, icon_h_rt;
static int icon_w_rec, icon_h_rec;
static int TunerNumWidth;

// channel logo
int PIC_W = CHANNAME_HEIGHT*1.67;
int PIC_H = CHANNAME_HEIGHT;

CInfoViewer::CInfoViewer()
{
  	Init();
}

void CInfoViewer::Init()
{
	frameBuffer = CFrameBuffer::getInstance ();
	
	//
	recordModeActive = false;
	is_visible = false;
	m_visible = false;

	showButtonBar = false;

	gotTime = timeset;
	CA_Status = false;
	virtual_zap_mode = false;
	chanready = 1;
	
	//
	frameBuffer->getIconSize(NEUTRINO_ICON_16_9, &icon_w_aspect, &icon_h_aspect);
	frameBuffer->getIconSize(NEUTRINO_ICON_VTXT, &icon_w_vtxt, &icon_h_vtxt);
	frameBuffer->getIconSize(NEUTRINO_ICON_RESOLUTION_000, &icon_w_reso, &icon_h_reso);
	frameBuffer->getIconSize(NEUTRINO_ICON_RESOLUTION_HD, &icon_w_sd, &icon_h_sd);
	frameBuffer->getIconSize(NEUTRINO_ICON_SUBT, &icon_w_subt, &icon_h_subt);
	frameBuffer->getIconSize(NEUTRINO_ICON_DD, &icon_w_dd, &icon_h_dd);
	frameBuffer->getIconSize(NEUTRINO_ICON_SCRAMBLED2_GREY, &icon_w_ca, &icon_h_ca);
	frameBuffer->getIconSize(NEUTRINO_ICON_RADIOTEXTOFF, &icon_w_rt, &icon_h_rt);
	frameBuffer->getIconSize(NEUTRINO_ICON_REC, &icon_w_rec, &icon_h_rec);
	
	TunerNumWidth = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth("T9", true);
}

void CInfoViewer::start()
{
	ChanNumberWidth = 100;
	ChanNumberHeight = CHANNAME_HEIGHT;
	
	lcdUpdateTimer = g_RCInput->addTimer(LCD_UPDATE_TIME_TV_MODE, false, true);
}

void CInfoViewer::paintTime(bool show_dot, bool firstPaint)
{
	if (gotTime) 
	{
		int ChanNameY = BoxStartY + SAT_INFOBOX_HEIGHT + TIMESCALE_BAR_HEIGHT + 5;
	
		char timestr[10];
		struct timeb tm;
	
		ftime (&tm);
		strftime ((char *) &timestr, 20, "%H:%M", localtime (&tm.time));
	
		if ((!firstPaint) && (strcmp(timestr, old_timestr) == 0)) 
		{
			if (show_dot)
				frameBuffer->paintBoxRel(BoxEndX - time_width + time_left_width - LEFT_OFFSET, ChanNameY + 5, time_dot_width, time_height/2, COL_INFOBAR_PLUS_0);
			else
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString (BoxEndX - time_width + time_left_width - LEFT_OFFSET, ChanNameY + time_height, time_dot_width, ":", COL_INFOBAR);

			strcpy (old_timestr, timestr);
		} 
		else 
		{
			strcpy (old_timestr, timestr);
	
			if (!firstPaint) 
			{
				frameBuffer->paintBoxRel(BoxEndX - time_width - LEFT_OFFSET, ChanNameY, time_width + LEFT_OFFSET, time_height, COL_INFOBAR_PLUS_0, RADIUS_MID, CORNER_TOP);
			}
	
			timestr[2] = 0;
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString (BoxEndX - time_width - LEFT_OFFSET, ChanNameY + time_height, time_left_width, timestr, COL_INFOBAR);

			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString (BoxEndX - time_left_width - LEFT_OFFSET, ChanNameY + time_height, time_left_width, &timestr[3], COL_INFOBAR);

			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString (BoxEndX - time_width + time_left_width - LEFT_OFFSET, ChanNameY + time_height, time_dot_width, ":", COL_INFOBAR);

			if (show_dot)
				frameBuffer->paintBoxRel (BoxEndX - time_left_width - time_dot_width - LEFT_OFFSET, ChanNameY + 5, time_dot_width, time_height/2, COL_INFOBAR_PLUS_0);
		}
	}
}

void CInfoViewer::showRecordIcon(const bool show)
{
	recordModeActive = CNeutrinoApp::getInstance()->recordingstatus || shift_timer;

	if (recordModeActive) 
	{
		if (show) 
		{
			frameBuffer->paintIcon(autoshift ? NEUTRINO_ICON_AUTO_SHIFT : NEUTRINO_ICON_REC, BoxStartX + 5, BoxStartY - 30);

			if(!autoshift && !shift_timer) 
			{
				// shadow
				frameBuffer->paintBoxRel(BoxStartX + 5 + icon_w_rec + 5 + SHADOW_OFFSET, BoxStartY - 30 + SHADOW_OFFSET, REC_INFOBOX_WIDTH, REC_INFOBOX_HEIGHT, COL_INFOBAR_SHADOW_PLUS_0);
				
				// rec info box
				frameBuffer->paintBoxRel(BoxStartX + 5 + icon_w_rec + 5, BoxStartY - 30, REC_INFOBOX_WIDTH, REC_INFOBOX_HEIGHT, COL_INFOBAR_PLUS_0);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString (BoxStartX + 5 + icon_w_rec + 5 + 10, BoxStartY - 8, REC_INFOBOX_WIDTH, ext_channel_name.c_str(), COL_INFOBAR, 0, true);
			} 
			else
				frameBuffer->paintBackgroundBoxRel(BoxStartX + 5, BoxStartY - 30, icon_w_rec, icon_h_rec);
		} 
		else 
			frameBuffer->paintBackgroundBoxRel(BoxStartX + 5, BoxStartY - 30, icon_w_rec, icon_h_rec);
	}
}

void CInfoViewer::showTitle(const int ChanNum, const std::string & Channel, const t_satellite_position satellitePosition, const t_channel_id new_channel_id, const bool calledFromNumZap, int epgpos)
{
	last_curr_id = last_next_id = 0;
	
	std::string ChannelName = Channel;

	bool show_dot = true;
	bool new_chan = false;

	showButtonBar = !calledFromNumZap;

	is_visible = true;
	
	newfreq = true;
	
	// get dimensions
	BoxHeight = 160;
	
	BoxEndX = g_settings.screen_EndX - 10;
	BoxEndY = g_settings.screen_EndY - 10 - SHADOW_OFFSET;
	BoxStartX = g_settings.screen_StartX + 10;
	BoxStartY = BoxEndY - BoxHeight;
	BoxWidth = BoxEndX - BoxStartX;
	
	// get all icons size
	frameBuffer->getIconSize(NEUTRINO_ICON_16_9, &icon_w_aspect, &icon_h_aspect);
	frameBuffer->getIconSize(NEUTRINO_ICON_VTXT, &icon_w_vtxt, &icon_h_vtxt);
	frameBuffer->getIconSize(NEUTRINO_ICON_RESOLUTION_000, &icon_w_reso, &icon_h_reso);
	frameBuffer->getIconSize(NEUTRINO_ICON_RESOLUTION_HD, &icon_w_sd, &icon_h_sd);
	frameBuffer->getIconSize(NEUTRINO_ICON_SUBT, &icon_w_subt, &icon_h_subt);
	frameBuffer->getIconSize(NEUTRINO_ICON_DD, &icon_w_dd, &icon_h_dd);
	frameBuffer->getIconSize(NEUTRINO_ICON_SCRAMBLED2_GREY, &icon_w_ca, &icon_h_ca);
	frameBuffer->getIconSize(NEUTRINO_ICON_RADIOTEXTOFF, &icon_w_rt, &icon_h_rt);
	
	// init progressbar
	sigscale = new CProgressBar(BAR_WIDTH, SIGSCALE_BAR_HEIGHT, RED_BAR, GREEN_BAR, YELLOW_BAR);
	snrscale = new CProgressBar(BAR_WIDTH, SNRSCALE_BAR_HEIGHT, RED_BAR, GREEN_BAR, YELLOW_BAR);
	timescale = new CProgressBar(BoxWidth - 10, TIMESCALE_BAR_HEIGHT, 30, GREEN_BAR, 70, true);	//5? see in code
	
	sigscale->reset(); 
	snrscale->reset(); 
	timescale->reset();

	// time dimension
	time_height = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getHeight() + 5; //FIXME
	time_left_width = 2 * g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth(widest_number);
	time_dot_width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth(":");
	time_width = time_left_width*2 + time_dot_width;

	if (!gotTime)
		gotTime = timeset;

	int col_NumBoxText;
	int col_NumBox;

	if (virtual_zap_mode) 
	{
		col_NumBoxText = COL_MENUHEAD;
		col_NumBox = COL_MENUHEAD_PLUS_0;

		if ((channel_id != new_channel_id) || (evtlist.empty())) 
		{
			evtlist.clear();
			sectionsd_getEventsServiceKey(new_channel_id & 0xFFFFFFFFFFFFULL, evtlist);
			
			if (!evtlist.empty())
				sort(evtlist.begin(),evtlist.end(), sortByDateTime);
			
			new_chan = true;
		}
	} 
	else 
	{
		col_NumBoxText = COL_INFOBAR;
		col_NumBox = COL_INFOBAR_PLUS_0;
	}

	if (! calledFromNumZap && !(g_RemoteControl->subChannels.empty()) && (g_RemoteControl->selected_subchannel > 0))
	{
		channel_id = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].getChannelID();
		ChannelName = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].subservice_name;
	} 
	else 
	{
		channel_id = new_channel_id;
	}

	// chan name/info dimension
	int ChanNameX = BoxStartX + ChanNumberWidth + 10;
	int ChanNameY = BoxStartY + SAT_INFOBOX_HEIGHT + TIMESCALE_BAR_HEIGHT + 5;
	
	ChanInfoX = BoxStartX + ChanNumberWidth + 10;

	// button cell width
	asize = (BoxWidth - ( 2 + icon_w_subt + 2 + icon_w_vtxt + 2 + icon_w_dd + 2 + icon_w_aspect + 2 + icon_w_sd + 2 + icon_w_reso + 2 + icon_w_ca + 2 + icon_w_rt + 2 + TunerNumWidth + 2))/4;
	
	// infobar shadow
	frameBuffer->paintBoxRel(BoxStartX + SHADOW_OFFSET, BoxStartY + SHADOW_OFFSET, BoxWidth, BoxHeight, COL_INFOBAR_SHADOW_PLUS_0, RADIUS_MID, CORNER_BOTH);
	
	// infobar box
	frameBuffer->paintBoxRel(BoxStartX, BoxStartY, BoxWidth, BoxHeight, COL_INFOBAR_PLUS_0, RADIUS_MID, CORNER_BOTH);
	
	// timescale position
	int posx = BoxStartX + 5;
	int posy = BoxStartY + SAT_INFOBOX_HEIGHT;
	
	// event progressbar bg
	frameBuffer->paintBoxRel(posx, posy, BoxWidth - 10, TIMESCALE_BAR_HEIGHT, COL_INFOBAR_BUTTONS_BACKGROUND);

	//time
	paintTime(show_dot, true);

	//record-icon
	showRecordIcon(show_dot);
	show_dot = !show_dot;

	//sat name
	int SatNameHeight = g_SignalFont->getHeight();

	char strChanNum[10];
	sprintf(strChanNum, "%d", ChanNum);

	if (satellitePositions.size()) 
	{
		sat_iterator_t sit = satellitePositions.find(satellitePosition);

		if(sit != satellitePositions.end()) 
		{
			satNameWidth = g_SignalFont->getRenderWidth(sit->second.name);
			
			// NOTE:freqStartX = BoxStartX + ChanNumberWidth + 80;
			if (satNameWidth > (ChanNumberWidth + 70))
				satNameWidth = ChanNumberWidth + 70;
				
			g_SignalFont->RenderString( BoxStartX + 5, BoxStartY + (SatNameHeight*3)/2, satNameWidth, sit->second.name, COL_INFOBAR );
		}
	}

	// channel logo/number/name
	if (satellitePosition != 0 && satellitePositions.size() ) 
	{
		// ChannelNumber (centered)
		int stringstartposX = BoxStartX + 10;
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString( stringstartposX, ChanNameY + time_height, ChanNumberWidth, strChanNum, col_NumBoxText);
			
		PIC_X = BoxStartX + ChanNumberWidth + 10;
		PIC_Y = BoxStartY + SAT_INFOBOX_HEIGHT + TIMESCALE_BAR_HEIGHT + 10;
		
		int ChanNameWidth = BoxWidth - (LEFT_OFFSET + time_width + ChanNumberWidth + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth(ChannelName, true));

		// display channel picon
		bool logo_ok = false;
		
		//
		int logo_w = PIC_W; 
		int logo_h = PIC_H;
		int logo_bpp = 0;
		
		// check logo
		logo_ok = g_PicViewer->checkLogo(channel_id);
		
		if(logo_ok)
		{
			// get logo size	
			g_PicViewer->getLogoSize(channel_id, &logo_w, &logo_h, &logo_bpp);
		
			// display logo
			g_PicViewer->DisplayLogo(channel_id, PIC_X, PIC_Y, (logo_bpp == 4 && !g_settings.show_channelname)? logo_w : PIC_W, PIC_H, (logo_h > PIC_H)? true : false, false, true);

			// recalculate ChanNameWidth
			ChanNameWidth = BoxWidth - (time_width + ChanNumberWidth + logo_w + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->getRenderWidth(ChannelName, true));
			
			// ChannelName
			if(g_settings.show_channelname)
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString(PIC_X + ((logo_bpp == 4)? logo_w : PIC_W) + 10, ChanNameY + time_height, ChanNameWidth, ChannelName, COL_INFOBAR, 0, true);	// UTF-8
		}
		else
		{
			// ChannelName
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_CHANNAME]->RenderString( BoxStartX + ChanNumberWidth + 10, ChanNameY + time_height, ChanNameWidth, ChannelName, COL_INFOBAR, 0, true);	// UTF-8
		}
	}

	int ChanInfoY = BoxStartY + SAT_INFOBOX_HEIGHT + TIMESCALE_BAR_HEIGHT + 5 + ChanNumberHeight + 5;
		
	// show date
	char datestr[11];
			
	time_t wakeup_time;
	struct tm *now;
			
	time(&wakeup_time);
	now = localtime(&wakeup_time);
		
	strftime(datestr, sizeof(datestr), "%d.%m.%Y", now);
			
	int widthtime = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth(datestr, true); //UTF-8
			
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(BoxEndX - 5 - widthtime, BoxStartY + (SatNameHeight*3)/2, widthtime, datestr, COL_INFOBAR, 0, true); // UTF-8
		
	// botton bar
	frameBuffer->paintBoxRel(BoxStartX, BoxEndY - BUTTON_BAR_HEIGHT, BoxWidth, BUTTON_BAR_HEIGHT, COL_INFOBAR_BUTTONS_BACKGROUND, RADIUS_MID, CORNER_BOTTOM);

	//signal
	showSNR();

	// blue button
	// features
	if(!timeshift)
	{
		int icon_w;
		int icon_h;
			
		frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_BLUE, &icon_w, &icon_h);
		
		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_BLUE, BoxStartX + 5 + icon_w + 5 + asize + icon_w + 5 + asize + icon_w + 5 + asize, BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h)/2);
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(BoxStartX + 5 + icon_w + 5 + asize + icon_w + 5 + asize + icon_w + 5 + asize + icon_w + 5, BoxEndY, asize - 5 - icon_w, g_Locale->getText(LOCALE_INFOVIEWER_FEATURES), COL_INFOBAR_BUTTONS, 0, true); // UTF-8
	}

	if( showButtonBar )
	{
		// add sec timer
		sec_timer_id = g_RCInput->addTimer(1*1000*1000, false);
		
		// green
		showButton_Audio();
			
		// yellow
		// sub services/help for timeshift
		if(timeshift)
		{
			int icon_w;
			int icon_h;
		
			frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_YELLOW, &icon_w, &icon_h);
	
			frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW, BoxStartX + 5 + icon_w + 5 + asize + icon_w + 5 + asize, BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h)/2 );
			g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(BoxStartX + 5 + icon_w + 5 + asize + icon_w + 5 + asize + icon_w + 5, BoxEndY, asize - 5 - icon_w, (char *)"help", COL_INFOBAR_BUTTONS, 0, true); // UTF-8
		}
		else
			showButton_SubServices();
			
		showIcon_CA_Status(0);
		showIcon_16_9();
		showIcon_VTXT();
		showIcon_SubT();
		showIcon_Resolution();
	}

	// show current_next epg data
	sectionsd_getCurrentNextServiceKey(channel_id & 0xFFFFFFFFFFFFULL, info_CurrentNext);
	
	if (!evtlist.empty()) 
	{
		if (new_chan) 
		{
			for ( eli = evtlist.begin(); eli!=evtlist.end(); ++eli ) 
			{
				if ((uint)eli->startTime >= info_CurrentNext.current_zeit.startzeit + info_CurrentNext.current_zeit.dauer)
					break;
			}
			
			if (eli == evtlist.end()) // the end is not valid, so go back
				--eli;
		}

		if (epgpos != 0) 
		{
			info_CurrentNext.flags = 0;
			if ((epgpos > 0) && (eli != evtlist.end())) 
			{
				++eli; // next epg
				if (eli == evtlist.end()) // the end is not valid, so go back
					--eli;
			}
			else if ((epgpos < 0) && (eli != evtlist.begin())) 
			{
				--eli; // prev epg
			}

			info_CurrentNext.flags = CSectionsdClient::epgflags::has_current;
			info_CurrentNext.current_uniqueKey      = eli->eventID;
			info_CurrentNext.current_zeit.startzeit = eli->startTime;
			info_CurrentNext.current_zeit.dauer     = eli->duration;

			if (eli->description.empty())
				info_CurrentNext.current_name   = g_Locale->getText(LOCALE_INFOVIEWER_NOEPG);
			else
				info_CurrentNext.current_name   = eli->description;

			info_CurrentNext.current_fsk            = '\0';

			if (eli != evtlist.end()) 
			{
				++eli;
				if (eli != evtlist.end()) 
				{
					info_CurrentNext.flags                  = CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::has_next;
					info_CurrentNext.next_uniqueKey         = eli->eventID;
					info_CurrentNext.next_zeit.startzeit    = eli->startTime;
					info_CurrentNext.next_zeit.dauer        = eli->duration;

					if (eli->description.empty())
						info_CurrentNext.next_name      = g_Locale->getText(LOCALE_INFOVIEWER_NOEPG);
					else
						info_CurrentNext.next_name      = eli->description;
				}
				--eli;
			}
		}
	}

	if (!(info_CurrentNext.flags & (CSectionsdClient::epgflags::has_later | CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::not_broadcast))) 
	{
		// nicht gefunden / noch nicht geladen
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (BoxStartX + ChanNumberWidth + 10, ChanInfoY + 2*g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight() + 5, BoxEndX - ChanNameX, g_Locale->getText (gotTime ? (showButtonBar ? LOCALE_INFOVIEWER_EPGWAIT : LOCALE_INFOVIEWER_EPGNOTLOAD) : LOCALE_INFOVIEWER_WAITTIME), COL_INFOBAR, 0, true);	// UTF-8
	} 
	else 
	{
		show_Data();
	}
		
#if !defined USE_OPENGL
	frameBuffer->blit();
#endif	

#if ENABLE_LCD
	showLcdPercentOver();
#endif	

	if ((g_RemoteControl->current_channel_id == channel_id) && !(((info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) && (info_CurrentNext.flags & (CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::has_no_current))) || (info_CurrentNext.flags & CSectionsdClient::epgflags::not_broadcast))) 
	{
		g_Sectionsd->setServiceChanged (channel_id & 0xFFFFFFFFFFFFULL, true);
	}
	
	// radiotext
#if ENABLE_RADIOTEXT	
	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_radio)
	{
		if ((g_settings.radiotext_enable) && (!recordModeActive) && (!calledFromNumZap))
			showRadiotext();
		else
			showIcon_RadioText(false);
	}
#endif	

	// loop msg
	neutrino_msg_t msg;
	neutrino_msg_data_t data;

	CNeutrinoApp * neutrino = CNeutrinoApp::getInstance();

	if (!calledFromNumZap) 
	{

		bool hideIt = true;
		virtual_zap_mode = false;
		unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd (g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR]);

		int res = messages_return::none;

		while (!(res & (messages_return::cancel_info | messages_return::cancel_all))) 
		{
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);

			if ( msg == CRCInput::RC_sat || msg == CRCInput::RC_favorites)
			{
				g_RCInput->postMsg (msg, 0);
				res = messages_return::cancel_info;
			}
			else if ( msg == CRCInput::RC_info )
			{
				if (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_iptv)
					webtv->showFileInfoWebTV(webtv->getTunedChannel());
				else
					g_RCInput->postMsg (NeutrinoMessages::SHOW_EPG, 0);
				
				res = messages_return::cancel_info;
			} 
			else if ((msg == CRCInput::RC_ok) || (msg == CRCInput::RC_home) || (msg == CRCInput::RC_timeout)) 
			{
				res = messages_return::cancel_info;
			} 			
			else if ( (msg == NeutrinoMessages::EVT_TIMER) && (data == sec_timer_id) )
			{
				showSNR();
				
				paintTime(show_dot, false);
				showRecordIcon (show_dot);
				show_dot = !show_dot;
				
				// radiotext
#if ENABLE_RADIOTEXT				
				if ((g_settings.radiotext_enable) && (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_radio))
					showRadiotext();
#endif				

				showIcon_16_9();
				
				if ( is_visible && showButtonBar ) 
					showIcon_Resolution();
			} 
			else if ( g_settings.virtual_zap_mode && ((msg == CRCInput::RC_right) || msg == CRCInput::RC_left )) 
			{
				virtual_zap_mode = true;
				res = messages_return::cancel_all;
				hideIt = true;
			} 
			else if ( !timeshift ) 
			{
				if ((msg == (neutrino_msg_t) g_settings.key_quickzap_up) || (msg == (neutrino_msg_t) g_settings.key_quickzap_down) || (msg == CRCInput::RC_0) || (msg == NeutrinoMessages::SHOW_INFOBAR)) 
				{
					// radiotext
#if ENABLE_RADIOTEXT					
					if ((g_settings.radiotext_enable) && (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_radio))
						hideIt =  true;
					else
#endif					  
						hideIt = false;
					
					g_RCInput->postMsg (msg, data);
					res = messages_return::cancel_info;
				} 
				else if (msg == NeutrinoMessages::EVT_TIMESET) 
				{
					// Handle anyway!
					neutrino->handleMsg (msg, data);
					g_RCInput->postMsg (NeutrinoMessages::SHOW_INFOBAR, 0);
					hideIt = false;
					res = messages_return::cancel_all;
				} 
				else 
				{
					if (msg == CRCInput::RC_standby) 
					{
						g_RCInput->killTimer (sec_timer_id);
					}
					
					res = neutrino->handleMsg (msg, data);
					
					if (res & messages_return::unhandled) 
					{
						// raus hier und im Hauptfenster behandeln...
						g_RCInput->postMsg (msg, data);
						res = messages_return::cancel_info;
					}
				}
			}
				
#if !defined USE_OPENGL
			frameBuffer->blit();
#endif			
		}

		if (hideIt)
			killTitle();
		
		g_RCInput->killTimer(sec_timer_id);
		sec_timer_id = 0;

		if (virtual_zap_mode)
			CNeutrinoApp::getInstance()->channelList->virtual_zap_mode(msg == CRCInput::RC_right);

	}
}

//showMovieInfo
extern bool isMovieBrowser;
extern bool isVlc;
extern bool cdDvd;
extern bool isWebTV;
extern bool isDVD;
extern bool isBlueRay;
extern bool isURL;

void CInfoViewer::showMovieInfo(const std::string &g_file_epg, const std::string &g_file_epg1, const int file_prozent, const int duration, const unsigned int ac3state, const int speed, const int playstate, bool lshow)
{
	m_visible = true;
	
	// get dimension
	BoxEndX = g_settings.screen_EndX - 10;
	BoxStartX = g_settings.screen_StartX + 10;
	BoxHeight = MOVIE_TIMEBAR_H * 3;
	BoxStartY = g_settings.screen_EndY - BoxHeight - 10;
	BoxEndY = BoxStartY + BoxHeight;
	BoxWidth = BoxEndX - BoxStartX;
	
	// init progressbar
	moviescale = new CProgressBar( BoxWidth - 15, TIMESCALE_BAR_HEIGHT, 40, 100, 70, true );
	
	moviescale->reset();
	
	// paint shadow
	frameBuffer->paintBoxRel(BoxStartX + SHADOW_OFFSET, BoxStartY + SHADOW_OFFSET, BoxWidth, BoxHeight, COL_INFOBAR_SHADOW_PLUS_0, RADIUS_MID, CORNER_BOTH );
		
	// paint info box
	frameBuffer->paintBoxRel(BoxStartX, BoxStartY, BoxWidth, BoxHeight, COL_INFOBAR_PLUS_0, RADIUS_MID, CORNER_BOTH); 
		
	// timescale bg
	frameBuffer->paintBoxRel(BoxStartX + 10, BoxStartY + 15, BoxWidth - 20, 6, COL_INFOBAR_SHADOW_PLUS_1 ); 
		
	// bottum bar
	frameBuffer->paintBoxRel(BoxStartX, BoxStartY + (BoxHeight - 20), BoxWidth, 20, COL_INFOBAR_SHADOW_PLUS_1,  RADIUS_MID, CORNER_BOTTOM); 
	
	// mp icon
	int m_icon_w = 0;
	int m_icon_h = 0;
	
	std::string IconName = DATADIR "/neutrino/icons/" NEUTRINO_ICON_MP ".png";
	
	if(!access(IconName.c_str(), F_OK))
	{
		frameBuffer->getIconSize((CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_iptv)? NEUTRINO_ICON_WEBTV : NEUTRINO_ICON_MP, &m_icon_w, &m_icon_h);

		int m_icon_x = BoxStartX + 5;
		int m_icon_y = BoxStartY + (BoxHeight - m_icon_h) / 2;
		
		frameBuffer->paintIcon((CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_iptv)? NEUTRINO_ICON_WEBTV : NEUTRINO_ICON_MP, m_icon_x, m_icon_y);
	}
	
	// paint buttons
	// red
	// movie info
	int icon_w, icon_h;
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_RED, &icon_w, &icon_h);
	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_RED, BoxStartX + RIGHT_OFFSET, BoxStartY + (BoxHeight - 20) + (20 - icon_h)/2);

	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString( BoxStartX + RIGHT_OFFSET + icon_w + 2, BoxEndY + 2, BoxWidth/5, (char *)"Info", (COL_INFOBAR_SHADOW + 1), 0, true); // UTF-8
		
	// green
	// audio
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_GREEN, &icon_w, &icon_h);
	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_GREEN, BoxStartX + BoxWidth/5, BoxStartY + BoxHeight - 20 + (20 - icon_h)/2);
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString( BoxStartX + (BoxWidth/5) + icon_w + 2, BoxEndY + 2, BoxWidth/5, g_Locale->getText(LOCALE_INFOVIEWER_LANGUAGES), (COL_INFOBAR_SHADOW + 1), 0, true); // UTF-8
		
	// yellow	
	// help
	if (CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_iptv)
	{
		frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_YELLOW, &icon_w, &icon_h);
		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW, BoxStartX + (BoxWidth/5)*2, BoxStartY + (BoxHeight - 20) + (20 - icon_h)/2);
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString( BoxStartX + (BoxWidth/5)*2 + icon_w + 2, BoxEndY + 2, BoxWidth/5, (char *)"help", (COL_INFOBAR_SHADOW * 1), 0, true); // UTF-8
	}
	
	// blue
	// bookmark
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_BLUE, &icon_w, &icon_h);
	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_BLUE, BoxStartX + (BoxWidth/5)*3, BoxStartY + (BoxHeight - 20) + (20 - icon_h)/2);
	
	if (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_iptv)
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString( BoxStartX + (BoxWidth/5)*3 + icon_w + 2, BoxEndY + 2, BoxWidth/5, g_Locale->getText(LOCALE_INFOVIEWER_FEATURES), (COL_INFOBAR_SHADOW + 1), 0, true); // UTF-8
	else if(isMovieBrowser)
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString( BoxStartX + (BoxWidth/5)*3 + icon_w + 2, BoxEndY + 2, BoxWidth/5, g_Locale->getText(LOCALE_MOVIEPLAYER_BOOKMARK), (COL_INFOBAR_SHADOW + 1), 0, true); // UTF-8
		
	// ac3
	int icon_w_ac3, icon_h_ac3;
	frameBuffer->getIconSize(NEUTRINO_ICON_DD, &icon_w_ac3, &icon_h_ac3);
	frameBuffer->paintIcon( (ac3state == CInfoViewer::AC3_ACTIVE)?NEUTRINO_ICON_DD : NEUTRINO_ICON_DD_GREY, BoxStartX + BoxWidth - LEFT_OFFSET - icon_w_ac3, BoxStartY + BoxHeight - 20 + (20 - icon_h_ac3)/2);
		
	// 4:3/16:9
	const char * aspect_icon = NEUTRINO_ICON_16_9_GREY;
				
	if(g_settings.video_Ratio == ASPECTRATIO_169)
		aspect_icon = NEUTRINO_ICON_16_9;
	
	int icon_w_asp, icon_h_asp;
	frameBuffer->getIconSize(aspect_icon, &icon_w_asp, &icon_h_asp);
	frameBuffer->paintIcon(aspect_icon, BoxStartX + BoxWidth - LEFT_OFFSET - icon_w_ac3 - 2 - icon_w_asp, BoxStartY + BoxHeight - 20 + (20 - icon_h_asp)/2);
	
	/* mp keys */
	if (CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_iptv)
	{
		frameBuffer->getIconSize("ico_mp_rewind", &icon_w, &icon_h);
		frameBuffer->paintIcon("ico_mp_rewind", BoxStartX + BoxWidth - LEFT_OFFSET - icon_w_ac3 - 2 - icon_w_asp - 2 - 5*icon_w, BoxStartY + BoxHeight - 20 + (20 - icon_h)/2);
		frameBuffer->paintIcon("ico_mp_play", BoxStartX + BoxWidth - LEFT_OFFSET - icon_w_ac3 - 2 - icon_w_asp - 2 - 4*icon_w, BoxStartY + BoxHeight - 20 + (20 - icon_h)/2);
		frameBuffer->paintIcon("ico_mp_pause", BoxStartX + BoxWidth - LEFT_OFFSET - icon_w_ac3 - 2 - icon_w_asp - 2 - 3*icon_w, BoxStartY + BoxHeight - 20 + (20 - icon_h)/2);
		frameBuffer->paintIcon("ico_mp_stop", BoxStartX + BoxWidth - LEFT_OFFSET - icon_w_ac3 - 2 - icon_w_asp - 2 - 2*icon_w, BoxStartY + BoxHeight - 20 + (20 - icon_h)/2);
		frameBuffer->paintIcon("ico_mp_forward", BoxStartX + BoxWidth - LEFT_OFFSET - icon_w_ac3 - 2 - icon_w_asp - 2 - icon_w, BoxStartY + BoxHeight - 20 + (20 - icon_h)/2);
	}
		
	//playstate
	const char *icon = "mp_play";
		
	switch(playstate)
	{
		case CMoviePlayerGui::PAUSE: icon = "mp_pause"; break;
		case CMoviePlayerGui::PLAY: icon = "mp_play"; break;
		case CMoviePlayerGui::REW: icon = "mp_b-skip"; break;
		case CMoviePlayerGui::FF: icon = "mp_f-skip"; break;
		case CMoviePlayerGui::SOFTRESET: break;
		case CMoviePlayerGui::SLOW: break;
		case CMoviePlayerGui::STOPPED: break;
	}

	// get icon size	
	frameBuffer->getIconSize(icon, &icon_w, &icon_h);

	//int icon_x = BoxStartX + 60 + 5;
	int icon_x = BoxStartX + 5 + m_icon_w + 10;
	int icon_y = BoxStartY + (BoxHeight - icon_h) / 2;
		

	frameBuffer->paintIcon(icon, icon_x, icon_y);
		
	// paint speed
	char strSpeed[4];
	if( playstate == CMoviePlayerGui::FF || playstate == CMoviePlayerGui::REW )
	{
		sprintf(strSpeed, "%d", speed);
			
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_NUMBER]->RenderString(icon_x + icon_w + 5, BoxStartY + (BoxHeight/3)*2, BoxWidth/5, strSpeed, COL_INFOBAR ); // UTF-8
	}
	
	time_t tDisplayTime = duration/1000;
	char cDisplayTime[8 + 1];
	strftime(cDisplayTime, 9, "%T", gmtime(&tDisplayTime));
	
	int durationWidth = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth("00:00:00");;
	int durationTextPos = BoxEndX - durationWidth - 15;
		
	int speedWidth = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth("-8");
		
	int InfoStartX = BoxStartX + 5 + m_icon_w + 10 + icon_w + 5 + speedWidth + 20;
	int InfoWidth = durationTextPos - InfoStartX;
		
	//Title 1
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (InfoStartX, BoxStartY + BoxHeight/2 - 5, InfoWidth, g_file_epg, COL_INFOBAR, 0, true);

	//Title2
	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (InfoStartX, BoxStartY + BoxHeight/2 + 25, InfoWidth, g_file_epg1, COL_INFOBAR, 0, true);

	// duration
	if( (CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_iptv) && !isVlc && lshow )
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(durationTextPos, BoxStartY + BoxHeight/2 - 5, durationWidth, cDisplayTime, COL_INFOBAR);
	
	// progressbar
	moviescale->paint(BoxStartX + 10, BoxStartY + 15, file_prozent);
	
#if !defined USE_OPENGL
	frameBuffer->blit();
#endif		
	
	// loop msg
	neutrino_msg_t msg;
	neutrino_msg_data_t data;
	bool hideIt = true;
	CNeutrinoApp * neutrino = CNeutrinoApp::getInstance();

	unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd (g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] == 0 ? 0xFFFF : g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR]);

	int res = messages_return::none;

	while (!(res & (messages_return::cancel_info | messages_return::cancel_all))) 
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd);
		
		if ((msg == CRCInput::RC_ok) || (msg == CRCInput::RC_home) || (msg == CRCInput::RC_timeout)) 
		{
			res = messages_return::cancel_info;
		} 
		else if(msg == CRCInput::RC_info)
		{
			if (CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_iptv)
			{
				killTitle();
				webtv->showFileInfoWebTV(webtv->getTunedChannel());
			}
		}
		else 
		{		
			res = neutrino->handleMsg (msg, data);
					
			if (res & messages_return::unhandled) 
			{
				// raus hier und im Hauptfenster behandeln...
				g_RCInput->postMsg (msg, data);
				res = messages_return::cancel_info;
			}
		}
	
#if !defined USE_OPENGL
		frameBuffer->blit();
#endif			
	}
	
	if (hideIt)
		killTitle();
}

void CInfoViewer::showSubchan()
{
  	CNeutrinoApp *neutrino = CNeutrinoApp::getInstance();

  	std::string subChannelName;	// holds the name of the subchannel/audio channel
  	int subchannel = 0;		// holds the channel index

  	if (!(g_RemoteControl->subChannels.empty ())) 
	{
		// get info for nvod/subchannel
		subchannel = g_RemoteControl->selected_subchannel;
		if (g_RemoteControl->selected_subchannel >= 0)
	  		subChannelName = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].subservice_name;
  	} 
	else if (g_RemoteControl->current_PIDs.APIDs.size () > 1 ) 
	{
		// get info for audio channel
		subchannel = g_RemoteControl->current_PIDs.PIDs.selected_apid;
		subChannelName = g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].desc;
  	}

  	if (!(subChannelName.empty ())) 
	{
		char text[100];
		sprintf (text, "%d - %s", subchannel, subChannelName.c_str ());

		int dx = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth (text) + 20;
		int dy = 25;

		if (g_RemoteControl->director_mode) 
		{
	  		int w = 20 + g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getRenderWidth (g_Locale->getText (LOCALE_NVODSELECTOR_DIRECTORMODE), true) + 20;	// UTF-8
	  		if (w > dx)
				dx = w;
	  			dy = dy * 2;
		} 
		else
	  		dy = dy + 5;

		int x = 0, y = 0;
		
		if (g_settings.infobar_subchan_disp_pos == 0) 
		{
	  		// Rechts-Oben
	  		x = g_settings.screen_EndX - dx - 10;
	  		y = g_settings.screen_StartY + 10;
		} 
		else if (g_settings.infobar_subchan_disp_pos == 1) 
		{
	  		// Links-Oben
	  		x = g_settings.screen_StartX + 10;
	  		y = g_settings.screen_StartY + 10;
		} 
		else if (g_settings.infobar_subchan_disp_pos == 2) 
		{
	  		// Links-Unten
	  		x = g_settings.screen_StartX + 10;
	  		y = g_settings.screen_EndY - dy - 10;
		} 
		else if (g_settings.infobar_subchan_disp_pos == 3) 
		{
	  		// Rechts-Unten
	  		x = g_settings.screen_EndX - dx - 10;
	  		y = g_settings.screen_EndY - dy - 10;
		}

		fb_pixel_t pixbuf[(dx + 2 * borderwidth) * (dy + 2 * borderwidth)];
		frameBuffer->SaveScreen (x - borderwidth, y - borderwidth, dx + 2 * borderwidth, dy + 2 * borderwidth, pixbuf);		

		// clear border
		frameBuffer->paintBackgroundBoxRel(x - borderwidth, y - borderwidth, dx + 2 * borderwidth, borderwidth);
		frameBuffer->paintBackgroundBoxRel(x - borderwidth, y + dy, dx + 2 * borderwidth, borderwidth);
		frameBuffer->paintBackgroundBoxRel(x - borderwidth, y, borderwidth, dy);
		frameBuffer->paintBackgroundBoxRel(x + dx, y, borderwidth, dy);

		frameBuffer->paintBoxRel(x, y, dx, dy, COL_MENUCONTENT_PLUS_0);
		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (x + 10, y + 30, dx - 20, text, COL_MENUCONTENT);

		if (g_RemoteControl->director_mode) 
		{
			int icon_w;
			int icon_h;
		
			frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_YELLOW, &icon_w, &icon_h);
	
	  		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW, x + 8, y + dy - 20);
	  		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString (x + 30 - icon_w + icon_w, y + dy - 2, dx - 40, g_Locale->getText (LOCALE_NVODSELECTOR_DIRECTORMODE), COL_MENUCONTENT, 0, true);	// UTF-8
		}
		
#if !defined USE_OPENGL
		frameBuffer->blit();
#endif		

		unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd(2);
		int res = messages_return::none;

		neutrino_msg_t msg;
		neutrino_msg_data_t data;

		while (!(res & (messages_return::cancel_info | messages_return::cancel_all))) 
		{
	  		g_RCInput->getMsgAbsoluteTimeout (&msg, &data, &timeoutEnd);

	  		if (msg == CRCInput::RC_timeout) 
			{
				res = messages_return::cancel_info;
	  		} 
			else 
			{
				res = neutrino->handleMsg(msg, data);

				if (res & messages_return::unhandled) 
				{
		  			// raus hier und im Hauptfenster behandeln...
		  			g_RCInput->postMsg (msg, data);
		  			res = messages_return::cancel_info;
				}
	  		}
		}

		frameBuffer->RestoreScreen(x - borderwidth, y - borderwidth, dx + 2 * borderwidth, dy + 2 * borderwidth, pixbuf);
		
#if !defined USE_OPENGL
		frameBuffer->blit();
#endif		
  		
	} 
	else 
	{
		g_RCInput->postMsg(NeutrinoMessages::SHOW_INFOBAR, 0);
  	}
}

// radiotext
#if ENABLE_RADIOTEXT
void CInfoViewer::showIcon_RadioText(bool rt_available) const
{
	if (showButtonBar)
	{
		int mode = CNeutrinoApp::getInstance()->getMode();
		std::string rt_icon = NEUTRINO_ICON_RADIOTEXTOFF;
		
		if ((!virtual_zap_mode) && (!recordModeActive) && (mode == NeutrinoMessages::mode_radio))
		{
			if (g_settings.radiotext_enable)
			{
				rt_icon = rt_available ? NEUTRINO_ICON_RADIOTEXTGET : NEUTRINO_ICON_RADIOTEXTWAIT;
			}
		}
		
		frameBuffer->paintIcon(rt_icon, BoxEndX - (LEFT_OFFSET + icon_w_subt + 2 + icon_w_vtxt + 2 + icon_w_dd + 2 + icon_w_aspect + 2 + icon_w_sd + 2 + icon_w_reso + 2 + icon_w_ca + 2 + icon_w_rt), BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h_rt)/2);
	}
}
#endif

void CInfoViewer::showIcon_16_9()
{			
	const char * aspect_icon = NEUTRINO_ICON_16_9_GREY;
			
	if(videoDecoder->getAspectRatio() == ASPECTRATIO_169)
		aspect_icon = NEUTRINO_ICON_16_9;
	
	if (is_visible)
		frameBuffer->paintIcon(aspect_icon, BoxEndX - (LEFT_OFFSET + icon_w_subt + 2 + icon_w_vtxt + 2 + icon_w_dd + 2 + icon_w_aspect), BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h_aspect)/2 );
}

void CInfoViewer::showIcon_VTXT() const
{
	frameBuffer->paintIcon((g_RemoteControl->current_PIDs.PIDs.vtxtpid != 0) ? NEUTRINO_ICON_VTXT : NEUTRINO_ICON_VTXT_GREY, BoxEndX - (LEFT_OFFSET + icon_w_subt + 2 + icon_w_vtxt), BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h_vtxt)/2 );
}

void CInfoViewer::showIcon_Resolution() const
{
	int xres, yres, framerate;
	const char *icon_name = NULL;
	const char *icon_name_res = NULL;
	
	if (is_visible)
	{
		videoDecoder->getPictureInfo(xres, yres, framerate);
		
		// show sd/hd icon on infobar	
		switch (yres) 
		{
			case 1920:
			case 1440:
			case 1280:
			case 1088:
			case 1080:
			case 720:
				icon_name_res = NEUTRINO_ICON_RESOLUTION_HD;
				//test
				CVFD::getInstance()->ShowIcon(VFD_ICON_HD, true); //FIXME: remove this to remotecontrol
				break;
				
			case 704:
			case 576:
			case 544:
			case 528:
			case 480:
			case 382:
			case 352:
			case 288:
				icon_name_res = NEUTRINO_ICON_RESOLUTION_SD;
				CVFD::getInstance()->ShowIcon(VFD_ICON_HD, false);
				break;
				
			default:
				icon_name_res = NEUTRINO_ICON_RESOLUTION_000;
				CVFD::getInstance()->ShowIcon(VFD_ICON_HD, false);
				break;	
		}
		
		frameBuffer->paintBoxRel(BoxEndX - (LEFT_OFFSET + icon_w_subt + 2 + icon_w_vtxt + 2 + icon_w_dd + 2 + icon_w_aspect + 2 + icon_w_sd), BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h_sd)/2, icon_w_sd, icon_h_sd, COL_INFOBAR_BUTTONS_BACKGROUND);
		//if(icon_name_res != NEUTRINO_ICON_RESOLUTION_000)
		frameBuffer->paintIcon(icon_name_res, BoxEndX - (LEFT_OFFSET + icon_w_subt + 2 + icon_w_vtxt + 2 + icon_w_dd + 2 + icon_w_aspect + 2 + icon_w_sd), BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h_sd)/2 );
		
		// resolution
		switch (yres) 
		{
			case 1920:
				icon_name = NEUTRINO_ICON_RESOLUTION_1920;
				break;
				
			case 1088:
			case 1080:
				icon_name = NEUTRINO_ICON_RESOLUTION_1080;
				break;
				
			case 1440:
				icon_name = NEUTRINO_ICON_RESOLUTION_1440;
				break;
				
			case 1280:
				icon_name = NEUTRINO_ICON_RESOLUTION_1280;
				break;
				
			case 720:
				icon_name = NEUTRINO_ICON_RESOLUTION_720;
				break;
				
			case 704:
				icon_name = NEUTRINO_ICON_RESOLUTION_704;
				break;
				
			case 576:
				icon_name = NEUTRINO_ICON_RESOLUTION_576;
				break;
				
			case 544:
				icon_name = NEUTRINO_ICON_RESOLUTION_544;
				break;
				
			case 528:
				icon_name = NEUTRINO_ICON_RESOLUTION_528;
				break;
				
			case 480:
				icon_name = NEUTRINO_ICON_RESOLUTION_480;
				break;
				
			case 382:
				icon_name = NEUTRINO_ICON_RESOLUTION_382;
				break;
				
			case 352:
				icon_name = NEUTRINO_ICON_RESOLUTION_352;
				break;
				
			case 288:
				icon_name = NEUTRINO_ICON_RESOLUTION_288;
				break;
				
			default:
				icon_name = NEUTRINO_ICON_RESOLUTION_000;
				break;
		}
	
		frameBuffer->paintBoxRel(BoxEndX - (LEFT_OFFSET + icon_w_subt + 2 + icon_w_vtxt + 2 + icon_w_dd + 2 + icon_w_aspect + 2 + icon_w_sd + 2 + icon_w_reso), BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h_reso)/2, icon_w_reso, icon_h_reso, COL_INFOBAR_BUTTONS_BACKGROUND);
		//if(icon_name != NEUTRINO_ICON_RESOLUTION_000)
		frameBuffer->paintIcon(icon_name, BoxEndX - (LEFT_OFFSET + icon_w_subt + 2 + icon_w_vtxt + 2 + icon_w_dd + 2 + icon_w_aspect + 2 + icon_w_sd + 2 + icon_w_reso), BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h_reso)/2 );
	}
}

// dvbsub icon
void CInfoViewer::showIcon_SubT() const
{
        bool have_sub = false;
	CZapitChannel * cc = CNeutrinoApp::getInstance()->channelList->getChannel(CNeutrinoApp::getInstance()->channelList->getActiveChannelNumber());
	if(cc && cc->getSubtitleCount())
		have_sub = true;

        frameBuffer->paintIcon(have_sub ? NEUTRINO_ICON_SUBT : NEUTRINO_ICON_SUBT_GREY, BoxEndX - (LEFT_OFFSET + icon_w_subt), BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h_subt)/2 );
}

void CInfoViewer::showFailure()
{
  	ShowHintUTF(LOCALE_MESSAGEBOX_ERROR, g_Locale->getText (LOCALE_INFOVIEWER_NOTAVAILABLE), 430);	// UTF-8
}

void CInfoViewer::showMotorMoving (int duration)
{
	char text[256];
	char buffer[10];
	
	sprintf (buffer, "%d", duration);
	strcpy (text, g_Locale->getText (LOCALE_INFOVIEWER_MOTOR_MOVING));
	strcat (text, " (");
	strcat (text, buffer);
	strcat (text, " s)");
	
	ShowHintUTF(LOCALE_MESSAGEBOX_INFO, text, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth (text, true) + 10, duration);	// UTF-8
}

// radiotext
#if ENABLE_RADIOTEXT
void CInfoViewer::killRadiotext()
{
	frameBuffer->paintBackgroundBox(rt_x, rt_y, rt_w, rt_h);
	
#if !defined USE_OPENGL
	frameBuffer->blit();
#endif
}

void CInfoViewer::showRadiotext()
{
	char stext[3][100];
	int yoff = 8, ii = 0;
	bool RTisIsUTF = false;

	if (g_Radiotext == NULL) 
		return;
	
	showIcon_RadioText(g_Radiotext->haveRadiotext());

	if (g_Radiotext->S_RtOsd) 
	{
		// dimensions of radiotext window
		rt_dx = BoxEndX - BoxStartX;
		rt_dy = 25;
		rt_x = BoxStartX;
		rt_y = g_settings.screen_StartY + 10;
		rt_h = rt_y + 7 + rt_dy*(g_Radiotext->S_RtOsdRows+1)+SHADOW_OFFSET;
		rt_w = rt_x+rt_dx+SHADOW_OFFSET;
		
		int lines = 0;
		for (int i = 0; i < g_Radiotext->S_RtOsdRows; i++) 
		{
			if (g_Radiotext->RT_Text[i][0] != '\0') lines++;
		}
		
		if (lines == 0)
		{
			frameBuffer->paintBackgroundBox(rt_x, rt_y, rt_w, rt_h);
		
#if !defined USE_OPENGL
			frameBuffer->blit();
#endif
		}

		if (g_Radiotext->RT_MsgShow) 
		{
			if (g_Radiotext->S_RtOsdTitle == 1) 
			{
				// Title
				if ((lines) || (g_Radiotext->RT_PTY !=0)) 
				{
					sprintf(stext[0], g_Radiotext->RT_PTY == 0 ? "%s %s%s" : "%s (%s)%s", tr("Radiotext"), g_Radiotext->RT_PTY == 0 ? g_Radiotext->RDS_PTYN : g_Radiotext->ptynr2string(g_Radiotext->RT_PTY), ":");
					
					// shadow
					frameBuffer->paintBoxRel(rt_x + SHADOW_OFFSET, rt_y + SHADOW_OFFSET, rt_dx, rt_dy, COL_INFOBAR_SHADOW_PLUS_0, RADIUS_MID, CORNER_TOP);
					frameBuffer->paintBoxRel(rt_x, rt_y, rt_dx, rt_dy, COL_INFOBAR_PLUS_0, RADIUS_MID, CORNER_TOP);
					g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rt_x + 10, rt_y + 30, rt_dx - 20, stext[0], COL_INFOBAR, 0, RTisIsUTF); // UTF-8
					
#if !defined USE_OPENGL
					frameBuffer->blit();
#endif
				}
				yoff = 17;
				ii = 1;
			}
			
			// Body
			if (lines) 
			{
				frameBuffer->paintBoxRel(rt_x + SHADOW_OFFSET, rt_y+rt_dy+SHADOW_OFFSET, rt_dx, 7+rt_dy* g_Radiotext->S_RtOsdRows, COL_INFOBAR_SHADOW_PLUS_0, RADIUS_MID, CORNER_BOTTOM);
				frameBuffer->paintBoxRel(rt_x, rt_y+rt_dy, rt_dx, 7+rt_dy* g_Radiotext->S_RtOsdRows, COL_INFOBAR_PLUS_0, RADIUS_MID, CORNER_BOTTOM);

				// RT-Text roundloop
				int ind = (g_Radiotext->RT_Index == 0) ? g_Radiotext->S_RtOsdRows - 1 : g_Radiotext->RT_Index - 1;
				int rts_x = rt_x + 10;
				int rts_y = rt_y + 30;
				int rts_dx = rt_dx - 20;
				
				if (g_Radiotext->S_RtOsdLoop == 1) 
				{ 
					// latest bottom
					for (int i = ind+1; i < g_Radiotext->S_RtOsdRows; i++)
						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rts_x, rts_y + (ii++)*rt_dy, rts_dx, g_Radiotext->RT_Text[i], COL_INFOBAR, 0, RTisIsUTF); // UTF-8
					for (int i = 0; i <= ind; i++)
						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rts_x, rts_y + (ii++)*rt_dy, rts_dx, g_Radiotext->RT_Text[i], COL_INFOBAR, 0, RTisIsUTF); // UTF-8
				}
				else 
				{ 
					// latest top
					for (int i = ind; i >= 0; i--)
						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rts_x, rts_y + (ii++)*rt_dy, rts_dx, g_Radiotext->RT_Text[i], COL_INFOBAR, 0, RTisIsUTF); // UTF-8
					for (int i = g_Radiotext->S_RtOsdRows-1; i > ind; i--)
						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(rts_x, rts_y + (ii++)*rt_dy, rts_dx, g_Radiotext->RT_Text[i], COL_INFOBAR, 0, RTisIsUTF); // UTF-8
				}
				
#if !defined USE_OPENGL
					frameBuffer->blit();
#endif				
			}
		}
	}
	
	g_Radiotext->RT_MsgShow = false;
}
#endif

int CInfoViewer::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{

 	if ((msg == NeutrinoMessages::EVT_CURRENTNEXT_EPG) || (msg == NeutrinoMessages::EVT_NEXTPROGRAM)) 
	{
		if ((*(t_channel_id *) data) == (channel_id & 0xFFFFFFFFFFFFULL)) 
		{
	  		getEPG (*(t_channel_id *) data, info_CurrentNext);
	  		if ( is_visible )
				show_Data(true);
			
#if ENABLE_LCD			
	  		showLcdPercentOver();
#endif			
		}

		return messages_return::handled;
  	} 
	else if (msg == NeutrinoMessages::EVT_TIMER) 
	{
		if (data == lcdUpdateTimer) 
		{
			if ( is_visible )
				show_Data( true );

#if ENABLE_LCD
	  		showLcdPercentOver();
#endif			

	  		return messages_return::handled;
		}
		else if (data == sec_timer_id) 
		{
			showSNR();

	  		return messages_return::handled;
		}
  	} 
	else if (msg == NeutrinoMessages::EVT_RECORDMODE) 
	{
		recordModeActive = data;
		if(is_visible) 
			showRecordIcon(true);		
  	} 
	else if (msg == NeutrinoMessages::EVT_ZAP_GOTAPIDS) 
	{
		if ((*(t_channel_id *) data) == channel_id) 
		{
	  		if ( is_visible && showButtonBar )
				showButton_Audio();
			
			// radiotext
#if ENABLE_RADIOTEXT			
			//if (g_settings.radiotext_enable && g_Radiotext && ((CNeutrinoApp::getInstance()->getMode()) == NeutrinoMessages::mode_radio))
			//	g_Radiotext->setPid(g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].pid);
#endif			
		}
		return messages_return::handled;
  	} 
	else if (msg == NeutrinoMessages::EVT_ZAP_GOTPIDS) 
	{
		if ((*(t_channel_id *) data) == channel_id) 
		{
	  		if ( is_visible && showButtonBar ) 
			{
				showIcon_VTXT();
				showIcon_SubT();
				showIcon_CA_Status(0);
				showIcon_Resolution();
	  		}
		}
		return messages_return::handled;
  	} 
	else if (msg == NeutrinoMessages::EVT_ZAP_GOT_SUBSERVICES) 
	{
		if ((*(t_channel_id *) data) == channel_id) 
		{
	  		if ( is_visible && showButtonBar )
				showButton_SubServices();
		}
		return messages_return::handled;
  	} 
  	else if ((msg == NeutrinoMessages::EVT_ZAP_COMPLETE) || (msg == NeutrinoMessages::EVT_ZAP_ISNVOD))
	{
		chanready = 1;
		showSNR();
		
		if ( is_visible && showButtonBar ) 
			showIcon_Resolution();
		
		channel_id = (*(t_channel_id *)data);
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_ZAP_SUB_COMPLETE) 
	{
		chanready = 1;
		showSNR();
		
		if ( is_visible && showButtonBar ) 
			showIcon_Resolution();

		//if ((*(t_channel_id *)data) == channel_id)
		{
	  		if ( is_visible && showButtonBar && (!g_RemoteControl->are_subchannels))
				show_Data (true);
		}

#if ENABLE_LCD
		showLcdPercentOver();
#endif		

		return messages_return::handled;
  	} 
	else if (msg == NeutrinoMessages::EVT_ZAP_SUB_FAILED) 
	{
		chanready = 1;

		showSNR();
		
		if ( is_visible && showButtonBar ) 
			showIcon_Resolution();

		// show failure..!
		if (CVFD::getInstance()->is4digits)
			CVFD::getInstance()->LCDshowText(g_RemoteControl->getCurrentChannelNumber());
		else
			CVFD::getInstance()->showServicename("(" + g_RemoteControl->getCurrentChannelName() + ')');
		
		dprintf(DEBUG_NORMAL, "CInfoViewer::handleMsg: zap failed!\n");
		showFailure();

#if ENABLE_LCD		
		CVFD::getInstance()->showPercentOver(255);
#endif		

		return messages_return::handled;
  	} 
	else if (msg == NeutrinoMessages::EVT_ZAP_FAILED) 
	{
		chanready = 1;

		showSNR();
		
		if ( is_visible && showButtonBar ) 
			showIcon_Resolution();

		if ((*(t_channel_id *) data) == channel_id) 
		{
	  		// show failure..!
			if (CVFD::getInstance()->is4digits)
				CVFD::getInstance()->LCDshowText(g_RemoteControl->getCurrentChannelNumber());
			else
				CVFD::getInstance()->showServicename ("(" + g_RemoteControl->getCurrentChannelName () + ')');
			
	  		dprintf(DEBUG_NORMAL, "CInfoViewer::handleMsg: zap failed!\n");
	  		showFailure ();

#if ENABLE_LCD			
	  		CVFD::getInstance()->showPercentOver(255);
#endif			
		}
		return messages_return::handled;
  	} 
	else if (msg == NeutrinoMessages::EVT_ZAP_MOTOR) 
	{
		chanready = 0;
		showMotorMoving (data);
		return messages_return::handled;
  	} 
	else if (msg == NeutrinoMessages::EVT_MODECHANGED) 
	{
		if ( is_visible && showButtonBar )
	  		showIcon_16_9();

		return messages_return::handled;
  	} 
	else if (msg == NeutrinoMessages::EVT_TIMESET) 
	{
		gotTime = true;
		return messages_return::handled;
  	} 
	else if (msg == NeutrinoMessages::EVT_ZAP_CA_CLEAR) 
	{
		Set_CA_Status(false);
		return messages_return::handled;
  	} 
	else if (msg == NeutrinoMessages::EVT_ZAP_CA_LOCK) 
	{
		Set_CA_Status(true);
		return messages_return::handled;
  	} 
	else if (msg == NeutrinoMessages::EVT_ZAP_CA_FTA) 
	{
		Set_CA_Status(false);
		return messages_return::handled;
  	}
	else if (msg == NeutrinoMessages::EVT_ZAP_CA_ID) 
	{
		chanready = 1;
		Set_CA_Status(data);

		showSNR();

		return messages_return::handled;
  	}

  	return messages_return::unhandled;
}

void CInfoViewer::showButton_SubServices()
{
  	if (!(g_RemoteControl->subChannels.empty ())) 
	{
		int icon_w;
		int icon_h;
		
		frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_YELLOW, &icon_w, &icon_h);
	
        	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_YELLOW, ChanInfoX + 2 + icon_w + 2 + asize + 2 + icon_w + 2 + asize + 2, BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h)/2 );
        	g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(ChanInfoX + 2 + icon_w + 2 + asize + 2 + icon_w + 2 + asize + 2 + icon_w + 2, BoxEndY, asize, g_Locale->getText((g_RemoteControl->are_subchannels) ? LOCALE_INFOVIEWER_SUBSERVICE : LOCALE_INFOVIEWER_SELECTTIME), COL_INFOBAR_BUTTONS, 0, true); // UTF-8
  	}
}

void CInfoViewer::getEPG(const t_channel_id for_channel_id, CSectionsdClient::CurrentNextInfo &info)
{
	static CSectionsdClient::CurrentNextInfo oldinfo;

	/* to clear the oldinfo for channels without epg, call getEPG() with for_channel_id = 0 */
	if (for_channel_id == 0)
	{
		oldinfo.current_uniqueKey = 0;
		return;
	}

	sectionsd_getCurrentNextServiceKey(for_channel_id & 0xFFFFFFFFFFFFULL, info);
	
	printf("CInfoViewer::getEPG: old uniqueKey %llx new %llx\n", oldinfo.current_uniqueKey, info.current_uniqueKey);

	/* of there is no EPG, send an event so that parental lock can work */
	if (info.current_uniqueKey == 0 && info.next_uniqueKey == 0) 
	{
		memcpy(&oldinfo, &info, sizeof(CSectionsdClient::CurrentNextInfo));
		char *p = new char[sizeof(t_channel_id)];
		memcpy(p, &for_channel_id, sizeof(t_channel_id));
		g_RCInput->postMsg (NeutrinoMessages::EVT_NOEPG_YET, (const neutrino_msg_data_t) p, false);
		
		return;
	}

	if (info.current_uniqueKey != oldinfo.current_uniqueKey || info.next_uniqueKey != oldinfo.next_uniqueKey)
	{
		char *p = new char[sizeof(t_channel_id)];
		memcpy(p, &for_channel_id, sizeof(t_channel_id));
		neutrino_msg_t msg;
		
		if (info.flags & (CSectionsdClient::epgflags::has_current | CSectionsdClient::epgflags::has_next))
		{
			if (info.flags & CSectionsdClient::epgflags::has_current)
				msg = NeutrinoMessages::EVT_CURRENTEPG;
			else
				msg = NeutrinoMessages::EVT_NEXTEPG;
		}
		else
			msg = NeutrinoMessages::EVT_NOEPG_YET;
		g_RCInput->postMsg(msg, (const neutrino_msg_data_t)p, false); // data is pointer to allocated memory
		memcpy(&oldinfo, &info, sizeof(CSectionsdClient::CurrentNextInfo));
	}
}

void CInfoViewer::showSNR()
{ 
  	char percent[10];
  	uint16_t ssig = 0;
	int ssnr = 0;
  	int sw = 0;
	int snr = 0;
	int sig = 0;
	int posx = 0;
	int posy = 0;
  	int barwidth = BAR_WIDTH;
	
  	if (g_settings.infobar_sat_display) 
	{
		if(is_visible)
		{
			// freq
			if (newfreq && chanready) 
			{
				char freq[20];

				newfreq = false;
				
				// get current service info
				CZapitClient::CCurrentServiceInfo si = g_Zapit->getCurrentServiceInfo();
				
				/* freq */
				if(live_fe != NULL)
				{
					if( live_fe->getInfo()->type == FE_QPSK || live_fe->getInfo()->type == FE_QAM)
					{
						sprintf (freq, "FREQ:%d.%d MHz", si.tsfrequency / 1000, si.tsfrequency % 1000);
					}
					else if( live_fe->getInfo()->type == FE_OFDM)
					{
						sprintf (freq, "FREQ:%d.%d MHz", si.tsfrequency / 1000000, si.tsfrequency % 1000);
					}
				}
				else
					sprintf (freq, "FREQ:%d.%d MHz", si.tsfrequency / 1000, si.tsfrequency % 1000);

				int SatNameHeight  = g_SignalFont->getHeight();
				freqWidth = g_SignalFont->getRenderWidth(freq);
				freqStartX = BoxStartX + ChanNumberWidth + 80;

				g_SignalFont->RenderString(freqStartX, BoxStartY + (SatNameHeight *3)/2, freqWidth, freq, COL_INFOBAR );
			
				if(live_fe != NULL)
				{
					ssig = live_fe->getSignalStrength();
					ssnr = live_fe->getSignalNoiseRatio();
				}
						
				//show aktiv tuner
				if( FrontendCount > 1 )
				{
					int Index = 0;
					
					for(int i = 0; i < FrontendCount; i++)
					{
						CFrontend * fe = getFE(i);
						
						if(live_fe != NULL)
						{
							if(fe->fenumber == live_fe->fenumber && fe->fe_adapter == live_fe->fe_adapter)
								Index = i;
						}
						else
							Index = 0;
					}
						
					char AktivTuner[255];
					
					if(live_fe != NULL)
						sprintf(AktivTuner, "T%d", (Index + 1));
					
					g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(BoxEndX - (LEFT_OFFSET + icon_w_subt + 2 + icon_w_vtxt + 2 + icon_w_dd + 2 + icon_w_aspect + 2 + icon_w_sd + 2 + icon_w_reso + 2 + icon_w_ca + 2 + icon_w_rt + 2 + TunerNumWidth), BoxEndY, TunerNumWidth, AktivTuner, COL_INFOBAR_BUTTONS, 0, true); // UTF-8
				}

				//sig = (ssig & 0xFFFF) * 100 / 65535;
				//snr = (ssnr & 0xFFFF) * 100 / 65535;
				sig = ((ssig * 100 + 0x8001) >> 16);
				snr = ((ssnr * 100 + 0x8001) >> 16);
				
				posy = BoxStartY + (SatNameHeight *3)/2;

				if (sigscale->getPercent() != sig) 
				{
					posx = freqStartX + freqWidth + 10;

					sigscale->paint(posx, BoxStartY + 15, sig);

					sprintf (percent, "SIG:%d%%S", sig);
					posx = posx + barwidth + 2;
					sw = g_SignalFont->getRenderWidth(percent);

					g_SignalFont->RenderString (posx, posy, sw, percent, COL_INFOBAR );
				}

				//SNR
				if (snrscale->getPercent() != snr) 
				{
					int snr_posx = posx + sw + 10;

					snrscale->paint(snr_posx, BoxStartY + 15, snr);

					sprintf (percent, "SNR:%d%%Q", snr);
					snr_posx = snr_posx + barwidth + 2;
					sw = g_SignalFont->getRenderWidth(percent);
					
					g_SignalFont->RenderString (snr_posx, posy, sw, percent, COL_INFOBAR );
				}
			
			}
		}
  	} 	
}

void CInfoViewer::show_Data(bool calledFromEvent)
{
  	char runningStart[10];
  	char runningRest[20];
  	char runningPercent = 0;
  	static char oldrunningPercent = 255;

  	char nextStart[10];
  	char nextDuration[10];

  	int is_nvod = false;

  	if (is_visible) 
	{
		if ((g_RemoteControl->current_channel_id == channel_id) && (g_RemoteControl->subChannels.size () > 0) && (!g_RemoteControl->are_subchannels)) 
		{
	  		is_nvod = true;
	  		info_CurrentNext.current_zeit.startzeit = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].startzeit;
	  		info_CurrentNext.current_zeit.dauer = g_RemoteControl->subChannels[g_RemoteControl->selected_subchannel].dauer;
		} 
		else 
		{
	  		if ((info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) && (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) && (showButtonBar) ) 
			{
				if ((uint) info_CurrentNext.next_zeit.startzeit < (info_CurrentNext.current_zeit.startzeit + info_CurrentNext.current_zeit.dauer)) {
		  			is_nvod = true;
				}
	  		}
		}

		time_t jetzt = time(NULL);

		if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) 
		{
	  		int seit = (jetzt - info_CurrentNext.current_zeit.startzeit + 30) / 60;
	  		int rest = (info_CurrentNext.current_zeit.dauer / 60) - seit;
			
	  		if (seit < 0) 
			{
				runningPercent = 0;
				sprintf (runningRest, "in %d min", -seit);
	  		} 
			else 
			{
				runningPercent = (unsigned) ((float) (jetzt - info_CurrentNext.current_zeit.startzeit) / (float) info_CurrentNext.current_zeit.dauer * 100.);
				if(runningPercent > 100)
					runningPercent = 100;
				sprintf (runningRest, "%d / %d min", seit, rest);
	  		}

	  		struct tm * pStartZeit = localtime(&info_CurrentNext.current_zeit.startzeit);
	  		sprintf(runningStart, "%02d:%02d", pStartZeit->tm_hour, pStartZeit->tm_min);
		} 
		else
			last_curr_id = 0;

		if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) 
		{
	  		unsigned dauer = info_CurrentNext.next_zeit.dauer / 60;
	  		sprintf (nextDuration, "%d min", dauer);
	  		struct tm *pStartZeit = localtime (&info_CurrentNext.next_zeit.startzeit);
	  		sprintf(nextStart, "%02d:%02d", pStartZeit->tm_hour, pStartZeit->tm_min);
		} 
		else
			last_next_id = 0;

		int ChanInfoY = BoxStartY + SAT_INFOBOX_HEIGHT + TIMESCALE_BAR_HEIGHT + 5 + ChanNumberHeight + 5;
		int EPGTimeWidth = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth("00:00:00"); //FIXME

		if (showButtonBar) 
		{
	  		//percent
	  		if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) 
			{
				if(!calledFromEvent || (oldrunningPercent != runningPercent)) 
				{
					oldrunningPercent = runningPercent;
				}

				// timescale position
				int posx = BoxStartX + 5;
				int posy = BoxStartY + SAT_INFOBOX_HEIGHT;
				
				timescale->paint(posx, posy, runningPercent);
	  		} 
			else 
			{
				oldrunningPercent = 255;
	  		}
	  		
	  		if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_anything) 
			{
				// red button
				// events text
				int icon_w;
				int icon_h;
		
				frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_RED, &icon_w, &icon_h);
		
				frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_RED, BoxStartX + 5, BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h)/2 );
				
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(BoxStartX + 5 + icon_w + 5, BoxEndY, asize - 5, g_Locale->getText(LOCALE_INFOVIEWER_EVENTLIST), COL_INFOBAR_BUTTONS, 0, true); // UTF-8
	  		}
		}

		// recalculate height
		int height = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getHeight();
		int xStart = BoxStartX + ChanNumberWidth + 10;

		if ((info_CurrentNext.flags & CSectionsdClient::epgflags::not_broadcast) || ((calledFromEvent) && !(info_CurrentNext.flags & (CSectionsdClient::epgflags::has_next | CSectionsdClient::epgflags::has_current)))) 
		{
	  		// no EPG available
	  		ChanInfoY += height;
			
			// refresh box
	  		frameBuffer->paintBox(ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);
			
			// noepg/waiting for time
	  		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(xStart, ChanInfoY + height, BoxEndX - (BoxStartX + ChanNumberWidth + 20), g_Locale->getText (gotTime ? LOCALE_INFOVIEWER_NOEPG : LOCALE_INFOVIEWER_WAITTIME), COL_INFOBAR, 0, true);	// UTF-8
		} 
		else 
		{
	  		// irgendein EPG gefunden
	  		int duration1Width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth(runningRest);
	  		int duration1TextPos = BoxEndX - duration1Width - LEFT_OFFSET;

	  		int duration2Width = g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->getRenderWidth(nextDuration);
	  		int duration2TextPos = BoxEndX - duration2Width - LEFT_OFFSET;

	  		if ((info_CurrentNext.flags & CSectionsdClient::epgflags::has_next) && (!(info_CurrentNext.flags & CSectionsdClient::epgflags::has_current))) 
			{
				// there are later events available - yet no current
				//refresh box
				frameBuffer->paintBox(ChanInfoX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);
				g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (xStart, ChanInfoY + height, BoxEndX - xStart, g_Locale->getText(LOCALE_INFOVIEWER_NOCURRENT), COL_COLORED_EVENTS_INFOBAR, 0, true);	// UTF-8

				// next
				ChanInfoY += height;

				if(last_next_id != info_CurrentNext.next_uniqueKey) 
				{
					// refresh box
					frameBuffer->paintBox(BoxStartX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);

					g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(BoxStartX + 10, ChanInfoY + height, EPGTimeWidth, nextStart, COL_INFOBAR );
					g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(xStart, ChanInfoY + height, duration2TextPos - xStart - 5, info_CurrentNext.next_name, COL_INFOBAR, 0, true);
					g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString(duration2TextPos, ChanInfoY + height, duration2Width, nextDuration, COL_INFOBAR );

					last_next_id = info_CurrentNext.next_uniqueKey;
				}
	  		} 
			else 
			{
				// current
		  		if(last_curr_id != info_CurrentNext.current_uniqueKey) 
				{
					// refresh box
			  		frameBuffer->paintBox(BoxStartX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);
					
			  		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (BoxStartX + 10, ChanInfoY + height, EPGTimeWidth, runningStart, COL_COLORED_EVENTS_INFOBAR);
			  		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (xStart, ChanInfoY + height, duration1TextPos - xStart - 5, info_CurrentNext.current_name, COL_COLORED_EVENTS_INFOBAR, 0, true);

			  		last_curr_id = info_CurrentNext.current_uniqueKey;
		  		}
		  		
		  		// refresh box
		  		frameBuffer->paintBox(BoxEndX - 80, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);//FIXME duration1TextPos not really good
		  		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (duration1TextPos, ChanInfoY + height, duration1Width, runningRest, COL_INFOBAR);

				// next 
				ChanInfoY += height;

				if ((!is_nvod) && (info_CurrentNext.flags & CSectionsdClient::epgflags::has_next)) 
				{
					if(last_next_id != info_CurrentNext.next_uniqueKey) 
					{
						// refresh
						frameBuffer->paintBox(BoxStartX + 10, ChanInfoY, BoxEndX, ChanInfoY + height, COL_INFOBAR_PLUS_0);

						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (BoxStartX + 10, ChanInfoY + height, EPGTimeWidth, nextStart, COL_INFOBAR);
						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (xStart, ChanInfoY + height, duration2TextPos - xStart - 5, info_CurrentNext.next_name, COL_INFOBAR, 0, true);
						g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_INFO]->RenderString (duration2TextPos, ChanInfoY + height, duration2Width, nextDuration, COL_INFOBAR );

						last_next_id = info_CurrentNext.next_uniqueKey;
					}
				} 
	  		}
		}
  	}
}

void CInfoViewer::showIcon_Audio(const int ac3state) const
{
	const char *dd_icon;
	
	switch (ac3state)
	{
		case AC3_ACTIVE:
			dd_icon = NEUTRINO_ICON_DD;
			break;
			
		case AC3_AVAILABLE:
			dd_icon = NEUTRINO_ICON_DD_AVAIL;
			break;
			
		case NO_AC3:
		default:
			dd_icon = NEUTRINO_ICON_DD_GREY;
			break;
	}

	frameBuffer->paintIcon(dd_icon, BoxEndX - (LEFT_OFFSET + icon_w_subt + 2 + icon_w_vtxt + 2 + icon_w_dd), BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h_dd)/2 );
	
	if (ac3state == AC3_ACTIVE)
	{
		CVFD::getInstance()->ShowIcon(VFD_ICON_DOLBY, true);
#if defined(PLATFORM_SPARK7162)
		CVFD::getInstance()->ShowIcon(VFD_ICON_AC3, true);
		CVFD::getInstance()->ShowIcon(VFD_ICON_MP3, false);//FIXME:@dbo: why???
#endif
		//CVFD::getInstance()->ShowIcon(VFD_ICON_MP3, false);//FIXME:@dbo: why???
	}
	else
	{
		CVFD::getInstance()->ShowIcon(VFD_ICON_DOLBY, false);
#if defined(PLATFORM_SPARK7162)
		CVFD::getInstance()->ShowIcon(VFD_ICON_AC3, false);
		CVFD::getInstance()->ShowIcon(VFD_ICON_MP3, true); //FIXME:@dbo: why???
#endif
	}
}

void CInfoViewer::showButton_Audio()
{
  	// green, in case of several APIDs
  	uint32_t count = g_RemoteControl->current_PIDs.APIDs.size();
	
	int icon_w;
	int icon_h;
		
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_GREEN, &icon_w, &icon_h);
  	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_GREEN, BoxStartX + 5 + icon_w + 5 + asize, BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h)/2 );

  	if (count > 0) 
	{
		int sx = BoxStartX + 5 + icon_w + 5 + asize + icon_w + 5;

		g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->RenderString(sx, BoxEndY, asize - 5 - icon_w, g_Locale->getText(LOCALE_INFOVIEWER_LANGUAGES), COL_INFOBAR_BUTTONS, 0, true); // UTF-8
  	}

  	//const char *dd_icon;
	int ac3state;
  	if ((g_RemoteControl->current_PIDs.PIDs.selected_apid < count) && (g_RemoteControl->current_PIDs.APIDs[g_RemoteControl->current_PIDs.PIDs.selected_apid].is_ac3))
	{
		ac3state = AC3_ACTIVE;
	}
  	else if (g_RemoteControl->has_ac3)
	{
		ac3state = AC3_AVAILABLE;
	}
  	else
	{
		ac3state = NO_AC3;
	}


	showIcon_Audio(ac3state);
}

void CInfoViewer::killTitle()
{
	printf("CInfoViewer::killTitle\n");
	
  	if (is_visible) 
	{
		is_visible = false;

		frameBuffer->paintBackgroundBox(BoxStartX, BoxStartY - 30, BoxEndX + SHADOW_OFFSET, BoxEndY + SHADOW_OFFSET );
				
#if !defined USE_OPENGL
		frameBuffer->blit();
#endif

		// hide radiotext
#if ENABLE_RADIOTEXT		
		if (g_settings.radiotext_enable && g_Radiotext) 
		{
			g_Radiotext->S_RtOsd = g_Radiotext->haveRadiotext() ? 1 : 0;
			killRadiotext();
		}
#endif
		if(sigscale)
		{
			delete sigscale;
			sigscale = 0;
		}
		
		if(snrscale)
		{
			delete snrscale;
			snrscale = 0;
		}
		
		if(timescale)
		{
			delete timescale;
			timescale = 0;
		}
  	}
  	
  	if (m_visible) 
	{
		m_visible = false;

		frameBuffer->paintBackgroundBox(BoxStartX, BoxStartY, BoxEndX + SHADOW_OFFSET, BoxEndY + SHADOW_OFFSET );
				
#if !defined USE_OPENGL
		frameBuffer->blit();
#endif
		
		if(moviescale)
		{
			delete moviescale;
			moviescale = 0;
		}
  	}
}

void CInfoViewer::Set_CA_Status(int Status)
{
	CA_Status = Status;
	m_CA_Status = Status;

	if ( is_visible && showButtonBar )
		showIcon_CA_Status(1);
}

#if ENABLE_LCD
void CInfoViewer::showLcdPercentOver()
{
	if (g_settings.lcd_setting[SNeutrinoSettings::LCD_SHOW_VOLUME] != 1) 
	{
		int runningPercent = -1;
		time_t jetzt = time (NULL);

		if (info_CurrentNext.flags & CSectionsdClient::epgflags::has_current) 
		{
			if (jetzt < info_CurrentNext.current_zeit.startzeit)
				runningPercent = 0;
			else
				runningPercent = MIN ((unsigned) ((float) (jetzt - info_CurrentNext.current_zeit.startzeit) / (float) info_CurrentNext.current_zeit.dauer * 100.), 100);
		}

		CVFD::getInstance()->showPercentOver(runningPercent);	
	}
}
#endif

extern int pmt_caids[11];

void CInfoViewer::showIcon_CA_Status(int notfirst)
{
	int i;
	int caids[] = { 0x0600, 0x1700, 0x0100, 0x0500, 0x1800, 0x0B00, 0x0D00, 0x0900, 0x2600, 0x4a00, 0x0E00 };
		
	if(!notfirst) 
	{
		bool fta = true;
			
		for(i = 0; i < (int)(sizeof(caids)/sizeof(int)); i++) 
		{
			if (pmt_caids[i]) 
			{
				fta = false;
				break;
			}
		}
		
		frameBuffer->paintIcon( fta ? NEUTRINO_ICON_SCRAMBLED2_GREY : NEUTRINO_ICON_SCRAMBLED2, BoxEndX - (LEFT_OFFSET + icon_w_subt + 2 + icon_w_vtxt + 2 + icon_w_dd + 2 + icon_w_aspect + 2 + icon_w_sd + 2 + icon_w_reso + 2 + icon_w_ca), BoxEndY - BUTTON_BAR_HEIGHT + (BUTTON_BAR_HEIGHT - icon_h_ca)/2 );
#if !defined (PLATFORM_COOLSTREAM)
		CVFD::getInstance()->ShowIcon(VFD_ICON_LOCK, !fta);
#endif			
		return;
	}
}

void CInfoViewer::showEpgInfo()   //message on event change
{
	int mode = CNeutrinoApp::getInstance()->getMode();
	
	/* show epg info only if we in TV- or Radio mode and current event is not the same like before */
	if ((eventname != info_CurrentNext.current_name) && (mode == NeutrinoMessages::mode_tv || mode == NeutrinoMessages::mode_radio))
	{
		eventname = info_CurrentNext.current_name;
		
		g_RCInput->postMsg(NeutrinoMessages::SHOW_INFOBAR , 0);
	}
}

int CInfoViewerHandler::exec(CMenuTarget * parent, const std::string &/*actionkey*/)
{
	int res = menu_return::RETURN_EXIT_ALL;
	CChannelList * channelList;
	CInfoViewer * i;
	
	if (parent) 
		parent->hide ();
	
	i = new CInfoViewer;
	
	channelList = CNeutrinoApp::getInstance()->channelList;
	i->start();
	i->showTitle (channelList->getActiveChannelNumber(), channelList->getActiveChannelName(), channelList->getActiveSatellitePosition(), channelList->getActiveChannel_ChannelID ());	// UTF-8
	delete i;
	
	return res;
}


