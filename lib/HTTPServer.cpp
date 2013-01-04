/*
 * tvdaemon
 *
 * author     Andre Roth <neolynx@gmail.com>
 * copyright  GNU General Public License, see COPYING file
 */

#include "HTTPServer.h"
#include "Utils.h"
#include "Log.h"

#include <sstream>
#include <istream>
#include <iterator>
#include <vector>
#include <iostream>
#include <fstream>
#include <streambuf>
#include <string.h>

#include <sstream>

static const struct http_status response_status[] = {
  { HTTP_OK, "OK" },
  { HTTP_REDIRECT, "Redirect" },
  { HTTP_BAD_REQUEST, "Bad Request" },
  { HTTP_UNAUTHORIZED, "Unauthorized" },
  { HTTP_FORBIDDEN, "Forbidden" },
  { HTTP_NOT_FOUND, "Not Found" },
  { HTTP_INTERNAL_SRV_ERROR, "Internal Server Error" },
};

static const struct mime_type mime_types[] = {
  { "html", "text/html", false },
  { "js", "text/javascript", false },
  { "css", "text/css", false },
  { "jpg", "image/jpeg", true },
  { "jpeg", "image/jpeg", true },
  { "gif", "image/gif", true },
  { "png", "image/png", true },
  { "json", "application/json", false },
};

HTTPServer::HTTPServer( const char *root ) : SocketHandler( ), _root(root)
{
}

HTTPServer::~HTTPServer( )
{
}

void HTTPServer::Connected( int client )
{
}

void HTTPServer::Disconnected( int client, bool error )
{
  if( _requests.find( client ) != _requests.end( ))
  {
    delete _requests[client];
    _requests.erase( client );
  }
}

SocketHandler::Message *HTTPServer::CreateMessage( int client ) const
{
  std::map<int, HTTPRequest *>::const_iterator it = _requests.find( client );
  if( it != _requests.end( ) && it->second->content_length != -1 )
    return new HTTPServer::Message( it->second->content_length );
  return new SocketHandler::Message( );
}

int HTTPServer::Message::AccumulateData( const char *buffer, int length )
{
  int i = content_length - line.length( );
  if( length < i )
    i = length;

  line.append( buffer, i );
  if( content_length == line.length( ))
  {
    Submit( );
  }
  return i;
}

void HTTPServer::HandleMessage( const int client, const SocketHandler::Message &msg )
{
  const char *buffer = msg.getLine( ).c_str( );
  int length = msg.getLine( ).length( );
  //LogWarn( "Got: '%s' from client %d", msg.getLine( ).c_str( ), client );

  if( _requests.find( client ) == _requests.end( ))
    _requests[client] = new HTTPRequest( );

  if( msg.getLine( ).empty( ))
  {
    if( _requests[client]->content_length == -1 )
      for( std::list<std::string>::iterator it = _requests[client]->header.begin( ); it != _requests[client]->header.end( ); it++ )
      {
        int content_length = -1;
        if( sscanf( it->c_str( ), "Content-Length: %d", &content_length ) == 1 && content_length > 0 )
        {
          _requests[client]->content_length = content_length;
          return;
        }
      }
    HandleHTTPRequest( client, *_requests[client] );
    DisconnectClient( client );
  }
  else
  {
    if( _requests[client]->content_length > 0 && _requests[client]->content.length( ) < _requests[client]->content_length )
    {
      _requests[client]->content += msg.getLine( );
      HandleHTTPRequest( client, *_requests[client] );
      DisconnectClient( client );
    }
    else
      _requests[client]->header.push_back( msg.getLine( ));
  }
}

void HTTPServer::AddDynamicHandler( std::string url, HTTPDynamicHandler *handler )
{
  url = "/" + url;
  dynamic_handlers[url] = handler;
}

