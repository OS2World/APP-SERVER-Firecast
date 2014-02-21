<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html" encoding="utf-8" indent="yes"/>

<xsl:include href="utils.xsl" />

<xsl:template match="/">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
  <meta http-equiv="Content-Script-Type" content="text/javascript" />
  <script type="text/javascript" src="utils.js"></script>
  <script type="text/javascript" src="general.js"></script>
  <link rel="stylesheet" type="text/css" href="style.css" />
</head>
<body>
  <xsl:call-template name="page-title">
    <xsl:with-param name="image">general</xsl:with-param>
    <xsl:with-param name="text">General</xsl:with-param>
  </xsl:call-template>

  <div id="general-list">
    <xsl:apply-templates select="general/bind" />
  </div>

  <form enctype="text/plain" action="/admin/general"
        method="post" name="apply"
        onsubmit="
           if ( !ConfirmChanges() )
             return false;
           document.apply.XMLText.value = GeneralMakeXML( document.getElementById('general-list') );
           return true;">
    <input type="hidden" name="XMLText" />
    <input type="submit" value="Apply" class="apply" />
  </form>
</body>
</xsl:template>

<xsl:template match="bind">
  <div name="bind" class="block">
    <xsl:call-template name="button-image-big">
      <xsl:with-param name="image">btn-big-x.gif</xsl:with-param>
      <xsl:with-param name="text">remove bind</xsl:with-param>
      <xsl:with-param name="code">return ItemDelete(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <xsl:call-template name="button-image-big">
      <xsl:with-param name="image">btn-big-add.gif</xsl:with-param>
      <xsl:with-param name="text">new bind</xsl:with-param>
      <xsl:with-param name="code">return ItemAdd(this.parentNode);</xsl:with-param>
    </xsl:call-template>
    <br />

    <span class="field">Interface (IP address):</span>
    <input type="text" name="interface">
      <xsl:attribute name="value">
        <xsl:value-of select="interface" />
      </xsl:attribute>
    </input>
    <br />

    <span class="field">Port:</span>
    <input type="text" name="port">
      <xsl:attribute name="value">
        <xsl:value-of select="port" />
      </xsl:attribute>
    </input>
    <br />

    <label>
      <input type="checkbox" name="shout-comp" onclick="ShoutCompChecked(this);">
        <xsl:if test="shout-mp">
          <xsl:attribute name="checked">1</xsl:attribute>
        </xsl:if>
      </input>
      shout compatible (additional port+1)
    </label>

    <div style="margin-left: 2em;">
      <span class="field">Mount point:</span>
      <input type="text" name="shout-mp">
        <xsl:choose>
            <xsl:when test="shout-mp">
                <xsl:attribute name="class">line</xsl:attribute>
            </xsl:when>
            <xsl:otherwise>
                <xsl:attribute name="disabled">1</xsl:attribute>
                <xsl:attribute name="class">shout-mp-disable</xsl:attribute>
            </xsl:otherwise>
        </xsl:choose>

        <xsl:attribute name="value">
          <xsl:value-of select="shout-mp" />
        </xsl:attribute>
      </input>
    </div>
  </div>
</xsl:template>

</xsl:stylesheet>
