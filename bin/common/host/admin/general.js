var xmlDoc;
var xmlBind;
var ShoutComp;

var SL_ROOT		= 0;
var SL_BIND		= 1;


function ShoutCompChecked(CheckBox)
{
  var Label = CheckBox.parentNode;
  var Div_sh = Label.nextSibling;
  var Scan = Div_sh.firstChild;

  SetClass(Div_sh, CheckBox.checked ? "line" : "shout-mp-disable");

  while( Scan != null )
  {
    if ( Scan.getAttribute != null && Scan.getAttribute("name") == "shout-mp" )
    {
      SetClass(Scan, CheckBox.checked ? "line" : "shout-mp-disable");

      Scan.disabled = !CheckBox.checked;
      break;
    }
    Scan = Scan.nextSibling;
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
        if ( Name == "bind" )
        {
          xmlBind = xmlDoc.createElement("bind");
          xmlDoc.documentElement.appendChild(xmlBind);
          NewScanLevel = SL_BIND;
        }
      }
      else if ( ScanLevel == SL_BIND )
      {
        if ( Name == "interface" )
        {
          if ( Node.value != "" )
          {
            var xmlInterface = xmlDoc.createElement("interface");
            xmlInterface.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
            xmlBind.appendChild(xmlInterface);
          }
        }
        else if ( Name == "port" )
        {
          var xmlPort = xmlDoc.createElement("port");
          xmlPort.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
          xmlBind.appendChild(xmlPort);
        }
        else if ( Name == "shout-comp" )
        {
          ShoutComp = Node.checked;
        }
        else if ( Name == "shout-mp" )
        {
          if ( ShoutComp )
          {
            var xmlPort = xmlDoc.createElement("shout-mp");
            xmlPort.appendChild( xmlDoc.createTextNode( strip(Node.value) ) );
            xmlBind.appendChild(xmlPort);
          }
        }
      }
    }

    Scan(Node, NewScanLevel);
    NewScanLevel = ScanLevel;
    Node = Node.nextSibling;
  }
}

function GeneralMakeXML(Node)
{
  xmlDoc = XMLNew("<?xml version=\"1.0\" encoding=\"utf-8\"?><general></general>");
  var TextXML;

  Scan(Node, SL_ROOT);

  TextXML = XMLSourseText(xmlDoc);
  delete xmlDoc;
  
  return TextXML;
}