bool HTTPServer::HandleHTTPRequest( const int client, HTTPRequest &request )
{
  if( request.header.empty( ))
    return false;

  const char *method = request.header.front( ).c_str( );

  if( strncmp( method, "GET ", 4 ) == 0 )
  {
    HandleMethodGET( client, request );
  }
  else if( strncmp( method, "POST ", 5 ) == 0 )
  {
    HandleMethodPOST( client, request );
  }
  else
  {
    HTTPResponse *err_response = new HTTPResponse( );
    err_response->AddStatus( HTTP_NOT_IMPLEMENTED );
    err_response->AddTimeStamp( );
    err_response->AddMime( "html" );
    err_response->AddContents( "<html><body><h1>501 Method not implemented</h1></body></html>" );
    LogError( "HTTPServer: method not implemented: %s", method );
    SendToClient( client, err_response->GetBuffer( ).c_str( ), err_response->GetBuffer( ).size( ));
  }
  return false;
}

bool HTTPServer::HandleMethodGET( const int client, HTTPRequest &request )
{
  std::vector<std::string> tokens;
  Tokenize( request.header.front( ).c_str( ), " ", tokens );

  if( tokens.size( ) < 3 )
  {
    LogError( "HTTPServer: unknown http message '%s'", request.header.front( ).c_str( ));
    // FIXME: http error
    return false;
  }

  std::string url = tokens[1];
  std::vector<std::string> params;
  Tokenize( url.c_str( ), "?&", params );
  std::map<std::string, std::string> parameters;
  for( int i = 1; i < params.size( ); i++ )
  {
    std::vector<std::string> p;
    Tokenize( params[i].c_str( ), "=", p );
    if( p.size( ) == 1 )
      parameters[p[0]] = "1";
    else if( p.size( ) == 2 )
      parameters[p[0]] = p[1];
    else
    {
      LogWarn( "Ignoring strange parameter: '%s'", params[i].c_str( ));
      continue;
    }
    //Log( "Param: %s => %s", p[0], val );
  }

  for( std::map<std::string, HTTPDynamicHandler *>::iterator it = dynamic_handlers.begin( ); it != dynamic_handlers.end( ); it++ )
    if( params[0] == it->first )
    {
      if( !it->second->HandleDynamicHTTP( client, parameters ))
      {
        LogError( "RPC Error %s", url.c_str( ));
        return false;
      }
      //Log( "RPC %s", url.c_str( ));
      return true;
    }

  // Handle http files

  url = _root;
  if( tokens[1][0] != '/' )
    url += "/";
  url += tokens[1];
  url = Utils::Expand( url.c_str( ));

  if( url.empty( ))
  {
    LogError( "HTTPServer: file not found: %s", tokens[1].c_str( ));
    HTTPResponse *err_response = new HTTPResponse( );
    err_response->AddStatus( HTTP_NOT_FOUND );
    err_response->AddTimeStamp( );
    err_response->AddMime( "html" );
    err_response->AddContents( "<html><body><h1>404 Not found</h1></body></html>" );
    //LogWarn( "Sending: %s", err_response->GetBuffer( ).c_str( ));
    SendToClient( client, err_response->GetBuffer( ).c_str( ), err_response->GetBuffer( ).size( ));
    return false;
  }

  if( Utils::IsDir( url ))
  {
    Utils::EnsureSlash( url );
    url += "index.html";
  }

  std::string filename;

  size_t p = url.find_first_of("#?");
  if( p != std::string::npos )
    filename = url.substr( 0, p );
  else
    filename = url;

  std::ifstream file;
  file.open( filename.c_str( ), std::ifstream::in );
  if( !file.is_open( ))
  {
    LogError( "HTTPServer: file not found: %s", url.c_str( ));
    HTTPResponse *err_response = new HTTPResponse( );
    err_response->AddStatus( HTTP_NOT_FOUND );
    err_response->AddTimeStamp( );
    err_response->AddMime( "html" );
    err_response->AddContents( "<html><body><h1>404 Not found</h1></body></html>" );
    //LogWarn( "Sending: %s", err_response->GetBuffer( ).c_str( ));
    SendToClient( client, err_response->GetBuffer( ).c_str( ), err_response->GetBuffer( ).size( ));
    return false;
  }

  //Log( "HTTPServer: serving: %s", url.c_str( ));
  std::string file_contents;

  file.seekg( 0, std::ios::end );
  file_contents.reserve( file.tellg( ));
  file.seekg( 0, std::ios::beg );

  file_contents.assign((std::istreambuf_iterator<char>( file )), std::istreambuf_iterator<char>( ));
  file.close( );

  std::string basename = Utils::BaseName( filename.c_str( ));
  std::string extension = Utils::GetExtension( basename );

  HTTPResponse *response = new HTTPResponse( );
  response->AddStatus( HTTP_OK );
  response->AddTimeStamp( );
  response->AddMime( extension.c_str( ));
  response->AddContents( file_contents );
  //LogWarn( "Sending %s", response->GetBuffer( ).c_str( ));

  return SendToClient( client, response->GetBuffer( ).c_str( ), response->GetBuffer( ).size( ));
}

