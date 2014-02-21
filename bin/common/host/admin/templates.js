var xmlDoc;
var xmlTempl;
var xmlAccess;
var xmlClients;

var SL_ROOT		= 0;
var SL_TEMPLATE		= 1;
var SL_ACCESS		= 2;
var SL_CLIENTS		= 3;
var SL_CLIENTS_ACCESS	= 4;

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
        if ( Name == "template" )
        {
          xmlTempl = xmlDoc.createElement("template");
          xmlDoc.documentElement.appendChild(xmlTempl);
          NewScanLevel = SL_TEMPLATE;
        }
      }
      else if ( ScanLevel == SL_TEMPLATE )
      {
        if ( Name == "denided" )
          xmlTempl.setAttribute("denided", Node.checked ? "1":"0");
        else if ( Name == "hidden" )
          xmlTempl.setAttribute("hidden", Node.checked ? "1":"0");
        else if ( Name == "mount-point" )
        {
          var xmlTemplMountPoint = xmlDoc.createElement("mount-point");
          xmlTemplMountPoint.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
          xmlTempl.appendChild(xmlTemplMountPoint);
        }
        else if ( Name == "password" )
        {
          var xmlTemplPassword = xmlDoc.createElement("password");
          xmlTemplPassword.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
          xmlTempl.appendChild(xmlTemplPassword);
        }
        else if ( Name == "keep-alive" )
        {
          var xmlKeepAlive = xmlDoc.createElement("keep-alive");
          xmlKeepAlive.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
          xmlTempl.appendChild(xmlKeepAlive);
        }
        else if ( Name == "field" )
        {
          var xmlField = xmlDoc.createElement("field");
          var Override = Node.getElementsByTagName("input")[0];
          var Value = Node.getElementsByTagName("input")[1];
          if ( Value.value != "" )
          {
            xmlTempl.appendChild(xmlField);
            xmlField.setAttribute( "name", Value.name );
            xmlField.setAttribute( "override", Override.checked ? "1":"0" );
            xmlField.appendChild( xmlDoc.createTextNode( strip(Value.value) ) );
          }
        }
        else if ( Name == "access" )
        {
          xmlAccess = xmlDoc.createElement("access");
          xmlTempl.appendChild(xmlAccess);
          NewScanLevel = SL_ACCESS;
        }
        else if ( Name == "clients" )
        {
          xmlClients = xmlDoc.createElement("clients");
          xmlTempl.appendChild(xmlClients);
          NewScanLevel = SL_CLIENTS;
        }
      }
      else if ( ScanLevel == SL_ACCESS )
      {
        if ( Name == "acl" )
          AddACL(Node);
      }
      else if ( ScanLevel == SL_CLIENTS )
      {
        if ( Name == "max-clients" )
        {
          var xmlMaxClients = xmlDoc.createElement("max-clients");
          if ( Node.value != "" )
          {
            xmlMaxClients.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
            xmlClients.appendChild(xmlMaxClients);
          }
        }
        else if ( Name == "access" )
        {
          xmlAccess = xmlDoc.createElement("access");
          xmlClients.appendChild(xmlAccess);
          NewScanLevel = SL_CLIENTS_ACCESS;
        }
      }
      else if ( ScanLevel == SL_CLIENTS_ACCESS )
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

function TemplatesMakeXML(Node)
{
  xmlDoc = XMLNew("<?xml version=\"1.0\" encoding=\"utf-8\"?><templates></templates>");
  var TextXML;

  Scan(Node, SL_ROOT);

  TextXML = XMLSourseText(xmlDoc);
  delete xmlDoc;
  
  return TextXML;
}
