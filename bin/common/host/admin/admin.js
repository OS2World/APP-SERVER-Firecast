var xmlDoc;
var xmlAccess;

var SL_ROOT		= 0;
var SL_ACCESS		= 1;

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
        if ( Name == "name" )
        {
          var xmlName;
          xmlName = xmlDoc.createElement("name");
          xmlName.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
          xmlDoc.documentElement.appendChild(xmlName);
        }
        else if ( Name == "password" )
        {
          var xmlPassword = xmlDoc.createElement("password");
          xmlPassword.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
          xmlDoc.documentElement.appendChild(xmlPassword);
        }
        else if ( Name == "access" )
        {
          xmlAccess = xmlDoc.createElement("access");
          xmlDoc.documentElement.appendChild(xmlAccess);
          NewScanLevel = SL_ACCESS;
        }
        else if ( Name == "log-level" )
        {
          var xmlLogLevel = xmlDoc.createElement("log-level");
          xmlLogLevel.appendChild( xmlDoc.createTextNode(
                                     strip(Node.options[Node.selectedIndex].value) ) );
          xmlDoc.documentElement.appendChild(xmlLogLevel);
        }
        else if ( Name == "log-buffer" )
        {
          var xmlLogBuffer = xmlDoc.createElement("log-buffer");
          xmlLogBuffer.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
          xmlDoc.documentElement.appendChild(xmlLogBuffer);
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

function AdminMakeXML(Node)
{
  var TextXML;
  xmlDoc = XMLNew("<?xml version=\"1.0\" encoding=\"utf-8\"?><admin></admin>");

  Scan(Node, SL_ROOT);

  TextXML = XMLSourseText(xmlDoc);
  delete xmlDoc;
  
  return TextXML;
}
