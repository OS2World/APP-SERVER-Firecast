<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html" encoding="utf-8" indent="yes"/>

<xsl:include href="utils.xsl" />

<xsl:template match="/">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
  <meta http-equiv="Content-Script-Type" content="text/javascript" />
  <script type="text/javascript" src="utils.js"></script>
  <script type="text/javascript" src="relays.js"></script>
  <link rel="stylesheet" type="text/css" href="style.css" />
</head>
<body>
  <xsl:call-template name="page-title">
    <xsl:with-param name="image">relays</xsl:with-param>
    <xsl:with-param name="text">Relays</xsl:with-param>
  </xsl:call-template>

  <div id="relays-list">
    <xsl:apply-templates select="relays/relay" />
  </div>

  <center>
    <div id="new-relay" style="margin-top: 3em;">
      <table border="0" cellspacing="0" cellpadding="0">
        <tr>
          <td>
            <label for="mount-point">
              Mount point:
            </label>
          </td>
          <td>
            <input type="text" name="mount-point" id="mount-point" size="30" maxlength="255" />
          </td>
        </tr>
        <tr>
          <td>
            <label for="location">
              Source location:
            </label>
          </td>
          <td>
            <input type="text" name="location" id="location" size="30" maxlength="255" value="http://" />
          </td>
        </tr>
        <tr>
          <td colspan="2">
            <label>
              <input type="checkbox" name="nailed-up" />
              Nailed-up
            </label>
          </td>
        </tr>
      </table>
    </div>

    <form enctype="text/plain" action="/admin/relays"
          method="post" name="add"
          onsubmit="
             document.add.XMLText.value = RelayMakeXML( document.getElementById('new-relay') );
             return true;">
      <input type="hidden" name="XMLText" />
      <input type="submit" value="Add" />
    </form>
  </center>
</body>
</xsl:template>

<xsl:template match="relay">
  <div class="block">
    <div class="source-title">
      <xsl:call-template name="button-image-big">
        <xsl:with-param name="image">btn-big-x.gif</xsl:with-param>
        <xsl:with-param name="text">remove relay</xsl:with-param>
        <xsl:with-param name="code">
          return RemoveRelay('<xsl:value-of select="location" />',
                              <xsl:value-of select="@id" />);
        </xsl:with-param>
      </xsl:call-template>
      <span class="field">Local:</span> <xsl:value-of select="mount-point" />
      <span class="field"> Source:</span> <xsl:value-of select="location" />
    </div>
    <span class="field">Nailed-up:</span>
    <xsl:choose>
      <xsl:when test="nailed-up='1'">YES</xsl:when>
      <xsl:otherwise>NO</xsl:otherwise>
    </xsl:choose>
    <br />
    <span class="field">State:</span>
    <span>
      <xsl:if test="state/@error='1'">
        <xsl:attribute name="class">relay-state-error</xsl:attribute>
      </xsl:if>
      <xsl:value-of select="state" />
    </span>
    <xsl:if test="state/@error='1'">
      <a><xsl:attribute name="href">?id=<xsl:value-of select="@id" /></xsl:attribute>[reconnect]</a>
    </xsl:if>
  </div>
</xsl:template>

</xsl:stylesheet>
