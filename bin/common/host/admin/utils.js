function StopSource(mp,id)
{
  if ( confirm('The source "'+mp+'" will be disconnected. Are you shure?') )
    document.location='source?id='+id+'&stop=1';
  return false;
}

function StopClient(addr,id,cid)
{
  if ( confirm('The client ['+addr+'] will be disconnected. Are you shure?') )
    document.location='source?id='+id+'&cid='+cid+'&stop=1';
  return false;
}

function ConfirmChanges()
{
  return confirm('Changes on this page can cause loss of the control. Continue?');
}


function strip(x) {
  while (x.substring(0,1) == ' ') x = x.substring(1);
  while (x.substring(x.length-1,x.length) == ' ') x = x.substring(0,x.length-1);
  return x;
}



function SetClass(el, class_name)
{
  if ( el.setAttribute )
    el.setAttribute((document.all ? 'className' : 'class'), class_name);
}

function GetClass(el)
{
  return el.getAttribute((document.all ? 'className' : 'class'));
}

function SetOnClick(el, code_str)
{
  if ( window.execScript )
  {
    code_str = new Function(code_str);
    el.onclick = code_str;
  }
  else
    el.setAttribute('onclick', code_str);
}

function DeleteElement(el)
{
  if ( el ) el.parentNode.removeChild(el);
  return false;
}


function ItemDelete(Node)
{
  if ( Node.parentNode.firstChild.nextSibling != null )
    DeleteElement(Node);
  return false;
}

function ItemMoveUp(Node)
{
  if ( Node ) {
    el_prev = Node.previousSibling;
    if ( el_prev != null && GetClass(el_prev) == GetClass(Node) )
      Node.parentNode.insertBefore(el_prev,Node.nextSibling);
  }
  return false;
}

function ItemMoveDown(Node)
{
  if ( Node ) {
    el_next = Node.nextSibling;
    if ( el_next != null && GetClass(el_next) == GetClass(Node) )
      Node.parentNode.insertBefore(el_next,Node);
  }
  return false;
}

function ItemAdd(Node)
{
  var NewAcl = document.createElement('div');
  var El;
  var Option;

  NewAcl = Node.cloneNode(true);
/*
  El = document.createElement('a');
  El.setAttribute('href','/');
  El.appendChild(document.createTextNode(' X '));
  SetClass(El, 'del-btn');
  SetOnClick(El, 'return DeleteElement(this.parentNode);');
  NewAcl.appendChild(El);

  El = document.createElement('a');
  El.setAttribute('href','/');
  El.appendChild(document.createTextNode(' + '));
  SetClass(El, 'add-btn');
  SetOnClick(El, 'return AddAcl(this.parentNode);');
  NewAcl.appendChild(El);

  El = document.createElement('a');
  El.setAttribute('href','/');
  El.appendChild(document.createTextNode(' ^ '));
  SetClass(El, 'up-btn');
  SetOnClick(El, 'return MoveUp(this.parentNode);');
  NewAcl.appendChild(El);

  El = document.createElement('a');
  El.setAttribute('href','/');
  El.appendChild(document.createTextNode(' v '));
  SetClass(El, 'down-btn');
  SetOnClick(El, 'return MoveDown(this.parentNode);');
  NewAcl.appendChild(El);

  El = document.createElement('select');
  Option = document.createElement('option');
  Option.setAttribute('value','allow');
  Option.appendChild(document.createTextNode('allow'));
  El.appendChild(Option);
  Option = document.createElement('option');
  Option.setAttribute('value','deny');
  Option.appendChild(document.createTextNode('deny'));
  El.appendChild(Option);
  NewAcl.appendChild(El);

  El = document.createElement('input');
  El.setAttribute('type','text');
  NewAcl.appendChild(El);

  SetClass(NewAcl,"acl-item");
  Node.appendChild(NewAcl);
*/
  Node.parentNode.insertBefore(NewAcl, Node.nextSibling);
  return false;
}


function XMLNew(xmlString)
{  
  var xmlDoc;

  if ( document.implementation.createDocument )
  { 
    var parser = new DOMParser(); 
    xmlDoc = parser.parseFromString(xmlString, "text/xml"); 
  }
  else { 
    xmlDoc = new ActiveXObject("Microsoft.XMLDOM") 
    xmlDoc.async = "false"; 
    xmlDoc.loadXML(xmlString);   
  }

  return xmlDoc;
}

function XMLSourseText(xmlDoc)
{
  if ( document.all )
    return xmlDoc.xml;
  else
    return (new XMLSerializer()).serializeToString(xmlDoc);
}


function AddACL(Node)
{
  var Action = Node.getElementsByTagName("select")[0];
  var Addr = Node.getElementsByTagName("input")[0];
  if ( Addr.value != "" )
  {
    var xmlACL = xmlDoc.createElement("acl");
    xmlACL.setAttribute( "action",
                         Action.options[Action.selectedIndex].value );
    xmlACL.appendChild( xmlDoc.createTextNode( Addr.value ) );
    xmlAccess.appendChild(xmlACL);
  }
}
