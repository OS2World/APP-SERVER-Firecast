<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html" encoding="utf-8" indent="yes"/>

<xsl:include href="utils.xsl" />

<xsl:template match="/source">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
  <meta http-equiv="Content-Script-Type" content="text/javascript" />
  <script type="text/javascript" src="utils.js"></script>
  <link rel="stylesheet" type="text/css" href="style.css" />
</head>
<body>
  <a href="sources" 
   style="
     text-decoration: none;
     font-weight: bold; font-size: 10pt;
     color: #401000;
   ">
    <xsl:attribute name="href">
      <xsl:choose>
          <xsl:when test="@admin">
              sources#<xsl:value-of select="@id" />
          </xsl:when>
          <xsl:otherwise>
              sources-info#<xsl:value-of select="@id" />
          </xsl:otherwise>
      </xsl:choose>
    </xsl:attribute>
    <img src="/img/back.gif" height="30" width="33" align="absmiddle"
     style="border: 0; margin-right: 0.3em;" />
    List
  </a>

  <div class="block">
    <div class="source-title">
      <xsl:if test="@admin">
        <xsl:call-template name="button-image-big">
          <xsl:with-param name="image">btn-big-x.gif</xsl:with-param>
          <xsl:with-param name="text">kill source</xsl:with-param>
          <xsl:with-param name="code">
            return StopSource('<xsl:value-of select="mount-point" />',
                              <xsl:value-of select="@id" />);
          </xsl:with-param>
        </xsl:call-template>
      </xsl:if>
      <xsl:call-template name="button-image-big">
        <xsl:with-param name="image">btn-speaker.gif</xsl:with-param>
        <xsl:with-param name="text">listen</xsl:with-param>
        <xsl:with-param name="href"><xsl:value-of select="mount-point" />.m3u</xsl:with-param>
      </xsl:call-template>
      <xsl:call-template name="button-image-big">
        <xsl:with-param name="image">btn-reload.gif</xsl:with-param>
        <xsl:with-param name="text">refresh</xsl:with-param>
        <xsl:with-param name="code">window.location.reload(false); return false;</xsl:with-param>
      </xsl:call-template>
      <xsl:value-of select="mount-point" />
    </div>

    <xsl:if test="source-address">
      <span class="field">From:</span>
      <xsl:choose>
        <xsl:when test="source-address='0.0.0.0'">
          Relay
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="source-address" />
        </xsl:otherwise>
      </xsl:choose>
    </xsl:if>
    <br />

    <xsl:apply-templates select="field" />
    <span class="field">Clients:</span> <xsl:value-of select="clients-count" /><br />
    <xsl:if test="meta-history">
      <xsl:apply-templates select="meta-history" />
    </xsl:if>
    <xsl:if test="clients">
      <xsl:apply-templates select="clients" />
    </xsl:if>
  </div>
</body>
</xsl:template>


<xsl:template match="field">
  <xsl:choose>
    <xsl:when test="@name='icy-name'">
      <span class="field">Stream title:</span>
      <xsl:value-of select="."/><br />
    </xsl:when>
    <xsl:when test="@name='icy-url'">
      <span class="field">Stream URL:</span>
      <a onclick="return !window.open(this.href)">
        <xsl:attribute name="href"><xsl:value-of select="."/></xsl:attribute>
        <xsl:value-of select="."/>
      </a><br />
    </xsl:when>
    <xsl:when test="@name='icy-br'">
      <span class="field">Bitrate:</span>
      <xsl:value-of select="."/> Kb/sec<br />
    </xsl:when>
    <xsl:when test="@name='icy-genre'">
      <span class="field">Music style:</span>
      <xsl:value-of select="."/><br />
    </xsl:when>
    <xsl:when test="@name='icy-description'">
      <span class="field">Description:</span>
      <xsl:value-of select="."/><br />
    </xsl:when>
    <xsl:when test="@name='icy-pub'">
      <xsl:if test=".='1'">
        <span class="field">Public:</span> YES<br />
      </xsl:if>
    </xsl:when>
    <xsl:otherwise>
      <span class="field">[<xsl:value-of select="@name"/>]:</span>
      <xsl:value-of select="."/><br />
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="meta-history">
  <h2>Meta History</h2>
  <xsl:apply-templates select="metadata" />
</xsl:template>

<xsl:template match="metadata">
  <span class="field"><small>[<xsl:value-of select="@time"/>]</small>:</span>
  <i><xsl:value-of select="."/></i><br />
</xsl:template>

<xsl:template match="clients">
  <h2>Clients</h2>
  <xsl:apply-templates select="client" />
</xsl:template>

<xsl:template match="client">
  <xsl:if test="/source/@admin">
    <xsl:call-template name="button-image-small">
      <xsl:with-param name="image">btn-x.gif</xsl:with-param>
      <xsl:with-param name="text">disconnect</xsl:with-param>
      <xsl:with-param name="code">return StopClient('<xsl:value-of select="addr"/>',<xsl:value-of select="/source/@id" />,<xsl:value-of select="@id" />);</xsl:with-param>
    </xsl:call-template>
  </xsl:if>
  [<xsl:value-of select="addr"/>]: 
  <xsl:value-of select="user-agent"/><br />
</xsl:template>

</xsl:stylesheet>
