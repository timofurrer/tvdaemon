/*
 *  tvdaemon
 *
 *  Activity class
 *
 *  Copyright (C) 2013 André Roth
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Activity.h"
#include "Log.h"
#include "Frontend.h"
#include "Channel.h"
#include "TVDaemon.h"

#include <unistd.h> // NULL

Activity::Activity( ) : Thread( ), state(State_New), channel(NULL), service(NULL), transponder(NULL), frontend(NULL), up(true)
{
}

Activity::~Activity( )
{
  up = false;
  JoinThread( );
}

bool Activity::Start( )
{
  state = State_Starting;
  state_changed = time( NULL );
  StartThread( );
}

void Activity::Run( )
{
  bool ret;
  std::string name = GetTitle( );
  state = State_Running;
  state_changed = time( NULL );
  LogInfo( "Activity starting: %s", name.c_str( ));
  if( channel )
  {
    TVDaemon::Instance( )->LockFrontends( );
    if( !channel->Tune( *this ))
    {
      LogError( "Activity %s unable to start: tuning failed", name.c_str( ));
      goto fail;
    }
    TVDaemon::Instance( )->UnlockFrontends( );
  }
  else if( frontend and port and transponder )
  {
    TVDaemon::Instance( )->LockFrontends( );
    if( !frontend->Tune( *this ))
    {
      frontend->LogError( "Activity %s unable to start: tuning failed", name.c_str( ));
      goto fail;
    }
    TVDaemon::Instance( )->UnlockFrontends( );
  }

  ret = Perform( );

  if( frontend )
    frontend->Release( );

  if( state == State_Running )
  {
    if( ret )
    {
      state = State_Done;
      LogInfo( "Activity done: %s", name.c_str( ));
    }
    else
    {
      state = State_Failed;
      LogWarn( "Activity failed: %s", name.c_str( ));
    }
    state_changed = time( NULL );
  }

  return;

fail:
  TVDaemon::Instance( )->UnlockFrontends( );
  state = State_Failed;
  state_changed = time( NULL );
  Failed( );
  return;
}

void Activity::Abort( )
{
  if( state == State_Running )
  {
    state = State_Aborted;
    state_changed = time( NULL );
  }
  up = false;
}

bool Activity::SetState( State s )
{
  state = s;
  state_changed = time( NULL );
  return true;
}