bool HTTPServer::HandleMethodPOST( const int client, HTTPRequest &request )
{
  std::vector<std::string> tokens;
  Tokenize( request.header.front( ).c_str( ), " ", tokens );

  if( tokens.size( ) < 3 )
  {
    // FIXME: http error
    return false;
  }

  std::string url = tokens[1];
  std::vector<std::string> params;
  Tokenize( url.c_str( ), "?&", params );
  std::map<std::string, std::string> parameters;
  for( int i = 1; i < params.size( ); i++ )
  {
    std::vector<std::string> p;
    Tokenize( params[i].c_str( ), "=", p );
    if( p.size( ) == 1 )
      parameters[p[0]] = "1";
    else if( p.size( ) == 2 )
      parameters[p[0]] = p[1];
    else
    {
      LogWarn( "Ignoring strange parameter: '%s'", params[i].c_str( ));
      continue;
    }
    //Log( "Param: %s => %s", p[0], val );
  }

  if( request.content.length( ) > 0 )
  {
    std::vector<std::string> vars;
    Tokenize( request.content.c_str( ), "&", vars );
    for( int i = 0; i < vars.size( ); i++ )
    {
      std::vector<std::string> p;
      Tokenize( vars[i].c_str( ), "=", p );
      if( p.size( ) == 1 )
        parameters[p[0]] = "1";
      else if( p.size( ) == 2 )
      {
        int len = p[1].length( );
        for( int j = 0; j < len; j++ )
        {
          if( p[1][j] == '%' )
          {
            if( j > len - 3 )
            {
              LogError( "HTTPServer: invalid url encoded data: '%s'", p[1].c_str( ));
              break;
            }
            char x = p[1][j+1];
            switch( x )
            {
              case '0' ... '9':
                x -= '0';
                break;
              case 'a' ... 'f':
                x -= 'a' - 10;
                break;
              case 'A' ... 'F':
                x -= 'A' - 10;
                break;
              default:
                LogError( "HTTPServer: inot a hex value: %c", x );
            }

            char y = p[1][j+2];
            switch( y )
            {
              case '0' ... '9':
                y -= '0';
                break;
              case 'a' ... 'f':
                y -= 'a' - 10;
                break;
              case 'A' ... 'F':
                y -= 'A' - 10;
                break;
              default:
                LogError( "HTTPServer: inot a hex value: %c", y );
            }
            parameters[p[0]] += ( x << 4 ) + y;

            j += 2;
            continue;
          }
          if( p[1][j] == '+' )
            parameters[p[0]] += ' ';
          else
            parameters[p[0]] += p[1][j];
        }
      }
      else
      {
        LogWarn( "Ignoring strange variable: '%s'", params[i].c_str( ));
        continue;
      }
    }

  }

  for( std::map<std::string, HTTPDynamicHandler *>::iterator it = dynamic_handlers.begin( ); it != dynamic_handlers.end( ); it++ )
    if( params[0] == it->first )
    {
      if( !it->second->HandleDynamicHTTP( client, parameters ))
      {
        LogError( "RPC Error %s", url.c_str( ));
        return false;
      }
      return true;
    }

  LogError( "HTTPServer: no dynamic handler found for POST request" );
  return false;
}

