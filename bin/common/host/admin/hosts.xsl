<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html" encoding="utf-8" indent="yes"/>

<xsl:include href="utils.xsl" />

<xsl:template match="/">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
  <meta http-equiv="Content-Script-Type" content="text/javascript" />
  <script type="text/javascript" src="utils.js"></script>
  <script type="text/javascript" src="hosts.js"></script>
  <link rel="stylesheet" type="text/css" href="style.css" />
</head>
<body>
  <xsl:call-template name="page-title">
    <xsl:with-param name="image">hosts</xsl:with-param>
    <xsl:with-param name="text">Web-hosts</xsl:with-param>
  </xsl:call-template>

  <div id="hosts-list">
    <xsl:apply-templates select="hosts/host" />
  </div>

  <form enctype="text/plain" action="/admin/hosts"
        method="post" name="apply"
        onsubmit="
           if ( !ConfirmChanges() )
             return false;
           document.apply.XMLText.value = HostsMakeXML( document.getElementById('hosts-list') );
           return true;">
    <input type="hidden" name="XMLText" />
    <input type="submit" value="Apply" class="apply" />
  </form>
</body>
</xsl:template>

<xsl:template match="host">
  <div name="host" class="block">
    <xsl:call-template name="button-image-big">
      <xsl:with-param name="image">btn-big-x.gif</xsl:with-param>
      <xsl:with-param name="text">remove host</xsl:with-param>
      <xsl:with-param name="code">return ItemDelete(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="button-image-big">
      <xsl:with-param name="image">btn-big-add.gif</xsl:with-param>
      <xsl:with-param name="text">new host</xsl:with-param>
      <xsl:with-param name="code">return ItemAdd(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="button-image-big">
      <xsl:with-param name="image">btn-big-up.gif</xsl:with-param>
      <xsl:with-param name="text">Up</xsl:with-param>
      <xsl:with-param name="code">return ItemMoveUp(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="button-image-big">
      <xsl:with-param name="image">btn-big-dn.gif</xsl:with-param>
      <xsl:with-param name="text">Down</xsl:with-param>
      <xsl:with-param name="code">return ItemMoveDown(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <br />

    <span class="field">Local path:</span>
    <input type="text" name="path" size="50" maxlength="255">
      <xsl:attribute name="value">
        <xsl:value-of select="path" />
      </xsl:attribute>
    </input>
    <br />

    <span class="field">Host names:</span>
    <div>
      <xsl:apply-templates select="name" />
      <xsl:call-template name="name-item" />
    </div>

    <span class="field">Index files:</span>
    <div>
      <xsl:apply-templates select="index" />
      <xsl:call-template name="index-item" />
    </div>

    <h2>Access list:</h2>
    <xsl:call-template name="access-list" />
  </div>
</xsl:template>

<xsl:template match="index">
  <xsl:call-template name="index-item">
    <xsl:with-param name="value" select="." />
  </xsl:call-template>
</xsl:template>

<xsl:template name="index-item">
  <xsl:param name="value" />

  <div>
    <input type="text" name="index" size="30" maxlength="64">
      <xsl:attribute name="value"><xsl:value-of select="$value" /></xsl:attribute>
    </input>

    <xsl:call-template name="button-image-small">
      <xsl:with-param name="image">btn-x.gif</xsl:with-param>
      <xsl:with-param name="text">remove item</xsl:with-param>
      <xsl:with-param name="code">return ItemDelete(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="button-image-small">
      <xsl:with-param name="image">btn-add.gif</xsl:with-param>
      <xsl:with-param name="text">copy item</xsl:with-param>
      <xsl:with-param name="code">return ItemAdd(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="button-image-small">
      <xsl:with-param name="image">btn-up.gif</xsl:with-param>
      <xsl:with-param name="text">Up</xsl:with-param>
      <xsl:with-param name="code">return ItemMoveUp(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="button-image-small">
      <xsl:with-param name="image">btn-dn.gif</xsl:with-param>
      <xsl:with-param name="text">Down</xsl:with-param>
      <xsl:with-param name="code">return ItemMoveDown(this.parentNode);</xsl:with-param>
    </xsl:call-template>
  </div>
</xsl:template>

<xsl:template match="name">
  <xsl:call-template name="name-item">
    <xsl:with-param name="value" select="." />
  </xsl:call-template>
</xsl:template>

<xsl:template name="name-item">
  <xsl:param name="value" />

  <div>
    <input type="text" name="name" size="30" maxlength="128">
      <xsl:attribute name="value"><xsl:value-of select="$value" /></xsl:attribute>
    </input>

    <xsl:call-template name="button-image-small">
      <xsl:with-param name="image">btn-x.gif</xsl:with-param>
      <xsl:with-param name="text">remove item</xsl:with-param>
      <xsl:with-param name="code">return ItemDelete(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="button-image-small">
      <xsl:with-param name="image">btn-add.gif</xsl:with-param>
      <xsl:with-param name="text">copy item</xsl:with-param>
      <xsl:with-param name="code">return ItemAdd(this.parentNode);</xsl:with-param>
    </xsl:call-template>
  </div>
</xsl:template>

</xsl:stylesheet>
