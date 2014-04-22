/*
 *  tvdaemon
 *
 *  DVB Stream class
 *
 *  Copyright (C) 2012 André Roth
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

#ifndef _Stream_
#define _Stream_

#include <stdint.h>

// libav
extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

#include <ccrtp/rtp.h> // FIXME: make interfase/impl

#include "RPCObject.h"
#include "Thread.h"

#include <libdvbv5/descriptors.h>

class ConfigBase;
class Service;
class Frontend;
class RingBuffer;

class Stream : public Thread
{
  public:
    enum Type
    {
      Type_Unknown,
      _start_video_types,
      Type_Video,
      Type_Video_H262,
      Type_Video_H264,
      _start_audio_types,
      Type_Audio,
      Type_Audio_13818_3,
      Type_Audio_ADTS,
      Type_Audio_LATM,
      Type_Audio_AC3,
      Type_Audio_AAC,
      _end_audio_types
    };

    Stream( Service &service, uint16_t id, enum Type type, int config_id );
    Stream( Service &service );
    virtual ~Stream( );

    virtual bool SaveConfig( ConfigBase &config );
    virtual bool LoadConfig( ConfigBase &config );

    virtual int GetKey( ) const { return id; }

    bool Update( Type type );
    Type GetType( ) { return type; }
    int GetTypeMPEG( );
    const char *GetTypeName( ) { return GetTypeName( type ); };
    bool IsVideo( ) { return type > _start_video_types && type < _start_audio_types; }
    bool IsAudio( ) { return type > _start_audio_types && type < _end_audio_types; }

    static const char *GetTypeName( Type type );

    bool Init( ost::RTPSession *session );
    void HandleData( uint8_t *data, ssize_t len );

    // RPC
    void json( json_object *entry ) const;

  private:
    Service &service;
    uint16_t id;
    Type type;

    AVFormatContext *ifc;
    AVIOContext *ictx;
    AVInputFormat *iformat;

    AVFormatContext *ofc;
    AVIOContext *octx;
    AVOutputFormat *oformat;

    AVStream *st;
    uint8_t *buffer;
    uint8_t *buffer2;
    RingBuffer *ringbuffer;
    bool up;
    Condition cond;

    static int read_packet( void *opaque, uint8_t *buf, int buf_size );
    static int write_packet( void *opaque, uint8_t *buf, int buf_size );
    virtual void Run( );

    ost::RTPSession *session;
    uint32 timestamp;
};

#endif