HTTPServer::HTTPResponse::HTTPResponse( )
{
}

HTTPServer::HTTPResponse::~HTTPResponse( )
{
}

void HTTPServer::HTTPResponse::AddStatus( HTTPStatus status )
{
  char tmp[128];
  uint8_t pos = 0;

  for( uint8_t i = 0; i < ( sizeof( response_status ) / sizeof( http_status )); i++ )
  {
    if( response_status[i].code == status )
    {
      pos = i;
      break;
    }
  }

  snprintf( tmp, sizeof( tmp ), "%s %d %s\r\n", HTTP_VERSION, status, response_status[pos].description );
  _buffer.append( tmp );
}

void HTTPServer::HTTPResponse::AddTimeStamp( )
{
  time_t rawtime;
  time( &rawtime );
  struct tm *timeinfo = gmtime( &rawtime );
  char date_buffer[512];
  strftime( date_buffer, sizeof( date_buffer ), "%A, %e %B %Y %H:%M:%S GMT\r\n", timeinfo );
  _buffer.append( date_buffer );
}

void HTTPServer::HTTPResponse::AddAttribute( const char *attrib_name, const char *attrib_value )
{
  char tmp[256];
  snprintf( tmp, sizeof( tmp ), "%s: %s\r\n", attrib_name, attrib_value );
  _buffer.append( tmp );
}

void HTTPServer::HTTPResponse::FreeResponseBuffer( )
{
  _buffer.clear( );
}

void HTTPServer::HTTPResponse::AddContents( std::string buffer )
{
  size_t length = buffer.size( );
  char length_str[10];
  snprintf( length_str, sizeof( length_str ), "%d", length );

  AddAttribute( "Content-Length", length_str );
  _buffer.append( "\r\n" );
  _buffer += buffer;
}

std::string HTTPServer::HTTPResponse::GetBuffer( )
{
  return _buffer;
}

void HTTPServer::HTTPResponse::AddMime( const char *mime )
{
  int i = -1;
  for( i = 0; i < sizeof( mime_types ) / sizeof( mime_type ); i++ )
    if( strcmp( mime, mime_types[i].extension ) == 0 )
      break;
  if( i >= sizeof( mime_types ) / sizeof( mime_type ))
  {
    LogError( "HTTPServer: unknown mime '%s'", mime );
    return;
  }

  _buffer.append( "Content-Type: " );
  _buffer += mime_types[i].type;
  _buffer.append( "\r\n" );
}

int HTTPServer::Tokenize( const char *string, const char delims[], std::vector<std::string> &tokens, int count )
{
  int len = strlen( string );
  int dlen = strlen( delims );
  int last = -1;
  for( int i = 0; i < len; i++ )
  {
    // eat delimiters
    while( i < len )
    {
      bool found = false;
      for( int j = 0; j < dlen; j++ )
        if( string[i] == delims[j] )
        {
          found = true;
          break;
        }
      if( !found )
        break;
      i++;
    }
    if( i >= len )
      break;

    int j = i;

    // eat non-delimiters
    while( i < len )
    {
      bool found = false;
      for( int j = 0; j < dlen; j++ )
        if( string[i] == delims[j] )
        {
          found = true;
          break;
        }
      if( found )
        break;
      i++;
    }

    tokens.push_back( std::string( string + j,  i - j ));

    if( i == len )
      break;

    if( count )
      if( --count == 0 )
        break;
  }
}


