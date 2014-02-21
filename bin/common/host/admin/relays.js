var xmlDoc;
var xmlRelay;

function Scan(Node)
{
  var Name;

  Node = Node.firstChild;
  while( Node != null )
  {
    if ( Node.getAttribute != null && Node.getAttribute("name") != null )
    {
      Name = Node.getAttribute("name");

      if ( Name == "mount-point" )
      {
        var xmlRelayMountPoint = xmlDoc.createElement("mount-point");
        xmlRelayMountPoint.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
        xmlRelay.appendChild(xmlRelayMountPoint);
      }
      else if ( Name == "location" )
      {
        var xmlRelayLocation = xmlDoc.createElement("location");
        xmlRelayLocation.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
        xmlRelay.appendChild(xmlRelayLocation);
      }
      else if ( Name == "nailed-up" )
      {
        var xmlRelayNailedUp = xmlDoc.createElement("nailed-up");
        xmlRelayNailedUp.appendChild( xmlDoc.createTextNode( Node.checked ? "1":"0" ) );
        xmlRelay.appendChild(xmlRelayNailedUp);
      }
    }

    Scan(Node);
    Node = Node.nextSibling;
  }
}

function RelayMakeXML(Node)
{
  xmlDoc = XMLNew("<?xml version=\"1.0\" encoding=\"utf-8\"?><relay></relay>");
  var TextXML;

  xmlRelay = xmlDoc.documentElement;

  Scan(Node);

  TextXML = XMLSourseText(xmlDoc);
  delete xmlDoc;
  
  return TextXML;
}

function RemoveRelay(Location, Id)
{
  if ( !confirm("Remove relay ("+Location+")?") )
    return false;

  document.location.href = "?stop=1&id="+Id;
  return false;
}
