$(document).ready( ready );

function ready( )
{
  Menu( "EPG" );
  t = ServerSideTable( 'epg', 'tvd?c=tvdaemon&a=get_epg', 10 );
  t["columns"] = {
    ""         : [ "Start", print_time ],
    "channel"  : "Channel",
    "id"  : "ID",
    "duration" : [ "Duration", duration ],
    "name"     : [ "Name", function ( row ) { return "<b>" + row["name"] + "</b><br/>" + row["description"] } ]
  };
  t["click"] = function( row ) {
    if( confirm( "Record " + this["name"] + " ?" ))
      getJSON( 'tvd?c=channel&a=schedule&channel_id=' + this["channel_id"] + "&event_id=" + this["id"], schedule );
  };
  t.load( );
}

function schedule( data, errmsg )
{
  if( !data )
    return false;
  return true;
}

function duration( event )
{
  d = event["duration"] / 60;
  return Math.floor( d ) + "'";
}

