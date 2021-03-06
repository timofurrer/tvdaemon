/*
 *  tvdaemon
 *
 *  Activity_Stream class
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

#ifndef _Activity_Stream_
#define _Activity_Stream_

#include "Activity.h"
#include "Thread.h"

#include <ccrtp/rtp.h>
#include <list>
#include <map>

class Channel;
class Activity_Record;
class Frame;

class Activity_Stream : public Activity
{
  public:
    Activity_Stream( Channel *channel, ost::RTPSession &session );
    Activity_Stream( Activity_Record *recording, ost::RTPSession &session );
    virtual ~Activity_Stream( );

    virtual std::string GetTitle( ) const;
    virtual void Stop( );

    double GetDuration( );
    void Play( );
    void Pause( );

  private:
    virtual bool Perform( );
    virtual void Failed( ) { }

    bool StreamChannel( );
    bool StreamRecording( );

    void SendRTP( const uint8_t *data, int length );

    Activity_Record *recording;
    ost::RTPSession &session;
    struct timespec start_time;

    Condition cond;
    enum
    {
      State_Idle,
      State_Playing,
      State_Paused,
    } state;

    class Packet
    {
      public:
        Packet( );
        ~Packet( );
        uint16_t pid;
        uint64_t ts;
        uint64_t seq;
        uint8_t *data;
    };

    class PacketComp
    {
      public:
        bool operator()( Packet *a, Packet *b );
    };

    std::map<uint16_t, uint64_t> bigbang_map;
    std::map<uint16_t, uint64_t> ts_map;
    std::map<uint16_t, std::list<Frame *> > frames;
};

#endif

