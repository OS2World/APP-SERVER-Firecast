<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html" encoding="utf-8" indent="yes"/>

<xsl:include href="utils.xsl" />

<xsl:template match="/">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
  <meta http-equiv="Content-Script-Type" content="text/javascript" />
  <script type="text/javascript" src="utils.js"></script>
  <link rel="stylesheet" type="text/css" href="style.css" />
</head>
<body>
  <xsl:call-template name="page-title">
    <xsl:with-param name="image"><xsl:choose>
      <xsl:when test="/sources/@admin">sources</xsl:when>
      <xsl:otherwise>sources-info</xsl:otherwise>
    </xsl:choose></xsl:with-param>
    <xsl:with-param name="text">Sources</xsl:with-param>
  </xsl:call-template>

  <xsl:apply-templates select="sources/source" />
</body>
</xsl:template>

<xsl:template match="source">
  <div class="block">
    <div class="source-title">
      <xsl:if test="/sources/@admin">
        <a>
          <xsl:attribute name="name"><xsl:value-of select="@id" /></xsl:attribute>
        </a>
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
<!--
      <xsl:if test="template-id">
        <a>
          <xsl:attribute name="href">/admin/templates#<xsl:value-of select="template-id" /></xsl:attribute>
          [Template]
        </a>
      </xsl:if>
-->
      <xsl:value-of select="mount-point" />
    </div>

    <div>
      <xsl:attribute name="onclick">
        <xsl:choose>
          <xsl:when test="template-id">
            document.location='source?id=<xsl:value-of select="@id" />';
          </xsl:when>
          <xsl:otherwise>
            document.location='source-info?id=<xsl:value-of select="@id" />';
          </xsl:otherwise>
        </xsl:choose>
        return false;
      </xsl:attribute>

      <xsl:apply-templates select="field" />

      <span class="field">Clients:</span> <xsl:value-of select="clients-count" /><br />
      <xsl:if test="meta-history">
        <xsl:apply-templates select="meta-history" />
      </xsl:if>
    </div>
  </div>
</xsl:template>

<xsl:template match="field">
  <xsl:choose>
    <xsl:when test="@name='icy-name'">
      <span class="field">Stream title:</span>
      <xsl:value-of select="."/><br />
    </xsl:when>
    <xsl:when test="@name='icy-url'">
      <span class="field">Stream URL:</span>
      <a>
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

<xsl:template match="meta-history/metadata">
  <span class="field">Current song: <small>[<xsl:value-of select="@time"/>]</small>:</span>
  <i><xsl:value-of select="."/></i>
</xsl:template>

</xsl:stylesheet>
