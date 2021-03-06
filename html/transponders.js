$(document).ready( ready );

var table;

function ready( )
{
  Menu( "Transponders" );
  table = ServerSideTable( 'transponders', 'tvd?c=tvdaemon&a=get_transponders', 20 );
  table["columns"] = {
    "delsys"    : [ "", print_delsys ],
    "frequency" : [ "Frequency", print_freq ],
    "state"     : [ "State", print_state ],
    "epg_state" : [ "EPG", epg_state ],
    "enabled"   : "Enabled",
    "tsid"      : "TS ID",
    "signal"    : [ "S/N", print_sn ],
    "services"  : "Services",
    "source"    : "Source",
    "_scan"  : [ "", print_scan ],
    "_dump"  : [ "", print_dump ],
  };
  table.load( );
  table.filters( [ "search" ] );
}

function print_freq( row )
{
  f = row["frequency"] / 1000000;
  f = Math.round( f * 1000 ) / 1000;
  return f.toFixed( 3 );
}

function print_sn( row )
{
  return row["signal"] + "/" + row["noise"];
}

function print_state( row )
{
  switch( row["state"] )
  {
    case 0: return "New";
    case 1: return "Selected";
    case 2: return "Tuning";
    case 3: return "Tuned";
    case 4: return "No Tune";
    case 5: return "Scanning";
    case 6: return "Scanned";
    case 7: return "Scanning failed";
    case 8: return "Idle";
    case 9: return "Duplicate";
  }
}

function print_delsys( row )
{
  switch( row["delsys"] )
  {
    case 3: return "DVB-T";
  }
}

function print_scan( row )
{
  return "<a href=\"javascript: scan( " + row['source_id'] + ", " + row['id'] + " );\">scan</a>";
}

function print_dump( row )
{
  return "<a href=\"javascript: dump( " + row['source_id'] + ", " + row['id'] + " );\">dump</a>";
}

function scan( source_id, id )
{
  getJSON( 'tvd?c=transponder&a=scan&source_id=' + source_id +
      '&transponder_id=' + id, scanTransponder );
}

function scanTransponder( data, errmsg )
{
  table.load( );
}

function dump( source_id, id )
{
  getJSON( 'tvd?c=transponder&a=dump&source_id=' + source_id +
      '&transponder_id=' + id, scanTransponder );
}

