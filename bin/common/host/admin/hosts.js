var xmlDoc;
var xmlHost;
var xmlAccess;

var SL_ROOT		= 0;
var SL_HOST		= 1;
var SL_ACCESS		= 2;
var SL_CLIENTS		= 3;
var SL_CLIENTS_ACCESS	= 4;

function AddACL(Node)
{
  var Action = Node.getElementsByTagName("select")[0];
  var Addr = Node.getElementsByTagName("input")[0];
  if ( Addr.value != "" )
  {
    var xmlACL = xmlDoc.createElement("acl");
    xmlACL.setAttribute( "action",
                         Action.options[Action.selectedIndex].value );
    xmlACL.appendChild( xmlDoc.createTextNode( strip(Addr.value) ) );
    xmlAccess.appendChild(xmlACL);
  }
}

function Scan(Node,ScanLevel)
{
  var Name;

  Node = Node.firstChild;
  while( Node != null )
  {
    NewScanLevel = ScanLevel;

    if ( Node.getAttribute != null && Node.getAttribute("name") != null )
    {
      Name = Node.getAttribute("name");

      if ( ScanLevel == SL_ROOT )
      {
        if ( Name == "host" )
        {
          xmlHost = xmlDoc.createElement("host");
          xmlDoc.documentElement.appendChild(xmlHost);
          NewScanLevel = SL_HOST;
        }
      }
      else if ( ScanLevel == SL_HOST )
      {
        if ( Name == "path" )
        {
          var xmlLocalPath = xmlDoc.createElement("path");
          xmlLocalPath.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
          xmlHost.appendChild(xmlLocalPath);
        }
        else if ( Name == "name" )
        {
          if ( Node.value != "" )
          {
            var xmlName = xmlDoc.createElement("name");
            xmlName.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
            xmlHost.appendChild(xmlName);
          }
        }
        else if ( Name == "index" )
        {
          if ( Node.value != "" )
          {
            var xmlIndex = xmlDoc.createElement("index");
            xmlIndex.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
            xmlHost.appendChild(xmlIndex);
          }
        }
        else if ( Name == "access" )
        {
          xmlAccess = xmlDoc.createElement("access");
          xmlHost.appendChild(xmlAccess);
          NewScanLevel = SL_ACCESS;
        }
      }
      else if ( ScanLevel == SL_ACCESS )
      {
        if ( Name == "acl" )
          AddACL(Node);
      }
    }

    Scan(Node, NewScanLevel);
    NewScanLevel = ScanLevel;
    Node = Node.nextSibling;
  }
}

function HostsMakeXML(Node)
{
  xmlDoc = XMLNew("<?xml version=\"1.0\" encoding=\"utf-8\"?><hosts></hosts>");
  var TextXML;

  Scan(Node, SL_ROOT);

  TextXML = XMLSourseText(xmlDoc);
  delete xmlDoc;
  
  return TextXML;
}
