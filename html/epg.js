$(document).ready( ready );

function ready( )
{
  Menu( "EPG" );
  t = ServerSideTable( 'epg', 'tvd?c=tvdaemon&a=get_epg', 10 );
  t["columns"] = {
    "start"    : [ "", print_start ],
    "name"     : [ "", print_event ],
  };
  t["click"] = function( ) {
    if( confirm( "Record " + this["name"] + " ?" ))
      getJSON( 'tvd?c=channel&a=schedule&channel_id=' + this["channel_id"] + "&event_id=" + this["id"], schedule );
  };
  t.filters( [ "search" ] );
  t.load( );
}

function schedule( data, errmsg )
{
  if( !data )
    return false;
  return true;
}

function duration( row )
{
  d = row["duration"] / 60;
  return Math.floor( d ) + "'";
}

function print_event( row )
{
  return "<div>" + row["name"] + "</div>" +
         "<div>" + row["channel"] + "</div>" +
         "<div>" + row["description"] + "</div>";
}

function print_start( row, field )
{
  r = print_time( row, field );
  r += "<br/>" + duration( row );
  return r;
}
